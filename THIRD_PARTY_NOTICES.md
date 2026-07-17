# Third-party notices

Release packaging must include this file and the exact license texts from every
pinned dependency.

| Component | Intended use | License |
| --- | --- | --- |
| iPlug2 | Plug-in framework and graphics | zlib-like |
| Steinberg VST3 SDK 3.8+ | VST3 interface | MIT |
| Varela Round | Latin interface font | SIL Open Font License 1.1 |
| GenSenRounded | Subsetted CJK interface font | SIL Open Font License 1.1 |
| RNNoise (optional) | Low-CPU voice denoising | BSD-3-Clause |
| DeepFilterNet (optional) | High-quality voice enhancement | MIT or Apache-2.0 |

No optional model or runtime may be enabled in a commercial build until its
source revision, model checksum, complete transitive license set, and model
weight redistribution rights are recorded in the release manifest.

The complete Varela Round and GenSenRounded license texts are distributed next
to their font files in `plugin/resources/fonts` and inside binary releases.

