# MINT Final Channel 0.1.2

Stability and Simple-mode behavior update.

## Fixed

- Removed host latency-change notifications from the realtime audio callback,
  preventing DAW processing-graph rebuilds from the audio thread.
- Added fixed worst-case latency compensation across active, bypassed and
  module-toggle states so latency never changes during playback.
- Made master bypass output the latency-aligned dry signal.
- Made Auto Clean engage denoising, compression, loudness control and limiting
  as one audible processing macro.
- Changed the central Auto Clean state from ambiguous `BYPASS / ACTIVE` wording
  to explicit `OFF / ON`, with a description of the stages it activates.
- Space presets now enable reverb automatically; Dry disables it.
- Increased the wet levels of Room, Studio, Plate, Hall and Dream presets so
  their effect is clearly audible in Simple mode.
- The default plug-in state is transparent until Auto Clean, a module or a
  non-Dry space preset is enabled.

## Validation

- Fixed-latency bypass transparency: passed.
- Stable latency across bypass, Auto Clean and limiter-lookahead changes: passed.
- Auto Clean compressor engagement: passed.
- Reverb tail and DSP stability tests: passed.
