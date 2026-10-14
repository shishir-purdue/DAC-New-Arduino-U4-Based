"""Generate independently runnable offline physical-control tests."""
import json
from pathlib import Path
from test_buttons import CASES

cells = []
def md(text):
    cells.append(dict(cell_type="markdown", metadata={}, source=text.splitlines(True)))
def code(text):
    cells.append(dict(cell_type="code", metadata={}, source=text.splitlines(True), execution_count=None, outputs=[]))

md("""# Physical recovery buttons

These tests compile and run the current firmware/driver on the computer. They do
not connect to the instrument or change outputs. Run the setup cell, then any
individual test. A failing assertion displays its error in that cell.

B cases use the application and real DAC driver with hardware doubles. N cases
use the actual local Ethernet socket/chip code with register/SPI doubles. N1-N10
each exercise W5100 and W5500; N11-N12 exercise W5500 PHY recovery/readback failure.
Neither proves analog voltage, electrical SPI behavior, or physical-chip recovery.
The manual bench procedure is in `Software/Physical controls.md`.
""")
code("""from pathlib import Path
import sys
ROOT = next((p for p in [Path.cwd(), *Path.cwd().parents]
             if (p / 'Software/src/main.cpp').is_file()), None)
assert ROOT is not None, 'Set ROOT to the DAC Box repository'
sys.path.insert(0, str(ROOT / 'tests'))
from checklist_tests import run_host
print('Firmware source:', ROOT)
""")
for case, description in CASES.items():
    md(f"## {case}: {description}\n")
    code(f"run_host('{case}')\nprint('PASS {case}')\n")

notebook = dict(cells=cells, metadata={
    "kernelspec": {"display_name": "Python 3", "language": "python", "name": "python3"},
    "language_info": {"name": "python"}}, nbformat=4, nbformat_minor=5)
for i, cell in enumerate(cells):
    cell['id'] = f'physical-controls-{i}'
Path(__file__).with_name('physical_controls.ipynb').write_text(json.dumps(notebook, indent=1) + '\n', encoding='utf-8')
