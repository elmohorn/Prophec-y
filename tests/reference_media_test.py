import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest
import wave

import numpy as np

spec = importlib.util.spec_from_file_location('media', Path(__file__).resolve().parents[1] / 'tools/inspect_reference_media.py')
media = importlib.util.module_from_spec(spec)
spec.loader.exec_module(media)


@unittest.skipUnless(shutil.which('ffmpeg') and shutil.which('ffprobe'), 'requires ffmpeg/ffprobe')
class ReferenceMediaTests(unittest.TestCase):
    def test_error_page_disguised_as_webm_is_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / 'source.webm'
            path.write_text('<html><title>Site Unavailable</title></html>')
            with self.assertRaisesRegex(ValueError, 'document instead of audio'):
                media.inspect(path)

    def test_complete_decode_and_duration_gate(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / 'tone.wav'
            rate = 16000
            tone = (np.sin(2 * np.pi * 440 * np.arange(rate) / rate) * 8192).astype('<i2')
            with wave.open(str(path), 'wb') as f:
                f.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
                f.writeframes(tone.tobytes())
            result = media.inspect(path, expected_duration=1, tolerance=.001)
            self.assertEqual(result['decoded_frames'], rate)
            self.assertEqual(result['integrity'], 'full-decode-passed')
            self.assertFalse(result['patch_identity_verified'])
            with self.assertRaisesRegex(ValueError, 'duration does not match'):
                media.inspect(path, expected_duration=2, tolerance=.001)


if __name__ == '__main__':
    unittest.main()
