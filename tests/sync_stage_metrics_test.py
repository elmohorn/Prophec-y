import sys
from pathlib import Path
import unittest
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from analyze_sync_stages import power_harmonics, SR


class SyncPowerTests(unittest.TestCase):
    def test_antiphase_partial_is_not_erased(self):
        t = np.arange(SR)/SR
        fundamental = np.sin(2*np.pi*88*t)
        four = 2*np.sin(2*np.pi*352*t)
        five = np.sin(2*np.pi*440*t)
        x = np.column_stack([fundamental+four+five, fundamental-four+five])
        result, _ = power_harmonics(x)
        self.assertAlmostEqual(result['power_h5_minus_h4_db'], -6.0206, delta=.1)
        self.assertAlmostEqual(result['power_f0_hz'], 88, delta=.1)
        changed, _ = power_harmonics(x[:, ::-1]*np.array([-1, 1]))
        self.assertAlmostEqual(result['power_h5_minus_h4_db'], changed['power_h5_minus_h4_db'], places=10)
        self.assertAlmostEqual(result['power_f0_hz'], changed['power_f0_hz'], places=10)

    def test_silence_is_not_a_harmonic_profile(self):
        with self.assertRaises(ValueError):
            power_harmonics(np.zeros((SR, 2)))


if __name__ == '__main__':
    unittest.main()
