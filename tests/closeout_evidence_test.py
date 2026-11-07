import sys
from pathlib import Path
import unittest
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from analyze_closeout_evidence import descriptors,SR

class CloseoutEvidenceTests(unittest.TestCase):
    def test_single_tone_and_antiphase_stereo_have_same_spectrum(self):
        x=.3*np.sin(2*np.pi*1000*np.arange(SR)/SR)
        _,mono=descriptors(np.column_stack([x,x]))
        _,opposed=descriptors(np.column_stack([x,-x]))
        self.assertEqual(mono,opposed)
        self.assertAlmostEqual(mono['centroid_hz_p05_p50_p95'][1],1000,delta=1)
        self.assertAlmostEqual(mono['dominant_line_hz_p05_p50_p95'][1],1000,delta=1)
        self.assertGreater(mono['dominant_line_frames'],0)
    def test_silence_is_not_a_cutoff_or_tone_measurement(self):
        _,result=descriptors(np.zeros((SR,2)))
        self.assertEqual(result['active_frames'],0)
        self.assertEqual(result['dominant_line_frames'],0)
        self.assertIsNone(result['centroid_hz_p05_p50_p95'])
        self.assertIsNone(result['dominant_line_hz_p05_p50_p95'])
if __name__=='__main__':unittest.main()
