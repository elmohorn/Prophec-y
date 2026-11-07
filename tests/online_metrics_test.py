"""Synthetic guards for calibration measurements, independent of synth DSP."""
import importlib.util
from pathlib import Path
import unittest
import numpy as np

spec=importlib.util.spec_from_file_location('online_metrics',Path(__file__).resolve().parents[1]/'tools/compare_online_suite.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class MetricTests(unittest.TestCase):
    def setUp(self):
        t=np.arange(48000)/48000
        self.x=np.column_stack([np.sin(2*np.pi*220*t),np.sin(2*np.pi*220*t+.2)])
    def test_gain_is_reported_but_does_not_change_shape_distance(self):
        a=m.compare(self.x,self.x,'steady')
        b=m.compare(self.x,self.x*.25,'steady')
        self.assertAlmostEqual(b['candidate_gain_match_db'],12.0411998266,places=6)
        self.assertLess(a['mr_log_spectral_mae_db'],1e-10)
        self.assertLess(b['mr_log_spectral_mae_db'],1e-10)
    def test_stereo_antiphase_does_not_erase_spectral_energy(self):
        x=self.x.copy();x[:,1]=-x[:,0]
        _,p=m.spectrum(x,2048)
        self.assertGreater(p.sum(),.1)
    def test_alignment_recovers_known_envelope_delay(self):
        x=self.x.copy();x[:12000]=0;x[24000:]=0
        y=np.pad(x,((2400,0),(0,0)))[:len(x)]
        _,_,lag=m.aligned_pair(x,y)
        self.assertAlmostEqual(lag,.05,places=6)
    def test_wrong_harmonic_profile_is_detected(self):
        t=np.arange(48000)/48000
        ref=np.sin(2*np.pi*88*t)+2*np.sin(2*np.pi*352*t)
        cand=np.sin(2*np.pi*88*t)+2*np.sin(2*np.pi*440*t)
        a=m.harmonic_metrics(np.column_stack([ref,ref]),np.column_stack([cand,cand]),88)
        self.assertLess(a['reference_h5_minus_h4_db'],-20)
        self.assertGreater(a['candidate_h5_minus_h4_db'],20)
    def test_silence_is_rejected(self):
        with self.assertRaises(ValueError):m.compare(np.zeros_like(self.x),self.x)

if __name__=='__main__':unittest.main()
