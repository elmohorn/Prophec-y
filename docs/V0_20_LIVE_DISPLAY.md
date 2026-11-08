# Live output and contextual OLED

The existing parameter/model preview and its FigBug-inspired cache remain in
`ContextDisplay` and `ContextPreview`. The trace is a model, not measured audio.
As in the earlier `ui-waveform-layout` experiment, the plot grows with the
display and oscillator traces are not filled to an artificial baseline.

The new modes display post-master plugin output: stereo-power spectrum and
left-channel waveform. Click the OLED or focus it and press Space/Enter to
cycle spectrum, waveform and contextual preview. Editing a parameter shows
its existing contextual preview temporarily. The live views are screen-only
and do not modify audio, saved state, or host parameters.

While at least one editor exists, the audio callback captures stereo output
into fixed-size 1024-sample frames and publishes through the existing
`RealtimeSnapshotMailbox`; it never waits or allocates. FFT and painting run
on the UI thread. The FFT uses a Hann window and stereo power, preserving
opposite-polarity stereo. Spectrum uses logarithmic frequency bands with a
maximum hold per band; 1024 samples are coarse at bass frequencies. If the
host stops calling the processor the display clears after roughly one second.

Native editor tests, concurrent/realtime testing, actual host performance and
visual acceptance remain required before release. This feature does not
diagnose or resolve the separately reported clicks or background rumble.
