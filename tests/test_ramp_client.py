"""Offline client protocol/units tests; never access the network."""
from pathlib import Path
import sys
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Software/python"))
from dac_client import ramp, ramp_parameters


class RampClientTests(unittest.TestCase):
    def test_units_and_voltage_boundaries(self):
        self.assertEqual(ramp_parameters(1, 1, .001), (1, 144179, 10, 762940))
        self.assertEqual(ramp_parameters(1, 1, .01), (1, 144179, 10, 76294))
        self.assertEqual(ramp_parameters(1, 0, 1)[1], 131072)
        self.assertEqual(ramp_parameters(1, -10, 1)[1], 0)
        self.assertEqual(ramp_parameters(1, 10, 1)[1], 262143)

    def test_invalid_inputs_never_send(self):
        dac = Mock()
        for args in [(0,0,1), (5,0,1), (1,11,1), (1,float('nan'),1),
                     (1,0,0), (1,0,-1), (1,0,float('inf')), (1,0,1e-320)]:
            with self.assertRaises((ValueError, OverflowError)):
                ramp(dac,*args)
        dac.query.assert_not_called()

    def test_ack_status_and_no_completion_line_to_drain(self):
        dac=Mock()
        dac.query.side_effect=['RAMP ACCEPTED 1 144179 10 76294',
                               'RAMP RUNNING 1 144179', 'RAMP COMPLETE 1 144179']
        with patch('dac_client.time.sleep'):
            self.assertEqual(ramp(dac,1,1,.01), 'RAMP COMPLETE 1 144179')
        self.assertEqual([c.args[0] for c in dac.query.call_args_list],
                         ['RAMP 1 144179 10 76294', 'STATUS', 'STATUS'])

    def test_failure_and_abort_surface_as_errors(self):
        for reply in ['ERR! DAC readback invalid', 'RAMP command received with parameters: 1 144179 10 76294']:
            with self.assertRaises(RuntimeError):
                ramp(Mock(query=Mock(return_value=reply)),1,1,.01)
        for status in ['RAMP ABORTED 1 144179','RAMP FAILED 1 144179','RAMP RUNNING 2 144179']:
            dac=Mock(query=Mock(side_effect=['RAMP ACCEPTED 1 144179 10 76294',status]))
            with self.assertRaises(RuntimeError): ramp(dac,1,1,.01)

    def test_nonwaiting_ack_and_bounded_timeout(self):
        dac=Mock(query=Mock(return_value='RAMP ACCEPTED 1 144179 10 76294'))
        self.assertIn('ACCEPTED',ramp(dac,1,1,.01,wait=False))
        dac.query.assert_called_once()
        dac.reset_mock()
        with patch('dac_client.time.monotonic',side_effect=[0,2]):
            with self.assertRaises(TimeoutError): ramp(dac,1,1,.01,timeout=1)
        dac.query.assert_called_once()  # No extra output/disconnect on timeout.


if __name__ == '__main__':
    unittest.main()
