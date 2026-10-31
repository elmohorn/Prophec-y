#!/usr/bin/env python3
"""Dependency-free audio metrics for v0.19 hardware/reference matching.

Reads ordinary PCM WAV (including 24-bit) and IEEE-float32 WAV produced by
p6_render_reference. Metrics are intentionally small and interpretable:
frequency from positive-going crossings, DC, RMS, peak, positive-duty fraction,
and H1..H16 amplitudes measured with a Hann-windowed Goertzel projection.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from statistics import median
from typing import Any


HARMONIC_INCLUDE_FLOOR_DB = -70.0
HARMONIC_MISSING_FLOOR_DB = -120.0


@dataclass(frozen=True)
class WavData:
    sample_rate: int
    channels: int
    bits_per_sample: int
    audio_format: int
    block_align: int
    data: bytes

    @property
    def frames(self) -> int:
        return len(self.data) // self.block_align


def _u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def _u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def parse_wav_bytes(raw: bytes) -> WavData:
    if len(raw) < 12 or raw[:4] != b"RIFF" or raw[8:12] != b"WAVE":
        raise ValueError("not a RIFF/WAVE file")

    fmt: tuple[int, int, int, int, int] | None = None
    audio_data: bytes | None = None
    pos = 12
    while pos + 8 <= len(raw):
        chunk_id = raw[pos:pos + 4]
        size = _u32(raw, pos + 4)
        start = pos + 8
        end = start + size
        if end > len(raw):
            raise ValueError(f"truncated WAV chunk {chunk_id!r}")
        payload = raw[start:end]

        if chunk_id == b"fmt ":
            if len(payload) < 16:
                raise ValueError("WAV fmt chunk is too short")
            audio_format = _u16(payload, 0)
            channels = _u16(payload, 2)
            sample_rate = _u32(payload, 4)
            block_align = _u16(payload, 12)
            bits = _u16(payload, 14)
            if audio_format == 0xFFFE:
                if len(payload) < 40:
                    raise ValueError("WAVE_FORMAT_EXTENSIBLE fmt chunk is too short")
                audio_format = _u16(payload, 24)
            fmt = (audio_format, channels, sample_rate, block_align, bits)
        elif chunk_id == b"data":
            audio_data = payload

        pos = end + (size & 1)

    if fmt is None:
        raise ValueError("WAV has no fmt chunk")
    if audio_data is None:
        raise ValueError("WAV has no data chunk")

    audio_format, channels, sample_rate, block_align, bits = fmt
    if audio_format not in (1, 3):
        raise ValueError(f"unsupported WAV audio format {audio_format}")
    if channels < 1 or sample_rate < 1 or block_align < 1:
        raise ValueError("invalid WAV channel/rate/block-align metadata")
    if audio_format == 3 and bits != 32:
        raise ValueError("only float32 IEEE WAV is supported")
    if audio_format == 1 and bits not in (8, 16, 24, 32):
        raise ValueError(f"unsupported PCM bit depth {bits}")
    expected_align = channels * (bits // 8)
    if block_align != expected_align:
        raise ValueError(
            f"unexpected block_align={block_align}, expected {expected_align}"
        )
    if len(audio_data) % block_align != 0:
        raise ValueError("WAV data length is not a whole number of frames")

    return WavData(sample_rate, channels, bits, audio_format, block_align, audio_data)


def read_wav(path: Path) -> WavData:
    return parse_wav_bytes(path.read_bytes())


def _decode_sample(data: bytes, offset: int, audio_format: int, bits: int) -> float:
    if audio_format == 3:
        return float(struct.unpack_from("<f", data, offset)[0])
    if bits == 8:
        return (data[offset] - 128) / 128.0
    if bits == 16:
        return struct.unpack_from("<h", data, offset)[0] / 32768.0
    if bits == 24:
        value = data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16)
        if value & 0x800000:
            value -= 1 << 24
        return value / 8388608.0
    if bits == 32:
        return struct.unpack_from("<i", data, offset)[0] / 2147483648.0
    raise AssertionError("validated format reached impossible decoder branch")


def mono_window(wav: WavData, start_seconds: float, duration_seconds: float) -> list[float]:
    if start_seconds < 0.0 or duration_seconds <= 0.0:
        raise ValueError("analysis start must be >=0 and duration must be >0")
    start = min(wav.frames, int(round(start_seconds * wav.sample_rate)))
    end = min(wav.frames, start + int(round(duration_seconds * wav.sample_rate)))
    if end - start < 32:
        raise ValueError("analysis window is too short or outside the WAV")

    width = wav.bits_per_sample // 8
    out: list[float] = []
    append = out.append
    for frame in range(start, end):
        base = frame * wav.block_align
        total = 0.0
        for channel in range(wav.channels):
            total += _decode_sample(
                wav.data,
                base + channel * width,
                wav.audio_format,
                wav.bits_per_sample,
            )
        append(total / wav.channels)
    return out


def estimate_frequency(samples: list[float], sample_rate: int) -> tuple[float | None, float | None]:
    mean = math.fsum(samples) / len(samples)
    crossings: list[float] = []
    previous = samples[0] - mean
    for i in range(1, len(samples)):
        current = samples[i] - mean
        if previous < 0.0 <= current:
            denominator = current - previous
            fraction = 0.0 if abs(denominator) < 1.0e-20 else (-previous / denominator)
            crossings.append((i - 1) + fraction)
        previous = current

    if len(crossings) < 6:
        return None, None
    periods = [b - a for a, b in zip(crossings, crossings[1:]) if b > a]
    if len(periods) < 5:
        return None, None
    period = median(periods)
    if period <= 0.0:
        return None, None

    deviations = [abs(p - period) for p in periods]
    relative_jitter = median(deviations) / period
    if relative_jitter > 0.08:
        return None, relative_jitter
    return sample_rate / period, relative_jitter


def goertzel_amplitude(samples: list[float], sample_rate: int, frequency: float) -> float:
    if frequency <= 0.0 or frequency >= sample_rate * 0.5:
        return 0.0
    n = len(samples)
    omega = 2.0 * math.pi * frequency / sample_rate
    coefficient = 2.0 * math.cos(omega)
    s1 = 0.0
    s2 = 0.0
    window_sum = 0.0
    denominator = max(1, n - 1)
    for i, sample in enumerate(samples):
        weight = 0.5 - 0.5 * math.cos(2.0 * math.pi * i / denominator)
        value = sample * weight
        s0 = value + coefficient * s1 - s2
        s2 = s1
        s1 = s0
        window_sum += weight
    power = s1 * s1 + s2 * s2 - coefficient * s1 * s2
    return 2.0 * math.sqrt(max(0.0, power)) / max(window_sum, 1.0e-20)


def audio_metrics(samples: list[float], sample_rate: int) -> dict[str, Any]:
    mean = math.fsum(samples) / len(samples)
    ac = [x - mean for x in samples]
    rms = math.sqrt(math.fsum(x * x for x in ac) / len(ac))
    peak = max(abs(x) for x in ac)
    duty_positive = sum(1 for x in ac if x >= 0.0) / len(ac)
    frequency, crossing_jitter = estimate_frequency(samples, sample_rate)

    harmonics: list[dict[str, float | int | None]] = []
    if frequency is not None:
        amplitudes: list[float] = []
        for harmonic in range(1, 17):
            f = frequency * harmonic
            amplitude = (
                goertzel_amplitude(ac, sample_rate, f)
                if f < sample_rate * 0.5 else 0.0
            )
            amplitudes.append(amplitude)
        h1 = amplitudes[0]
        for harmonic, amplitude in enumerate(amplitudes, 1):
            relative_db = None
            if h1 > 1.0e-15 and amplitude > 1.0e-15:
                relative_db = 20.0 * math.log10(amplitude / h1)
            harmonics.append({
                "harmonic": harmonic,
                "frequency_hz": frequency * harmonic,
                "amplitude": amplitude,
                "relative_db": relative_db,
            })

    centroid_hz = None
    if frequency is not None and harmonics:
        weighted = 0.0
        total = 0.0
        for item in harmonics:
            amplitude = float(item["amplitude"] or 0.0)
            power = amplitude * amplitude
            weighted += float(item["frequency_hz"] or 0.0) * power
            total += power
        if total > 1.0e-30:
            centroid_hz = weighted / total

    return {
        "frames": len(samples),
        "mean_dc": mean,
        "rms_ac": rms,
        "peak_ac": peak,
        "crest_factor": peak / rms if rms > 1.0e-20 else None,
        "positive_duty_fraction": duty_positive,
        "frequency_hz": frequency,
        "crossing_period_mad_fraction": crossing_jitter,
        "harmonic_centroid_hz_h1_h16": centroid_hz,
        "harmonics": harmonics,
    }


def _db_ratio(numerator: float, denominator: float) -> float | None:
    if numerator <= 1.0e-20 or denominator <= 1.0e-20:
        return None
    return 20.0 * math.log10(numerator / denominator)


def compare_metrics(hardware: dict[str, Any], model: dict[str, Any]) -> dict[str, Any]:
    hw_f = hardware.get("frequency_hz")
    model_f = model.get("frequency_hz")
    pitch_cents = None
    if isinstance(hw_f, (int, float)) and isinstance(model_f, (int, float)) and hw_f > 0 and model_f > 0:
        pitch_cents = 1200.0 * math.log2(model_f / hw_f)

    harmonic_deltas: list[dict[str, float | int | bool | None]] = []
    absolute_deltas: list[float] = []
    hw_harmonics = hardware.get("harmonics") or []
    model_harmonics = model.get("harmonics") or []
    for hw, md in zip(hw_harmonics, model_harmonics):
        harmonic = int(hw["harmonic"])
        hw_raw = hw.get("relative_db")
        model_raw = md.get("relative_db")
        hw_db = float(hw_raw) if isinstance(hw_raw, (int, float)) else HARMONIC_MISSING_FLOOR_DB
        model_db = float(model_raw) if isinstance(model_raw, (int, float)) else HARMONIC_MISSING_FLOOR_DB
        included = 2 <= harmonic <= 16 and max(hw_db, model_db) >= HARMONIC_INCLUDE_FLOOR_DB
        delta = model_db - hw_db if included else None
        if delta is not None:
            absolute_deltas.append(abs(delta))
        harmonic_deltas.append({
            "harmonic": harmonic,
            "hardware_relative_db": hw_raw,
            "model_relative_db": model_raw,
            "included_in_h2_h16_metric": included,
            "model_minus_hardware_db": delta,
        })

    return {
        "pitch_error_cents": pitch_cents,
        "rms_error_db": _db_ratio(float(model["rms_ac"]), float(hardware["rms_ac"])),
        "peak_error_db": _db_ratio(float(model["peak_ac"]), float(hardware["peak_ac"])),
        "positive_duty_error": float(model["positive_duty_fraction"]) - float(hardware["positive_duty_fraction"]),
        "harmonic_include_floor_db": HARMONIC_INCLUDE_FLOOR_DB,
        "harmonic_missing_floor_db": HARMONIC_MISSING_FLOOR_DB,
        "h2_h16_terms_used": len(absolute_deltas),
        "mean_abs_h2_h16_error_db": (
            math.fsum(absolute_deltas) / len(absolute_deltas)
            if absolute_deltas else None
        ),
        "harmonic_deltas": harmonic_deltas,
    }


def analyze_file(path: Path, start: float, duration: float) -> dict[str, Any]:
    wav = read_wav(path)
    samples = mono_window(wav, start, duration)
    result = audio_metrics(samples, wav.sample_rate)
    result.update({
        "path": str(path),
        "sample_rate_hz": wav.sample_rate,
        "channels": wav.channels,
        "bits_per_sample": wav.bits_per_sample,
        "audio_format": "PCM" if wav.audio_format == 1 else "IEEE_FLOAT",
        "window_start_seconds": start,
        "window_duration_seconds": duration,
    })
    return result


def _make_float32_stereo_wav(samples: list[float], sample_rate: int) -> bytes:
    interleaved = b"".join(struct.pack("<ff", x, x) for x in samples)
    fmt = struct.pack("<HHIIHH", 3, 2, sample_rate, sample_rate * 8, 8, 32)
    body = b"fmt " + struct.pack("<I", len(fmt)) + fmt
    body += b"data" + struct.pack("<I", len(interleaved)) + interleaved
    return b"RIFF" + struct.pack("<I", len(body) + 4) + b"WAVE" + body


def run_self_test() -> int:
    sample_rate = 48000
    count = sample_rate
    sine = [math.sin(2.0 * math.pi * 440.0 * n / sample_rate) for n in range(count)]
    parsed = parse_wav_bytes(_make_float32_stereo_wav(sine, sample_rate))
    decoded = mono_window(parsed, 0.0, 1.0)
    metrics = audio_metrics(decoded, sample_rate)
    assert metrics["frequency_hz"] is not None
    assert abs(float(metrics["frequency_hz"]) - 440.0) < 0.2
    assert abs(float(metrics["rms_ac"]) - math.sqrt(0.5)) < 0.002
    assert len(metrics["harmonics"]) == 16

    duty = 0.25
    pulse = [1.0 if (440.0 * n / sample_rate) % 1.0 < duty else -1.0 for n in range(count)]
    pulse_metrics = audio_metrics(pulse, sample_rate)
    assert pulse_metrics["frequency_hz"] is not None
    assert abs(float(pulse_metrics["frequency_hz"]) - 440.0) < 0.5
    assert abs(float(pulse_metrics["positive_duty_fraction"]) - duty) < 0.01

    comparison = compare_metrics(pulse_metrics, pulse_metrics)
    assert abs(float(comparison["pitch_error_cents"] or 0.0)) < 1.0e-9
    assert abs(float(comparison["rms_error_db"] or 0.0)) < 1.0e-9
    assert int(comparison["h2_h16_terms_used"]) > 0
    assert abs(float(comparison["mean_abs_h2_h16_error_db"] or 0.0)) < 1.0e-9

    print("reference_audio_metrics self-test OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Measure v0.19 reference WAV metrics.")
    parser.add_argument("hardware", type=Path, nargs="?", help="WAV to analyze")
    parser.add_argument("--compare", type=Path, help="optional model WAV for comparison")
    parser.add_argument("--start-seconds", type=float, default=0.0,
                        help="hardware/single-file analysis start")
    parser.add_argument("--compare-start-seconds", type=float,
                        help="model analysis start; defaults to --start-seconds")
    parser.add_argument("--duration-seconds", type=float, default=1.0)
    parser.add_argument("--pretty", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return run_self_test()
    if args.hardware is None:
        parser.error("hardware WAV is required unless --self-test is used")

    hardware = analyze_file(args.hardware, args.start_seconds, args.duration_seconds)
    output: dict[str, Any] = {"hardware": hardware}
    if args.compare is not None:
        model_start = args.start_seconds if args.compare_start_seconds is None else args.compare_start_seconds
        model = analyze_file(args.compare, model_start, args.duration_seconds)
        if model["sample_rate_hz"] != hardware["sample_rate_hz"]:
            raise ValueError("hardware/model sample rates differ; do not resample implicitly")
        output["model"] = model
        output["comparison"] = compare_metrics(hardware, model)

    json.dump(output, sys.stdout, indent=2 if args.pretty else None, sort_keys=True)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, struct.error) as exc:
        print(f"reference_audio_metrics: {exc}", file=sys.stderr)
        raise SystemExit(1)
