"""Optional runtime stays explicit, credential-free and pinned independently."""
from pathlib import Path
import re
import unittest
import yaml

ROOT = Path(__file__).resolve().parents[3]


class OptionalScienceWorkflow(unittest.TestCase):
    def test_manual_only_readonly_pinned_and_retains_failed_setup(self):
        workflow = yaml.safe_load((ROOT / ".github/workflows/optional-science.yml").read_text(encoding="utf-8"))
        self.assertEqual(set(workflow["on"]), {"workflow_dispatch"})
        self.assertEqual(workflow["permissions"], {"contents": "read"})
        self.assertEqual(workflow["env"]["NVIDIA_API_KEY"], "")
        job = workflow["jobs"]["calibration"]
        self.assertEqual(job["runs-on"], "windows-2022")
        for step in job["steps"]:
            if "uses" in step:
                self.assertTrue(re.fullmatch(r"actions/[a-z-]+@[0-9a-f]{40}", step["uses"]))
        checkout = next(s for s in job["steps"] if s.get("uses", "").startswith("actions/checkout@"))
        self.assertIs(checkout["with"]["persist-credentials"], False)
        upload = next(s for s in job["steps"] if s.get("uses", "").startswith("actions/upload-artifact@"))
        self.assertEqual(upload["if"], "always()")
        self.assertEqual(upload["with"]["if-no-files-found"], "error")
        self.assertNotIn(".tools/science-physicsnemo", upload["with"]["path"])


if __name__ == "__main__":
    unittest.main()
