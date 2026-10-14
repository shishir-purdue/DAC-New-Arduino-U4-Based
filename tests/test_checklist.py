"""Run every checklist case with stdlib unittest (also discoverable by pytest)."""
import unittest
from checklist_tests import DESCRIPTIONS, run_case


class ChecklistTests(unittest.TestCase):
    pass


for case_id, description in DESCRIPTIONS.items():
    def test(self, case_id=case_id):
        run_case(case_id)
    test.__doc__ = description
    setattr(ChecklistTests, f"test_{case_id.replace('-', '_')}", test)


if __name__ == "__main__":
    unittest.main()
