import csv,json,subprocess,sys,tempfile,unittest
from pathlib import Path
TOOL=Path(__file__).resolve().parents[1]/'tools/probe_event_uncertainty.py'

class EventUncertaintyTests(unittest.TestCase):
    def test_only_gate_lengths_change_and_source_is_unchanged(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);events=root/'events.csv'
            original='time_s,midi_note,velocity_0_to_1,duration_s\n0.125,60,0.75,0.8\n1.0,64,0.5,0.4\n'
            events.write_text(original)
            manifest=root/'suite.csv';manifest.write_text('case_id,events,reference,sysex,trust,provenance\nTEST,events.csv,ref.wav,patch.syx,A,original\n')
            out=root/'out'
            subprocess.run([sys.executable,str(TOOL),str(manifest),'--case','TEST','--output',str(out)],check=True,capture_output=True)
            self.assertEqual(events.read_text(),original)
            for fraction,tag in [(0.25,'025'),(.5,'050'),(.75,'075'),(1.,'100')]:
                rows=list(csv.DictReader((out/f'gate{tag}_events.csv').read_text().splitlines()))
                self.assertEqual([r['time_s'] for r in rows],['0.125','1.0'])
                self.assertEqual([r['midi_note'] for r in rows],['60','64'])
                self.assertEqual([r['velocity_0_to_1'] for r in rows],['0.75','0.5'])
                self.assertAlmostEqual(float(rows[0]['duration_s']),.8*fraction)
            self.assertTrue(all(r['trust']=='C' for r in csv.DictReader((out/'manifest.csv').read_text().splitlines())))
    def test_nonfinite_duration_is_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'events.csv').write_text('time_s,midi_note,velocity_0_to_1,duration_s\n0,60,1,nan\n')
            (root/'suite.csv').write_text('case_id,events,reference,sysex,trust,provenance\nTEST,events.csv,ref.wav,patch.syx,A,original\n')
            r=subprocess.run([sys.executable,str(TOOL),str(root/'suite.csv'),'--case','TEST','--output',str(root/'out')],capture_output=True)
            self.assertNotEqual(r.returncode,0)

if __name__=='__main__':unittest.main()
