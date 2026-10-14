"""Optional pytest interface: open bugs fail, bench reviews are skips."""
import pytest
from checklist_tests import DESCRIPTIONS, run_case


@pytest.mark.parametrize("case_id", list(DESCRIPTIONS))
def test_checklist(case_id):
    run_case(case_id)
