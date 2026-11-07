#!/usr/bin/env python3
"""Infer a likely monophonic MIDI performance from a Prophet-6 demo recording.

This is a calibration helper, not a general-purpose pitch tracker.  It uses the
actual patch's oscillator coarse/fine settings, keyboard tracking, sub level,
and hard-sync state to resolve octave ambiguities that ordinary F0 trackers
cannot resolve on Prophet-6 patches (for example when the sub oscillator is
louder than the oscillator fundamental).

Input is a real Prophet-6 program SysEx plus a reference-audio segment.  The
output event CSV can be fed directly to p6_render_events_cal.

The inference is intentionally conservative:
  * it assumes one dominant played note at a time;
  * frames with poor score separation are marked ambiguous;
  * hard-sync patches are anchored primarily to Oscillator 2, the sync master;
  * no claim is made that velocity can be recovered from compressed audio.

Requires numpy/scipy and ffmpeg.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import subprocess
from dataclasses import dataclass, asdict
from pathlib import Path

import numpy as np
from scipy.ndimage import median_filter
from scipy.signal import get_window


# Serialized Prophet-6 program offsets established independently from the
# factory bank and cross-checked against public editor/librarian projects.
OSC1_FREQ = 0
OSC2_FREQ = 1
OSC2_FINE = 2
OSC1_LEVEL = 7
OSC2_LEVEL = 8
SUB_LEVEL = 9
NOISE_LEVEL = 10
SYNC = 11
OSC2_KEY = 12
OSC2_LOW = 13
SLOP = 18
NAME_OFFSET = 107
NAME_LEN = 20


def unpack_sequential_7bit(packed: bytes, wanted: int = 1024) -> bytes:
    """Decode Sequential/DSI's 7-bit SysEx packing."""
    out = bytearray()
    i = 0
    while i < len(packed) and len(out) < wanted:
        header = packed[i]
        i += 1
        for bit in range(7):
            if i >= len(packed) or len(out) >= wanted:
                break
            b = packed[i] | (((header >> bit) & 1) << 7)
            out.append(b)
            i += 1
    if len(out) < wanted:
        raise ValueError(f"packed payload decoded to {len(out)} bytes, expected >= {wanted}")
    return bytes(out[:wanted])


def decode_program(path: Path) -> bytes:
    data = path.read_bytes()
    # Program: F0 01 2D 02 bank program <1171 packed bytes> F7
    # Edit buffer: F0 01 2D 03 <1171 packed bytes> F7
    if len(data) < 10 or data[:3] != b"\xF0\x01\x2D":
        raise ValueError("not a Prophet-6 SysEx message")
    if data[3] == 0x02:
        packed = data[6:-1]
    elif data[3] == 0x03:
        packed = data[4:-1]
    else:
        raise ValueError(f"unsupported Prophet-6 SysEx command 0x{data[3]:02X}")
    return unpack_sequential_7bit(packed)


def decode_mono(path: Path, sr: int, start: float | None = None, end: float | None = None) -> np.ndarray:
    cmd = ["ffmpeg", "-v", "error"]
    if start is not None:
        cmd += ["-ss", f"{start:.9f}"]
    cmd += ["-i", str(path)]
    if end is not None:
        duration = end - (start or 0.0)
        if duration <= 0:
            raise ValueError("end must be after start")
        cmd += ["-t", f"{duration:.9f}"]
    cmd += ["-ac", "1", "-ar", str(sr), "-f", "f32le", "-acodec", "pcm_f32le", "pipe:1"]
    raw = subprocess.check_output(cmd)
    return np.frombuffer(raw, dtype="<f4").astype(np.float64)


def midi_hz(note: float) -> float:
    return 440.0 * 2.0 ** ((note - 69.0) / 12.0)


def fine_semitones(raw: int) -> float:
    # Prophet-6 Osc2 fine: center 127, ±50 cents over 0..254.
    return (float(raw) - 127.0) / 127.0 * 0.5


@dataclass(frozen=True)
class PatchPitchModel:
    name: str
    osc1_offset_st: float
    osc2_offset_st: float
    osc1_level: float
    osc2_level: float
    sub_level: float
    noise_level: float
    sync: bool
    osc2_key: bool
    osc2_low: bool
    slop_raw: int

    @classmethod
    def from_raw(cls, raw: bytes) -> "PatchPitchModel":
        name = raw[NAME_OFFSET:NAME_OFFSET + NAME_LEN].decode("ascii", errors="replace").rstrip(" \x00")
        return cls(
            name=name,
            osc1_offset_st=float(raw[OSC1_FREQ]) - 24.0,
            osc2_offset_st=float(raw[OSC2_FREQ]) - 24.0 + fine_semitones(raw[OSC2_FINE]),
            osc1_level=raw[OSC1_LEVEL] / 127.0,
            osc2_level=raw[OSC2_LEVEL] / 127.0,
            sub_level=raw[SUB_LEVEL] / 127.0,
            noise_level=raw[NOISE_LEVEL] / 127.0,
            sync=raw[SYNC] != 0,
            osc2_key=raw[OSC2_KEY] != 0,
            osc2_low=raw[OSC2_LOW] != 0,
            slop_raw=int(raw[SLOP]),
        )


@dataclass
class FrameResult:
    time_s: float
    midi_note: int | None
    score: float
    second_score: float
    confidence_gap: float
    rms_db_rel: float
    ambiguous: bool


def local_db(freqs: np.ndarray, db: np.ndarray, hz: float, cents: float = 28.0) -> float:
    if hz <= freqs[1] or hz >= freqs[-1]:
        return -120.0
    ratio = 2.0 ** (cents / 1200.0)
    lo, hi = hz / ratio, hz * ratio
    a = int(np.searchsorted(freqs, lo, side="left"))
    b = int(np.searchsorted(freqs, hi, side="right"))
    if b <= a:
        idx = int(np.argmin(np.abs(freqs - hz)))
        return float(db[idx])
    return float(np.max(db[a:b]))


def source_score(freqs: np.ndarray, db: np.ndarray, f0: float, source_weight: float,
                 max_harmonics: int = 8) -> float:
    """Score one expected periodic source using its fundamental and harmonics."""
    if source_weight <= 0.005 or f0 < 20.0 or f0 > freqs[-1] * 0.92:
        return 0.0
    # The fundamental is deliberately overweighted.  This is what separates
    # C4+sub(C2) from the octave-lower false solution where only the sub peak
    # happens to line up with the oscillator fundamental.
    total = 0.0
    norm = 0.0
    for h in range(1, max_harmonics + 1):
        hz = f0 * h
        if hz >= freqs[-1] * 0.95:
            break
        harmonic_weight = (2.2 if h == 1 else 1.0 / math.sqrt(h))
        value = local_db(freqs, db, hz)
        total += harmonic_weight * value
        norm += harmonic_weight
    return source_weight * (total / max(norm, 1e-9))


def autocorr_at_frequency(ac: np.ndarray | None, sr: int, hz: float) -> float:
    """Normalized autocorrelation support for a candidate period."""
    if ac is None or hz <= 0.0:
        return 0.0
    lag = sr / hz
    if lag < 1.0 or lag >= len(ac) - 1:
        return 0.0
    lo = int(math.floor(lag))
    frac = lag - lo
    return float(ac[lo] * (1.0 - frac) + ac[lo + 1] * frac)


def candidate_score(model: PatchPitchModel, note: int, freqs: np.ndarray, db: np.ndarray,
                    periodicity: np.ndarray | None = None, sr: int = 48000) -> float:
    f1 = midi_hz(note + model.osc1_offset_st)
    f2 = midi_hz(note + model.osc2_offset_st) if model.osc2_key and not model.osc2_low else None
    fs = f1 * 0.5

    if model.sync and f2 is not None:
        # Osc2 is the hard-sync master.  Hard sync can make the 2nd/3rd harmonic
        # much stronger than the master fundamental, so the ordinary source
        # scorer's overweighted fundamental creates octave aliases.  Use a
        # harmonic-comb average instead: the correct master explains *all* the
        # partials, whereas an octave-high candidate explains only a subset.
        vals = []
        weights = []
        for h in range(1, 11):
            hz = f2 * h
            if hz >= freqs[-1] * 0.95:
                break
            vals.append(local_db(freqs, db, hz))
            weights.append(1.0 / (h ** 0.4))
        score = float(np.average(vals, weights=weights)) if vals else -1e9
        # A sync spectrum can have a vanishingly weak master fundamental, so an
        # octave-high hypothesis often explains the louder even harmonics and
        # wins by a couple of dB.  Because the spectrum has been normalized to
        # its median noise floor, a genuine peak at half the proposed master is
        # direct evidence that the proposal is an octave alias.  Penalize only
        # subharmonic peaks that stand clearly above the local floor; this is
        # deliberately conservative and does not require the true fundamental
        # itself to be loud.
        half = local_db(freqs, db, f2 * 0.5)
        if half > 12.0:
            score -= 0.35 * (half - 12.0)
        third = local_db(freqs, db, f2 / 3.0)
        if third > 18.0:
            score -= 0.12 * (third - 18.0)
        # Sub can still resolve octave ambiguity when it is audible, but keep it
        # secondary because sync resets make Osc1/sub spectra less textbook.
        if model.sub_level > 0.05:
            score += 0.18 * source_score(freqs, db, fs, model.sub_level, 4)

        # A hard-synced slave can hide the master's fundamental, but the whole
        # waveform is still periodic at the master period. Autocorrelation is
        # therefore a much safer octave anchor than spectral-peak strength.
        # This specifically prevents a strong 2nd harmonic from masquerading
        # as an octave-higher played note. The double-period term penalizes
        # that alias only when the longer period is clearly more coherent.
        if periodicity is not None:
            p1 = autocorr_at_frequency(periodicity, sr, f2)
            p2 = autocorr_at_frequency(periodicity, sr, f2 * 0.5)
            score += 8.0 * p1 - 3.0 * max(0.0, p2 - p1)
        return score

    components: list[tuple[float, float, int]] = []
    if model.osc1_level > 0.005:
        components.append((f1, model.osc1_level, 8))
    if f2 is not None and model.osc2_level > 0.005:
        components.append((f2, model.osc2_level, 8))
    if model.sub_level > 0.005:
        components.append((fs, model.sub_level * 1.12, 5))
    if not components:
        return -1e9

    # Normalize by total active tonal source weight.  Add a small coherence
    # bonus when several predicted fundamentals are simultaneously present.
    weight_sum = sum(w for _, w, _ in components)
    score = sum(source_score(freqs, db, f, w, h) for f, w, h in components) / max(weight_sum, 1e-9)
    fundamentals = [local_db(freqs, db, f) for f, _, _ in components if 25.0 <= f <= freqs[-1] * 0.9]
    if len(fundamentals) >= 2:
        score += 0.18 * float(np.mean(fundamentals))
    return score


def transition_penalty(a: int, b: int) -> float:
    d = abs(a - b)
    if d == 0:
        return 0.0
    if d <= 2:
        return 0.45
    if d <= 5:
        return 0.75
    if d <= 7:
        return 0.95
    if d <= 12:
        return 1.20
    if d <= 24:
        return 1.55
    return 2.2


def infer(model: PatchPitchModel, x: np.ndarray, sr: int, frame_s: float, hop_s: float,
          note_min: int, note_max: int, silence_db: float, ambiguity_gap: float) -> tuple[list[FrameResult], np.ndarray]:
    frame_n = max(512, int(round(frame_s * sr)))
    # Zero-padding gives finer peak placement while keeping temporal resolution.
    nfft = 1
    while nfft < frame_n * 4:
        nfft *= 2
    hop = max(1, int(round(hop_s * sr)))
    window = get_window("hann", frame_n, fftbins=True)
    notes = np.arange(note_min, note_max + 1, dtype=int)
    freqs = np.fft.rfftfreq(nfft, 1.0 / sr)

    starts = np.arange(0, max(1, len(x) - frame_n + 1), hop, dtype=int)
    if len(starts) == 0:
        starts = np.array([0], dtype=int)
    rms = np.empty(len(starts), dtype=float)
    emissions = np.full((len(starts), len(notes)), -1e9, dtype=float)

    for i, st in enumerate(starts):
        frame = x[st:st + frame_n]
        if len(frame) < frame_n:
            frame = np.pad(frame, (0, frame_n - len(frame)))
        rms[i] = math.sqrt(float(np.mean(frame * frame)) + 1e-20)
        windowed = frame * window
        spec = np.abs(np.fft.rfft(windowed, n=nfft)) + 1e-12
        db = 20.0 * np.log10(spec)
        # Remove absolute recording level.  The score then describes where the
        # peaks are, not how loud the YouTube/MP3 segment happened to be.
        db -= float(np.median(db[(freqs >= 25.0) & (freqs <= min(12000.0, sr * 0.45))]))

        periodicity = None
        if model.sync:
            # FFT autocorrelation is cheap relative to the already-computed
            # spectrum and remains robust when hard sync suppresses the master
            # fundamental. Normalize to lag zero so the term is level-free.
            ac_nfft = 1
            while ac_nfft < frame_n * 2:
                ac_nfft *= 2
            ax = np.fft.rfft(windowed - float(np.mean(windowed)), n=ac_nfft)
            periodicity = np.fft.irfft(np.abs(ax) ** 2, n=ac_nfft)[:frame_n]
            periodicity /= max(float(periodicity[0]), 1e-12)

        for j, note in enumerate(notes):
            emissions[i, j] = candidate_score(model, int(note), freqs, db, periodicity, sr)

    peak_rms = max(float(np.max(rms)), 1e-12)
    rms_db_rel = 20.0 * np.log10(rms / peak_rms + 1e-12)
    active = rms_db_rel >= silence_db

    # Viterbi smoothing.  Emission scores are in dB-like units; transition
    # penalties are intentionally small enough to permit real melodic motion.
    dp = np.full_like(emissions, -1e18)
    back = np.zeros_like(emissions, dtype=np.int16)
    dp[0] = emissions[0]
    for t in range(1, len(starts)):
        prev = dp[t - 1]
        for j, n in enumerate(notes):
            penalties = np.array([transition_penalty(int(p), int(n)) for p in notes])
            values = prev - penalties
            k = int(np.argmax(values))
            dp[t, j] = emissions[t, j] + values[k]
            back[t, j] = k
    path = np.empty(len(starts), dtype=int)
    path[-1] = int(np.argmax(dp[-1]))
    for t in range(len(starts) - 1, 0, -1):
        path[t - 1] = int(back[t, path[t]])
    inferred = notes[path]

    # A 3-frame median removes isolated one-hop octave/harmonic mistakes.
    if len(inferred) >= 3:
        inferred = median_filter(inferred, size=3, mode="nearest").astype(int)

    frames: list[FrameResult] = []
    for i, st in enumerate(starts):
        if not active[i]:
            frames.append(FrameResult(
                time_s=(st + frame_n * 0.5) / sr,
                midi_note=None,
                score=float("nan"),
                second_score=float("nan"),
                confidence_gap=0.0,
                rms_db_rel=float(rms_db_rel[i]),
                ambiguous=False,
            ))
            continue
        row = emissions[i]
        order = np.argsort(row)
        best_j = int(np.where(notes == inferred[i])[0][0])
        best = float(row[best_j])
        second = float(row[order[-1] if order[-1] != best_j else order[-2]])
        gap = best - second
        frames.append(FrameResult(
            time_s=(st + frame_n * 0.5) / sr,
            midi_note=int(inferred[i]),
            score=best,
            second_score=second,
            confidence_gap=gap,
            rms_db_rel=float(rms_db_rel[i]),
            ambiguous=gap < ambiguity_gap,
        ))
    return frames, emissions


def compress_events(frames: list[FrameResult], frame_s: float, hop_s: float,
                    min_event_s: float = 0.10) -> list[dict]:
    if not frames:
        return []
    # Build runs on active inferred note.  Boundaries are halfway between frame
    # centres, which is less biased than using the centre itself as note-on.
    events: list[dict] = []
    i = 0
    while i < len(frames):
        if frames[i].midi_note is None:
            i += 1
            continue
        note = frames[i].midi_note
        j = i + 1
        while j < len(frames) and frames[j].midi_note == note:
            j += 1
        first_t = frames[i].time_s
        last_t = frames[j - 1].time_s
        start = max(0.0, first_t - hop_s * 0.5)
        end = last_t + hop_s * 0.5
        # Do not extend across following silence/note transitions beyond the
        # midpoint represented by the hop grid.
        duration = max(hop_s, end - start)
        confs = [f.confidence_gap for f in frames[i:j] if not math.isnan(f.score)]
        amb = [f.ambiguous for f in frames[i:j] if not math.isnan(f.score)]
        if duration >= min_event_s:
            events.append({
                "time_s": start,
                "midi_note": int(note),
                "velocity_0_to_1": 1.0,
                "duration_s": duration,
                "confidence_gap_median": float(np.median(confs)) if confs else 0.0,
                "ambiguous_fraction": float(np.mean(amb)) if amb else 0.0,
            })
        i = j

    # Merge same-note runs separated by a tiny silent gap.
    merged: list[dict] = []
    for ev in events:
        if merged and ev["midi_note"] == merged[-1]["midi_note"]:
            prev_end = merged[-1]["time_s"] + merged[-1]["duration_s"]
            if ev["time_s"] - prev_end <= max(0.08, hop_s * 2.1):
                new_end = ev["time_s"] + ev["duration_s"]
                merged[-1]["duration_s"] = new_end - merged[-1]["time_s"]
                merged[-1]["confidence_gap_median"] = min(
                    merged[-1]["confidence_gap_median"], ev["confidence_gap_median"])
                merged[-1]["ambiguous_fraction"] = max(
                    merged[-1]["ambiguous_fraction"], ev["ambiguous_fraction"])
                continue
        merged.append(ev)
    return merged


def lookup_segment(csv_path: Path, program: int) -> tuple[float, float, str]:
    with csv_path.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            if int(row["program"]) == program:
                return float(row["start_s"]), float(row["end_s"]), row.get("name", "")
    raise ValueError(f"program {program} not found in {csv_path}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("reference_audio", type=Path)
    ap.add_argument("program_sysex", type=Path)
    ap.add_argument("output_events_csv", type=Path)
    ap.add_argument("--program", type=int, help="program number used to look up start/end in --segments")
    ap.add_argument("--segments", type=Path, help="CSV containing program,start_s,end_s")
    ap.add_argument("--start", type=float, help="absolute reference-audio start time")
    ap.add_argument("--end", type=float, help="absolute reference-audio end time")
    ap.add_argument("--sample-rate", type=int, default=48000)
    ap.add_argument("--frame-ms", type=float, default=120.0)
    ap.add_argument("--hop-ms", type=float, default=40.0)
    ap.add_argument("--note-min", type=int, default=24)
    ap.add_argument("--note-max", type=int, default=96)
    ap.add_argument("--silence-db", type=float, default=-34.0)
    ap.add_argument("--ambiguity-gap", type=float, default=1.0)
    ap.add_argument("--frames-csv", type=Path)
    ap.add_argument("--report-json", type=Path)
    args = ap.parse_args()

    raw = decode_program(args.program_sysex)
    model = PatchPitchModel.from_raw(raw)

    start, end = args.start, args.end
    segment_name = model.name
    if args.segments is not None:
        if args.program is None:
            raise SystemExit("--segments requires --program")
        start, end, csv_name = lookup_segment(args.segments, args.program)
        segment_name = csv_name or segment_name
    if start is None or end is None:
        raise SystemExit("provide --start/--end, or --segments plus --program")

    x = decode_mono(args.reference_audio, args.sample_rate, start, end)
    frame_s = args.frame_ms / 1000.0
    hop_s = args.hop_ms / 1000.0
    frames, _ = infer(
        model, x, args.sample_rate, frame_s, hop_s,
        args.note_min, args.note_max, args.silence_db, args.ambiguity_gap,
    )
    events = compress_events(frames, frame_s, hop_s)

    args.output_events_csv.parent.mkdir(parents=True, exist_ok=True)
    with args.output_events_csv.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        # First four columns deliberately match p6_render_events_cal exactly.
        w.writerow(["time_s", "midi_note", "velocity_0_to_1", "duration_s"])
        for ev in events:
            w.writerow([
                f"{ev['time_s']:.6f}", ev["midi_note"],
                f"{ev['velocity_0_to_1']:.6f}", f"{ev['duration_s']:.6f}",
            ])

    if args.frames_csv:
        args.frames_csv.parent.mkdir(parents=True, exist_ok=True)
        with args.frames_csv.open("w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=list(asdict(frames[0]).keys()) if frames else ["time_s"])
            w.writeheader()
            for fr in frames:
                w.writerow(asdict(fr))

    active = [fr for fr in frames if fr.midi_note is not None]
    report = {
        "reference_audio": str(args.reference_audio),
        "reference_start_s": start,
        "reference_end_s": end,
        "program_sysex": str(args.program_sysex),
        "program_number": args.program,
        "patch": asdict(model),
        "segment_name": segment_name,
        "assumption": "one dominant played MIDI note at a time; velocity not inferred",
        "hard_sync_mode": model.sync,
        "events": events,
        "active_frame_count": len(active),
        "ambiguous_frame_fraction": (
            float(np.mean([fr.ambiguous for fr in active])) if active else 1.0
        ),
        "median_confidence_gap": (
            float(np.median([fr.confidence_gap for fr in active])) if active else 0.0
        ),
    }
    if args.report_json:
        args.report_json.parent.mkdir(parents=True, exist_ok=True)
        args.report_json.write_text(json.dumps(report, indent=2), encoding="utf-8")

    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
