import sys
from pathlib import Path
import unittest

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from analyze_selfosc_recording import analyze, SR


class SelfOscRecordingTests(unittest.TestCase):
    def test_recovers_known_semitone_plateaus(self):
        frequencies = np.repeat(1200*2**(np.arange(8)/12), int(.25*SR))
        x = .3*np.sin(2*np.pi*np.cumsum(frequencies)/SR)
        *_, plateaus, transitions = analyze(np.column_stack([x, x]))
        self.assertEqual(len(plateaus), 8)
        self.assertEqual(len(transitions), 7)
        self.assertTrue(all(abs(r['distance_cents']-100) < 2 for r in transitions))

    def test_constant_tone_does_not_invent_steps(self):
        x = .3*np.sin(2*np.pi*1200*np.arange(SR)/SR)
        *_, plateaus, transitions = analyze(np.column_stack([x, x]))
        self.assertEqual(len(plateaus), 1)
        self.assertEqual(transitions, [])


if __name__ == '__main__':
    unittest.main()
