"""Independent analytical checks and lifecycle bounds for local G1 acquisition."""
from copy import deepcopy
import io
import json
import math
import os
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from local_plant import Plant
from telemetry_publisher import Publisher, strict_json


def fixture():
    network = {"assets":[{"id":"storage","kind":"tank","initial_m3":80,"capacity_m3":200},
                         {"id":"well","kind":"source","inflow_m3h":36,"curtailment_m3h":0},
                         {"id":"destination","kind":"treatment","capacity_m3h":60}],
               "routes":[{"id":"feed","source_id":"well","target_id":"storage","setpoint_m3h":36,"has_pump":False,"available":True},
                         {"id":"delivery","source_id":"storage","target_id":"destination","setpoint_m3h":12,"has_pump":True,"available":True}]}
    configuration = {"physical_hash":"a"*64,"policy":{"heartbeat_period_s":1},"sensors":[]}
    for identity,quantity,unit,attachment in [("volume","inventory","m3","storage"),("source","source_flow","m3/h","well"),
                                              ("flow","route_flow","m3/h","delivery"),("feedback","pump_state","state","delivery")]:
        configuration["sensors"].append(dict(sensor_id=identity,quantity=quantity,unit=unit,attachment={"id":attachment},
                                              enabled=True,expected_period_s=1,uncertainty_bound=0.01))
    return network,configuration


class PlantTests(unittest.TestCase):
    def test_analytical_balance_and_fault_boundary_splitting(self):
        for preset,expected in [("healthy",80.8),("pump_loss",81.0),("unmetered_loss",80.5)]:
            network,configuration=fixture()
            plant=Plant(network,configuration,preset=preset,onset_s=30,recovery_s=90)
            plant.advance(120)
            self.assertAlmostEqual(plant.volumes["storage"],expected,places=11)
            incremental=Plant(network,configuration,preset=preset,onset_s=30,recovery_s=90)
            for t in range(1,121): incremental.advance(t)
            self.assertAlmostEqual(incremental.volumes["storage"],expected,places=11)
            self.assertEqual(network,fixture()[0])

    def test_seeded_noise_is_bounded_and_repeatable(self):
        network,configuration=fixture()
        first,second=Plant(network,configuration,seed=9),Plant(network,configuration,seed=9)
        for t in range(100):
            first.advance(t); second.advance(t)
            a,b=first.sample(),second.sample()
            self.assertEqual(a,b)
            self.assertLessEqual(abs(a[0]["value"]-first.volumes["storage"]),0.010000000001)
            self.assertLessEqual(abs(a[2]["value"]-12),0.010000000001)

    def test_zero_delivery_feedback_and_recovery_preserve_commands(self):
        network,configuration=fixture()
        plant=Plant(network,configuration,preset="pump_loss",onset_s=1,recovery_s=3)
        plant.advance(2); rows={r["sensor_id"]:r for r in plant.sample()}
        self.assertEqual(rows["flow"]["value"],0)
        self.assertEqual(rows["feedback"]["value"],"delivery_loss")
        self.assertEqual(plant.network["routes"][1]["setpoint_m3h"],12)
        plant.advance(3)
        self.assertEqual(plant.sample()[-1]["value"],"running")

    def test_stuck_drift_missing_and_out_of_range_evidence(self):
        network,configuration=fixture()
        stuck=Plant(network,configuration,preset="stuck",onset_s=1,recovery_s=100)
        stuck.advance(2); value=stuck.sample()[0]["value"]
        stuck.advance(20); self.assertEqual(stuck.sample()[0]["value"],value)
        missing=Plant(network,configuration,preset="missing",onset_s=1,recovery_s=100)
        missing.advance(2); self.assertNotIn("volume",[r["sensor_id"] for r in missing.sample()])
        drift=Plant(network,configuration,preset="drift",onset_s=1,recovery_s=100)
        drift.advance(80); self.assertGreater(drift.sample()[0]["value"]-drift.volumes["storage"],0.1)
        network["assets"][0]["initial_m3"]=-4
        raw=Plant(network,configuration); self.assertLess(raw.sample()[0]["value"],0)

    def test_renamed_topology_and_level_inverse(self):
        network,configuration=fixture()
        for asset in network["assets"]: asset["id"]="north:"+asset["id"]
        for route in network["routes"]:
            route["id"]="north:"+route["id"]
            route["source_id"]="north:"+route["source_id"]; route["target_id"]="north:"+route["target_id"]
        for sensor in configuration["sensors"]: sensor["attachment"]["id"]="north:"+sensor["attachment"]["id"]
        plant=Plant(network,configuration); plant.advance(120); self.assertAlmostEqual(plant.volumes["north:storage"],80.8)
        sensor={"calibration":{"knots":[{"level_m":0,"volume_m3":0},{"level_m":1,"volume_m3":30},{"level_m":2,"volume_m3":80}]}}
        self.assertAlmostEqual(Plant.level_for_volume(sensor,50),1.4)
        self.assertLess(Plant.level_for_volume(sensor,-5),0)

    def test_resource_and_nonfinite_inputs(self):
        network,configuration=fixture()
        for options in ({"seed":True},{"seed":-1},{"preset":"unknown"},{"onset_s":math.nan},{"onset_s":4,"recovery_s":3}):
            with self.assertRaises(ValueError): Plant(network,configuration,**options)
        fast=deepcopy(configuration)
        for sensor in fast["sensors"]: sensor["expected_period_s"]=0.001
        with self.assertRaises(ValueError): Plant(network,fast)
        plant=Plant(network,configuration)
        with self.assertRaises(ValueError): plant.advance(math.inf)
        plant.advance(1)
        with self.assertRaises(ValueError): plant.advance(0)


class PublisherTests(unittest.TestCase):
    def setUp(self):
        network,configuration=fixture()
        self.output=io.StringIO(); self.now=0.0
        self.publisher=Publisher(configuration,{"network":network,"physical_hash":"a"*64},"b"*64,"a"*64,
                                 seed=3,preset="pump_loss",onset_s=2,recovery_s=8,output=self.output,monotonic=lambda:self.now)

    def command(self,operation,payload=None,identity=None):
        return dict(telemetry_version=1,kind="stream_command",request_id=identity or "request-"+operation,
                    stream_id=self.publisher.stream_id,epoch_id=self.publisher.epoch_id,operation=operation,payload=payload or {})

    def frames(self): return [strict_json(line) for line in self.output.getvalue().splitlines()]

    def test_real_lifecycle_matching_acks_and_new_epoch(self):
        self.now=1.1; self.publisher.tick()
        self.publisher.command(self.command("pause")); paused=self.publisher.plant.time_s
        self.now=4; self.publisher.tick(); self.assertEqual(self.publisher.plant.time_s,paused)
        self.publisher.command(self.command("set_rate",{"clock_rate":2}))
        self.publisher.command(self.command("resume")); self.now=5; self.publisher.tick()
        self.assertAlmostEqual(self.publisher.plant.time_s,paused+2)
        epoch=self.publisher.epoch_id
        self.publisher.command(self.command("restart"))
        self.assertNotEqual(epoch,self.publisher.epoch_id); self.assertEqual(self.publisher.plant.time_s,0)
        self.publisher.command(self.command("stop"))
        self.assertTrue(self.publisher.stopped)
        frames=self.frames(); self.assertEqual(frames[-1]["type"],"end")
        self.assertEqual(frames[-2]["payload"]["request_id"],"request-stop")
        self.assertNotIn("faults",self.output.getvalue()); self.assertNotIn("seed",self.output.getvalue())
        self.assertTrue(all(f["configuration_hash"]=="b"*64 for f in frames))

    def test_duplicate_commands_are_idempotent_and_conflicts_fault(self):
        command=self.command("pause"); self.publisher.command(command); original=self.frames()[-1]
        self.publisher.command(command); self.assertEqual(self.frames()[-1],original)
        changed=dict(command,operation="resume")
        with self.assertRaises(ValueError): self.publisher.command(changed)

    def test_unsupported_commands_cannot_change_plant_controls(self):
        for command in [self.command("apply_controls"),self.command("set_rate",{"clock_rate":math.inf}),
                        self.command("stop",{"plan":{}}),dict(self.command("pause"),epoch_id="old"),
                        dict(self.command("pause"),telemetry_version=True)]:
            with self.assertRaises(ValueError): self.publisher.command(command)
        self.assertEqual(self.publisher.plant.network["routes"][1]["setpoint_m3h"],12)
        for raw in ('{"x":1,"x":2}','{"x":NaN}','{"x":Infinity}','{"x":1e999}','["\\ud800"]','['*65+'0'+']'*65):
            with self.assertRaises(ValueError): strict_json(raw)

    def test_real_process_exits_cleanly_after_stop_with_parent_stdin_still_open(self):
        network,configuration=fixture()
        with tempfile.TemporaryDirectory(prefix="local-publisher-stop-") as folder:
            root=Path(folder)
            (root/"configuration.json").write_text(json.dumps(configuration),encoding="utf-8")
            (root/"context.json").write_text(json.dumps({"network":network,"physical_hash":"a"*64}),encoding="utf-8")
            environment={k:os.environ[k] for k in ("SystemRoot","WINDIR","PATH","TEMP","TMP") if k in os.environ}
            environment.update(BREAKWATER_TELEMETRY_CONFIGURATION_FILE=str(root/"configuration.json"),
                BREAKWATER_TELEMETRY_CONTEXT_FILE=str(root/"context.json"),BREAKWATER_TELEMETRY_CONFIGURATION_HASH="b"*64,
                BREAKWATER_TELEMETRY_PHYSICAL_HASH="a"*64,PYTHONDONTWRITEBYTECODE="1",PYTHONNOUSERSITE="1",PYTHONUNBUFFERED="1")
            process=subprocess.Popen([sys.executable,"-B",str(Path(__file__).resolve().parents[1]/"telemetry_publisher.py")],
                stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=environment)
            try:
                first=strict_json(process.stdout.readline())
                stop=dict(telemetry_version=1,kind="stream_command",request_id="stop-actual-process",stream_id=first["stream_id"],
                          epoch_id=first["epoch_id"],operation="stop",payload={})
                process.stdin.write((json.dumps(stop)+"\n").encode("utf-8")); process.stdin.flush()
                self.assertEqual(process.wait(timeout=5),0)
                self.assertFalse(process.stdin.closed)
                self.assertEqual(process.stderr.read(),b"")
            finally:
                if process.poll() is None: process.kill(); process.wait(timeout=5)
                process.stdin.close(); process.stdout.close(); process.stderr.close()


if __name__=="__main__": unittest.main()
