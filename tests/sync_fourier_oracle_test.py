"""Analytic identities and numerical quadrature checks, independent of synth."""
import importlib.util
from pathlib import Path
import unittest
import numpy as np
spec=importlib.util.spec_from_file_location('oracle',Path(__file__).resolve().parents[1]/'tools/sync_fourier_oracle.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class OracleTests(unittest.TestCase):
    def test_triangle_identity(self):
        a=abs(m.coefficients(1.,0.,harmonics=10))
        for h in (1,3,5,7,9):self.assertAlmostEqual(a[h-1],4/(np.pi*np.pi*h*h),places=12)
        self.assertLess(max(a[1::2]),1e-12)
    def test_integer_ratio_removes_nonmultiples(self):
        c=abs(m.coefficients(4.,.25,harmonics=20))
        self.assertLess(max(c[[h%4!=0 for h in range(1,21)]]),1e-11)
    def test_segment_coverage_and_numeric_quadrature(self):
        ratio=2**(28/12);seg=m.segments(ratio,.25)
        self.assertAlmostEqual(sum(b-a for a,b,_,_ in seg),1.,places=12)
        u=(np.arange(262144)+.5)/262144
        # Triangle->saw at shape .25: ramp durations .75/.25 (finite-edge contract).
        phase=np.mod(u*ratio,1);up=.75-.5e-6
        x=np.where(phase<up,2*phase/up-1,1-2*(phase-up)/(1-up))
        numerical=np.array([np.mean(x*np.exp(-2j*np.pi*h*u)) for h in range(1,21)])
        np.testing.assert_allclose(m.coefficients(ratio,.25),numerical,atol=2e-8,rtol=0)
    def test_bad_geometry_is_rejected(self):
        with self.assertRaises(ValueError):m.coefficients(float('nan'),.5)
        with self.assertRaises(ValueError):m.coefficients(2.,2.)

if __name__=='__main__':unittest.main()
