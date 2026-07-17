# Architecture

## Audio thread

`vaporwave::Processor` owns the complete fixed-order processing graph. All
variable-sized storage is allocated by `prepare()`. `process()` performs no
heap allocation, file access, model loading, network access, or mutex locking.

The host-facing wrapper snapshots normalized iPlug2 parameters once per block,
copies them into the fixed-size `ParameterState`, and processes oversized host
blocks in preallocated chunks.

## Denoise engine policy

- **Mint RT** and **Live** use the bundled 256-point STFT engine. Its
  algorithmic latency is 5.33 ms at 48 kHz and 5.80 ms at 44.1 kHz.
- **Voice Live** is reserved for the audited RNNoise adapter.
- **Voice AI** is reserved for the audited DeepFilterNet adapter.
- Until an optional adapter is compiled and its model is ready, selecting a
  voice engine falls back to Live without blocking or silencing audio.

Optional engines must implement the same fixed-latency, preallocated interface.
Model construction and warm-up happen off the audio thread. Engine changes use
a short dual-path crossfade and only publish readiness after warm-up.

## State compatibility

Host-visible parameter indices are append-only. Existing indices must never be
reordered or reused. `ParameterState::kStateVersion` versions non-host preset
files. Factory preset JSON is immutable at runtime; user presets belong in the
platform application-data directory.

## Release boundary

The v0.1 development build is unsigned and uses placeholder vendor contact
fields. A release build is not publishable until product identity, signing
credentials, model revisions/checksums, and all transitive notices are filled
in and reviewed.
