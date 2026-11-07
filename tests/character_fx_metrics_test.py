import sys,unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'research/character_fx'))
from analyze_probe import metrics
class CharacterFxMetricsTests(unittest.TestCase):
 def test_coherent_harmonic_ratio(self):
  n=32768;sr=48000;k=2*int(np.floor(1000*n/sr/2+.5))+1;p=2*np.pi*k*np.arange(n)/n
  y=.2*np.sin(p)+.02*np.sin(3*p);m=metrics(np.column_stack([y,y]),'tone_low',sr)
  self.assertAlmostEqual(m['thd_percent'],10,places=6)
  self.assertLess(m['nonharmonic_inband_db_relative_fundamental'],-180)
 def test_silence_and_channel_leak(self):
  x=np.zeros((32768,2));m=metrics(x,'silence',48000);self.assertEqual(m['noise_rms_dbfs'],-300)
  x[:,0]=.2;m=metrics(x,'left_only',48000);self.assertEqual(m['right_leak_dbfs'],-300)
 def test_impulse_location(self):
  x=np.zeros((32768,2));x[199]=.5;m=metrics(x,'impulse',48000);self.assertEqual(m['impulse_peak_index'],199)
if __name__=='__main__':unittest.main()
