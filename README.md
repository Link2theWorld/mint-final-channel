# MINT Final Channel

MINT Final Channel is a bilingual Windows/macOS channel-strip VST3 with a fast
Simple view and a complete Formal view. Audio processing is fully local: there
is no account, telemetry, networking, or cloud inference.

MINT Final Channel 是一款支持中英文界面的通道条 VST3。简洁版用于快速完成清理与
发布准备，专业版提供完整的 EQ、降噪、压缩、混响和限制器控制。所有音频处理均在
本机完成，不包含账号、遥测、联网或云端推理。

![Simple view](showcase/mint-final-channel-simple.png)

![Formal view](showcase/mint-final-channel-formal.png)

## Features / 功能

- Simple and Formal interfaces with persistent `EN / 中文` switching.
- 12-band EQ with filter type and stereo/left/right/mid/side routing.
- Realtime spectral denoising, stereo compression, FDN reverb and look-ahead
  true-peak limiting.
- Allocation-free processing after preparation and stable parameter ordering.
- Formal technical labels remain in English in both interface languages.
- 简洁版提供自动清理、响度目标、均衡风格和空间预设。
- 中文模式采用内嵌圆润字体；专业参数、单位和枚举值保留英文。

## Windows installation / Windows 安装

1. Download the Windows x64 ZIP from GitHub Releases.
2. Copy `MINT Final Channel.vst3` to
   `C:\Program Files\Common Files\VST3\`.
3. Restart your DAW and rescan VST3 plug-ins.

从 GitHub Releases 下载 Windows x64 压缩包，将 `MINT Final Channel.vst3`
复制到 `C:\Program Files\Common Files\VST3\`，然后重启宿主并重新扫描插件。

## Build and test

The DSP core has no external runtime dependency:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure -C Release
```

Building the VST3 requires a pinned iPlug2 checkout:

```sh
cmake -S . -B build-plugin \
  -DVAPORWAVE_BUILD_PLUGIN=ON \
  -DIPLUG2_DIR=/absolute/path/to/iPlug2
cmake --build build-plugin --config Release --target VaporwaveSuite-vst3
```

Release builds are validated with the DSP test suite, pluginval at strictness
level 10, and Steinberg's VST3 validator.

## License

Source code is available under the [MIT License](LICENSE). Bundled fonts and
other third-party components retain their own licenses; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
