"""A9 harness regressions using independent physical evidence and corruptions.

The qualification CLI separately executes the real native matrix. These tests
exercise the checker with an exactly integrable source-only pipe, so malformed or
internally consistent but physically wrong evidence cannot acquire a passing gate.
"""
from __future__ import annotations

import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from qualify_transport import evaluate, evaluate_case, validate_inputs, validate_matrix

SCIENCE = Path(__file__).resolve().parents[1]


def source_case():
    """Uniform source: dc/dt=.25 mg/L/h, c(0)=.1, 200 L total volume."""
    cells = 8
    return {
        "id": "independent-source-only",
        "regime": "source_only",
        "study": "exact",
        "pipe": {"length_m": 1., "area_m2": .2, "dispersion_m2_h": 0.,
                 "decay_per_h": 0., "cells": cells},
        "config": {"horizon_h": 1., "output_step_h": 1., "cfl_safety": .5,
                   "boundary": "closed_no_flux", "max_substeps": 1000000},
        "flow": [{"time_h": 0., "flow_m3h": 0.}],
        "inlet": [],
        "initial_mg_l": [.1] * cells,
        "source_mg_l_h": [.25] * cells,
        "reference_parameters": {"initial_mg_l": .1, "source_mg_l_h": .25},
        "result": {
            "solver_version": "independent-test-evidence",
            "x_m": [(index + .5) / cells for index in range(cells)],
            "time_h": [0., 1.],
            "concentration_mg_l": [[.1] * cells, [.35] * cells],
            "inventory_mg": [20., 70.],
            "cumulative_inlet_mg": [0., 0.],
            "cumulative_outlet_mg": [0., 0.],
            "cumulative_source_mg": [0., 50.],
            "cumulative_decay_mg": [0., 0.],
            "mass_balance_residual_mg": [0., 0.],
            "diagnostics": {"substeps": 1, "max_cfl_number": 0.,
                            "minimum_concentration_mg_l": .1,
                            "max_mass_balance_residual_mg": 0.},
        },
    }


def matrix_metadata(config):
    """Expand the declared matrix as metadata; no simulated fields are invented."""
    cases = []
    matrix = config["matrix"]

    def append(regime, study, cells, cfl, mode=None):
        cases.append({
            "id": f"metadata-{len(cases)}",
            "regime": regime, "study": study,
            "reference_parameters": {} if mode is None else {"mode_fraction": mode},
            "pipe": {"cells": cells}, "config": {"cfl_safety": cfl},
        })

    for regime, spec in matrix["regimes"].items():
        if regime in config["spatial_gates"]:
            for mode in spec.get("mode_fractions", [None]):
                for cells in spec.get("space_cells", matrix["space_cells"]):
                    append(regime, "space", cells, spec["space_cfl"], mode)
        if regime in config["temporal_gates"]:
            for cfl in spec.get("time_cfl", matrix["time_cfl"]):
                append(regime, "time", spec.get("cells", matrix["time_cells"]), cfl,
                       spec.get("time_mode_fraction"))
        if regime == "source_only":
            append(regime, "exact", spec["cells"], spec["cfl"])
    return {
        "fixture_version": 1, "solver_version": "independent-test-evidence",
        "qualification_variant": config.get("qualification_variant", "baseline"),
        "cases": cases,
        "rejections": [{"id": name, "exception_type": kind,
                        "message": "Explicit bounded request rejection"}
                       for name, kind in config["expected_rejections"].items()],
    }


class QualificationEvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = json.loads((SCIENCE / "a9_transport_config.json").read_text())

    def failed_gates(self, case):
        row = evaluate_case(case, self.config)
        return {gate["name"] for gate in row["gates"] if not gate["passed"]}

    def test_exact_uniform_source_has_correct_units_and_passes(self):
        case = source_case()
        validate_inputs(case, self.config)
        row = evaluate_case(case, self.config)
        self.assertEqual(self.failed_gates(case), set())
        self.assertEqual(row["terminal_continuum_error"]["maximum_mg_l"], 0.)
        self.assertEqual(row["analytic_ledger_errors_mg"]["cumulative_source_mg"], 0.)
        self.assertEqual(row["maximum_signed_mass_residual_mg"], 0.)

    def test_consistent_missing_litre_conversion_cannot_pass_balance(self):
        case = source_case()
        # A counterfeit ledger scaled by 1/1000 balances internally, but the
        # actual field occupies 200 L, not .2 L. Recompute inventory independently.
        case["result"]["inventory_mg"] = [.02, .07]
        case["result"]["cumulative_source_mg"] = [0., .05]
        failed = self.failed_gates(case)
        self.assertIn("independent_inventory", failed)
        self.assertIn("independent_signed_balance", failed)

    def test_wrong_source_ledger_sign_cannot_pass_recorded_zero_residual(self):
        case = source_case()
        case["result"]["cumulative_source_mg"][-1] = -50.
        failed = self.failed_gates(case)
        self.assertIn("independent_signed_balance", failed)
        self.assertIn("recorded_balance", failed)

    def test_conserving_but_wrong_field_fails_the_independent_solution(self):
        case = source_case()
        # All invented output terms agree with each other. The captured physical
        # source remains .25, so c(1)=.35 and M(1)=70 are fixed independently.
        case["result"]["concentration_mg_l"][-1] = [.4] * 8
        case["result"]["inventory_mg"][-1] = 80.
        case["result"]["cumulative_source_mg"][-1] = 60.
        failed = self.failed_gates(case)
        self.assertNotIn("independent_inventory", failed)
        self.assertNotIn("independent_signed_balance", failed)
        self.assertIn("source_only_exact", failed)

    def test_false_diagnostics_and_positive_excess_work_fail_gates(self):
        mutations = [
            ("declared_cfl", "max_cfl_number", .6),
            ("diagnostic_minimum", "minimum_concentration_mg_l", -.01),
            ("diagnostic_minimum", "minimum_concentration_mg_l", .2),
            ("substep_balance", "max_mass_balance_residual_mg", 1.),
            ("bounded_work", "substeps", 1000001),
        ]
        for gate, key, value in mutations:
            with self.subTest(key=key, value=value):
                case = source_case()
                case["result"]["diagnostics"][key] = value
                self.assertIn(gate, self.failed_gates(case))

    def test_zero_step_diagnostic_is_explicitly_malformed(self):
        case = source_case()
        case["result"]["diagnostics"]["substeps"] = 0
        with self.assertRaises(ValueError):
            evaluate_case(case, self.config)

    def test_nonfinite_negative_and_untyped_diagnostics_are_rejected(self):
        for key, value in (("substeps", -1), ("substeps", 1.5), ("substeps", True),
                           ("max_cfl_number", -1.), ("max_cfl_number", float("nan")),
                           ("max_mass_balance_residual_mg", -1.),
                           ("minimum_concentration_mg_l", float("inf"))):
            with self.subTest(key=key, value=value):
                case = source_case()
                case["result"]["diagnostics"][key] = value
                with self.assertRaises(ValueError):
                    evaluate_case(case, self.config)

    def test_captured_inputs_cannot_be_changed_to_fit_the_result(self):
        mutations = [
            lambda case: case["pipe"].update(area_m2=.001),
            lambda case: case["config"].update(horizon_h=.9),
            lambda case: case["config"].update(output_step_h=.5),
            lambda case: case["config"].update(boundary="dirichlet_inlet"),
            lambda case: case["config"].update(max_substeps=10),
            lambda case: case["source_mg_l_h"].__setitem__(0, .3),
            lambda case: case["flow"][0].update(flow_m3h=.1),
            lambda case: case["flow"][0].update(time_h=.1),
            lambda case: case["inlet"].append({"time_h": 0., "concentration_mg_l": 1.}),
            lambda case: case["reference_parameters"].update(initial_mg_l=.2),
        ]
        for mutation in mutations:
            case = source_case()
            mutation(case)
            with self.subTest(case=case), self.assertRaises(ValueError):
                validate_inputs(case, self.config)

    def test_initial_metadata_and_result_must_both_match_cell_average_oracle(self):
        for alter_captured in (False, True):
            with self.subTest(alter_captured=alter_captured):
                case = source_case()
                case["result"]["concentration_mg_l"][0][0] = .2
                if alter_captured:
                    case["initial_mg_l"][0] = .2
                with self.assertRaises(ValueError):
                    evaluate_case(case, self.config)

    def test_result_shapes_nonfinite_values_and_endpoint_are_rejected(self):
        mutations = [
            lambda case: case["result"].update(time_h=[0., .9]),
            lambda case: case["result"].update(time_h=[0., 0.]),
            lambda case: case["result"]["concentration_mg_l"][-1].pop(),
            lambda case: case["result"]["concentration_mg_l"][-1].__setitem__(0, float("nan")),
            lambda case: case["result"].update(inventory_mg=[20.]),
            lambda case: case["result"]["inventory_mg"].__setitem__(-1, float("inf")),
            lambda case: case["result"]["x_m"].__setitem__(0, .01),
        ]
        for index, mutation in enumerate(mutations):
            with self.subTest(mutation=index):
                case = source_case()
                mutation(case)
                with self.assertRaises(ValueError):
                    evaluate_case(case, self.config)

    def test_scheduled_result_cannot_omit_intermediate_requested_outputs(self):
        case = source_case()
        case.update(id="missing-scheduled-outputs", regime="scheduled_inlet", study="space",
                    initial_mg_l=[0.] * 8, source_mg_l_h=[0.] * 8,
                    reference_parameters={"initial_mg_l": 0.},
                    flow=[{"time_h": 0., "flow_m3h": .08},
                          {"time_h": .137, "flow_m3h": 0.},
                          {"time_h": .293, "flow_m3h": .14}],
                    inlet=[{"time_h": time, "concentration_mg_l": value}
                           for time, value in ((0., 0.), (.11, 1.), (.29, .2),
                                               (.47, .8), (.63, 0.))])
        case["config"].update(horizon_h=.6, output_step_h=.05,
                              cfl_safety=.1, boundary="dirichlet_inlet")
        case["result"]["time_h"] = [0., .6]
        # The endpoints alone are insufficient for a scheduled study. This must
        # fail on the grid before examining the omitted field/ledger samples.
        validate_inputs(case, self.config)
        with self.assertRaisesRegex(ValueError, "omitted or shifted a requested output"):
            evaluate_case(case, self.config)


class QualificationMatrixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = json.loads((SCIENCE / "a9_transport_config.json").read_text())

    def test_declared_matrix_metadata_is_accepted(self):
        document = matrix_metadata(self.config)
        self.assertEqual(len(document["cases"]), 45)
        validate_matrix(document, self.config)

    def test_refined_followup_keeps_every_original_acceptance_gate(self):
        refined = json.loads((SCIENCE / "a9_transport_refined_config.json").read_text())
        for section in ("common_gates", "spatial_gates", "temporal_gates", "expected_rejections"):
            self.assertEqual(refined[section], self.config[section], section)
        self.assertEqual(refined["qualification_variant"], "refined_inlet")
        self.assertEqual(refined["native_arguments"], ["--refined-inlet"])
        self.assertEqual(refined["notes"][:len(self.config["notes"])], self.config["notes"])
        # No other regime, forcing, CFL, global mesh, or fixture count changes.
        without_annotations = copy.deepcopy(refined)
        for name in ("declaration", "notes", "qualification_variant", "native_arguments"):
            without_annotations.pop(name, None)
        expected = copy.deepcopy(self.config)
        for name in ("declaration", "notes", "qualification_variant", "native_arguments"):
            expected.pop(name, None)
        self.assertEqual(without_annotations["matrix"]["regimes"]["scheduled_inlet"].pop("space_cells"),
                         [128, 256, 512])
        self.assertEqual(without_annotations, expected)

    def test_refined_matrix_accepts_only_its_tag_and_refined_inlet_meshes(self):
        refined = json.loads((SCIENCE / "a9_transport_refined_config.json").read_text())
        document = matrix_metadata(refined)
        self.assertEqual(sorted(case["pipe"]["cells"] for case in document["cases"]
                                if case["regime"] == "scheduled_inlet"), [128, 256, 512])
        validate_matrix(document, refined)
        wrong_tag = copy.deepcopy(document)
        wrong_tag["qualification_variant"] = "baseline"
        with self.assertRaisesRegex(ValueError, "variant"):
            validate_matrix(wrong_tag, refined)
        with self.assertRaisesRegex(ValueError, "variant"):
            validate_matrix(document, self.config)
        baseline_cells = matrix_metadata(self.config)
        baseline_cells["qualification_variant"] = refined["qualification_variant"]
        with self.assertRaisesRegex(ValueError, "declared qualification matrix"):
            validate_matrix(baseline_cells, refined)

    def test_empty_missing_duplicate_and_changed_matrix_are_rejected(self):
        mutations = [
            lambda document: document.update(cases=[]),
            lambda document: document["cases"].pop(),
            lambda document: document.update(fixture_version=2),
            lambda document: document["cases"][0].update(id=document["cases"][1]["id"]),
            lambda document: document["cases"][0].update(regime="unqualified-regime"),
            lambda document: document["cases"][0]["pipe"].update(cells=33),
            lambda document: document["cases"][0]["config"].update(cfl_safety=.9),
            lambda document: document["cases"][0].update(
                **{key: copy.deepcopy(document["cases"][1][key])
                   for key in ("regime", "study", "reference_parameters", "pipe", "config")}),
        ]
        for index, mutation in enumerate(mutations):
            with self.subTest(mutation=index):
                document = matrix_metadata(self.config)
                mutation(document)
                with self.assertRaises(ValueError):
                    validate_matrix(document, self.config)

    def test_required_rejections_need_the_declared_type_and_retained_error(self):
        mutations = [
            lambda document: document.update(rejections=[]),
            lambda document: document["rejections"].append(copy.deepcopy(document["rejections"][0])),
            lambda document: document["rejections"][0].update(exception_type="wrong_type"),
            lambda document: document["rejections"][0].update(message=""),
        ]
        for index, mutation in enumerate(mutations):
            with self.subTest(mutation=index):
                document = matrix_metadata(self.config)
                mutation(document)
                with self.assertRaises(ValueError):
                    validate_matrix(document, self.config)

    def test_evaluation_rejects_result_method_identity_mismatch(self):
        document = matrix_metadata(self.config)
        document["cases"][0]["result"] = {"solver_version": "another-method"}
        with self.assertRaisesRegex(ValueError, "method identities"):
            evaluate(document, self.config)


if __name__ == "__main__":
    unittest.main()
