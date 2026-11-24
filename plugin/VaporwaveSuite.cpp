#include "VaporwaveSuite.h"
#include "VaporwaveEditor.h"
#include "MintUIFontData.h"
#include "MintCJKFontData.h"
#include "IPlug_include_in_plug_src.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace {

const char* bandParameterName(int band, const char* field) {
  static thread_local char name[64];
  std::snprintf(name, sizeof(name), "EQ %02d %s", band + 1, field);
  return name;
}

} // namespace

VaporwaveSuite::VaporwaveSuite(const InstanceInfo& info)
  : Plugin(info, MakeConfig(kNumParams, 12)) {
  initializeParameters();

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* graphics) {
    if (graphics->NControls()) {
      updateModeVisibility();
      return;
    }

    using namespace vaporwave_ui;
    const bool zh = GetParam(kParamLanguage)->Bool();
    const char* font = zh ? kCJKFont : kFont;
    const auto text = [zh](const char* en, const char* cn) { return zh ? cn : en; };
    const auto style = vaporwave_ui::Style(font, zh);
    const auto compactStyle = vaporwave_ui::CompactStyle(font, zh);
    const IRECT bounds = graphics->GetBounds();
    graphics->LoadFont(kFont, const_cast<unsigned char*>(kMintUIFontData), kMintUIFontDataSize);
    graphics->LoadFont(kCJKFont, const_cast<unsigned char*>(kMintCJKFontData), kMintCJKFontDataSize);
    graphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    graphics->AttachPanelBackground(kBackground);

    graphics->AttachControl(new ITextControl(
      IRECT(24, 12, 500, 54), text("MINT // FINAL CHANNEL", "MINT // 最终通道条"),
      IText(24.0f, kWhite, font, EAlign::Near)), kCtrlTitle);
    graphics->AttachControl(new ITextControl(
      IRECT(24, 47, 500, 70), text("THE FINAL WORD IN CHANNEL STRIPS", "调音的终结和最后的通道条"),
      IText(10.0f, kPink, font, EAlign::Near)));
    graphics->AttachControl(new HardwareBypassControl(
      IRECT(365, 14, 515, 62), kParamMasterBypass, font, zh), kNoTag, "simple");
    if (zh) {
      graphics->AttachControl(new IVTabSwitchControl(
        IRECT(870, 18, 1154, 52), kParamViewMode,
        {"简洁", "专业"}, "", style, EVShape::EndsRounded), kCtrlMode);
    } else {
      graphics->AttachControl(new IVTabSwitchControl(
        IRECT(870, 18, 1154, 52), kParamViewMode,
        {"SIMPLE", "FORMAL"}, "", style, EVShape::EndsRounded), kCtrlMode);
    }
    graphics->AttachControl(new IVTabSwitchControl(
      IRECT(1010, 56, 1154, 80), kParamLanguage,
      {"EN", "中文"}, "", compactStyle, EVShape::EndsRounded));

    graphics->AttachControl(new ITextControl(
      IRECT(525, 18, 850, 58),
      text("IN  -∞     OUT  -∞     LUFS  --     NR  0.0 dB",
           "输入 -∞   输出 -∞   响度 --   降噪 0.0 dB"),
      IText(12.0f, kMuted, font, EAlign::Far)), kCtrlMeters);

    // Simple: one primary action plus scene/preset choices.
    graphics->AttachControl(new ITextControl(
      IRECT(100, 110, 1080, 170), text("ONE MOVE. READY TO RELEASE.", "一步完成，准备发布。"),
      IText(28.0f, kWhite, font)), kNoTag, "simple");
    graphics->AttachControl(new IVToggleControl(
      IRECT(390, 185, 790, 315), kParamAutoClean, text("AUTO CLEAN", "自动清理"),
      style.WithValueText(IText(24.0f, kWhite, font)),
      text("OFF", "关闭"), text("ON", "开启")),
      kNoTag, "simple");
    graphics->AttachControl(new ITextControl(
      IRECT(280, 318, 900, 350),
      text("DENOISE + COMPRESS + LEVEL + LIMIT",
           "降噪 + 压缩 + 响度整理 + 限幅"),
      IText(zh ? 11.0f : 9.0f, kMuted, font)), kNoTag, "simple");
    if (zh) {
      graphics->AttachControl(new IVTabSwitchControl(
        IRECT(210, 370, 970, 425), kParamLoudnessTarget,
        {"流媒体 -14", "播客 -16", "广播 -23"},
        "目标响度", style, EVShape::EndsRounded), kNoTag, "simple");
      graphics->AttachControl(new IVTabSwitchControl(
        IRECT(100, 480, 1080, 535), kParamEqPreset,
        {"平直", "温暖", "明亮", "人声", "低频", "空气感", "自定义"},
        "均衡风格", style, EVShape::EndsRounded), kNoTag, "simple");
      graphics->AttachControl(new IVTabSwitchControl(
        IRECT(100, 600, 1080, 655), kParamReverbPreset,
        {"干声", "房间", "录音棚", "板式", "大厅", "梦幻", "自定义"},
        "空间", style, EVShape::EndsRounded), kNoTag, "simple");
    } else {
      graphics->AttachControl(new IVTabSwitchControl(
        IRECT(210, 370, 970, 425), kParamLoudnessTarget,
        {"STREAMING  -14", "PODCAST  -16", "BROADCAST  -23"},
        "TARGET LOUDNESS", style, EVShape::EndsRounded), kNoTag, "simple");
      graphics->AttachControl(new IVTabSwitchControl(
        IRECT(100, 480, 1080, 535), kParamEqPreset,
        {"FLAT", "WARM", "BRIGHT", "VOCAL", "BASS", "AIR", "CUSTOM"},
        "EQ CHARACTER", style, EVShape::EndsRounded), kNoTag, "simple");
      graphics->AttachControl(new IVTabSwitchControl(
        IRECT(100, 600, 1080, 655), kParamReverbPreset,
        {"DRY", "ROOM", "STUDIO", "PLATE", "HALL", "DREAM", "CUSTOM"},
        "SPACE", style, EVShape::EndsRounded), kNoTag, "simple");
    }

    // Formal: spectrum/EQ workspace.
    const auto technicalStyle = vaporwave_ui::Style(kFont, false);
    const auto technicalCompactStyle = vaporwave_ui::CompactStyle(kFont, false);
    graphics->AttachControl(new ITextControl(
      IRECT(24, 12, 350, 54), text("MINT // FINAL CHANNEL", "MINT // 最终通道条"),
      IText(24.0f, kWhite, font, EAlign::Near)), kNoTag, "formal");
    graphics->AttachControl(new ITextControl(
      IRECT(24, 47, 380, 70), text("THE FINAL WORD IN CHANNEL STRIPS", "调音的终结和最后的通道条"),
      IText(10.0f, kPink, font, EAlign::Near)), kNoTag, "formal");
    graphics->AttachControl(new HardwareBypassControl(
      IRECT(365, 14, 515, 62), kParamMasterBypass, kFont, false), kNoTag, "formal");
    graphics->AttachControl(new EqGraphControl(
      IRECT(20, 88, 775, 390)), kNoTag, "formal");
    graphics->AttachControl(new IVToggleControl(
      IRECT(30, 98, 145, 128), kParamEqEnabled, "EQ", technicalStyle),
      kNoTag, "formal");

    for (int band = 0; band < 12; ++band) {
      const int column = band % 4;
      const int row = band / 4;
      const float x = 25.0f + static_cast<float>(column) * 188.0f;
      const float y = 405.0f + static_cast<float>(row) * 100.0f;
      char frequencyLabel[16];
      char gainLabel[16];
      char bandLabel[8];
      std::snprintf(frequencyLabel, sizeof(frequencyLabel), "B%d FREQ", band + 1);
      std::snprintf(gainLabel, sizeof(gainLabel), "B%d GAIN", band + 1);
      std::snprintf(bandLabel, sizeof(bandLabel), "B%d", band + 1);
      graphics->AttachControl(new IVToggleControl(
        IRECT(x, y, x + 34, y + 23), EqParam(band, kEqEnabled), bandLabel,
        technicalCompactStyle), kNoTag, "formal");
      graphics->AttachControl(new IVMenuButtonControl(
        IRECT(x + 38, y, x + 112, y + 23), EqParam(band, kEqType), "TYPE",
        technicalCompactStyle), kNoTag, "formal");
      graphics->AttachControl(new IVMenuButtonControl(
        IRECT(x + 116, y, x + 184, y + 23), EqParam(band, kEqChannel), "CHAN",
        technicalCompactStyle), kNoTag, "formal");
      graphics->AttachControl(new IVKnobControl(
        IRECT(x, y + 25, x + 60, y + 96), EqParam(band, kEqFrequency),
        frequencyLabel, technicalCompactStyle), kNoTag, "formal");
      graphics->AttachControl(new IVKnobControl(
        IRECT(x + 62, y + 25, x + 122, y + 96), EqParam(band, kEqGain),
        gainLabel, technicalCompactStyle), kNoTag, "formal");
      graphics->AttachControl(new IVKnobControl(
        IRECT(x + 124, y + 25, x + 184, y + 96), EqParam(band, kEqQ),
        "Q", technicalCompactStyle), kNoTag, "formal");
    }

    // Formal: right-side module rack.
    const std::array<float, 4> knobX { 800.0f, 890.0f, 980.0f, 1070.0f };
    const std::array<float, 7> denseKnobX {
      800.0f, 851.0f, 902.0f, 953.0f, 1004.0f, 1055.0f, 1106.0f
    };
    graphics->AttachControl(new IVToggleControl(
      IRECT(800, 82, 875, 112), kParamDenoiseEnabled, "DENOISE", technicalCompactStyle),
      kNoTag, "formal");
    graphics->AttachControl(new IVMenuButtonControl(
      IRECT(880, 82, 980, 112), kParamDenoiseEngine, "ENGINE", technicalCompactStyle),
      kNoTag, "formal");
    graphics->AttachControl(new IVMenuButtonControl(
      IRECT(985, 82, 1075, 112), kParamContentMode, "CONTENT", technicalCompactStyle),
      kNoTag, "formal");
    graphics->AttachControl(new IVToggleControl(
      IRECT(1080, 82, 1155, 112), kParamNoiseLearn, "LEARN", technicalCompactStyle),
      kNoTag, "formal");
    graphics->AttachControl(new IVKnobControl(
      IRECT(knobX[0], 118, knobX[0] + 76, 210), kParamDenoiseReduction,
      "REDUCE", technicalStyle), kNoTag, "formal");
    graphics->AttachControl(new IVKnobControl(
      IRECT(knobX[1], 118, knobX[1] + 76, 210), kParamDenoiseSensitivity,
      "SENSE", technicalStyle), kNoTag, "formal");
    graphics->AttachControl(new IVKnobControl(
      IRECT(knobX[2], 118, knobX[2] + 76, 210), kParamDenoiseSmoothing,
      "SMOOTH", technicalStyle), kNoTag, "formal");
    graphics->AttachControl(new IVKnobControl(
      IRECT(knobX[3], 118, knobX[3] + 76, 210), kParamDenoiseMix,
      "MIX", technicalStyle), kNoTag, "formal");

    graphics->AttachControl(new IVToggleControl(
      IRECT(800, 225, 930, 255), kParamCompressorEnabled, "COMPRESSOR", technicalCompactStyle),
      kNoTag, "formal");
    graphics->AttachControl(new IVToggleControl(
      IRECT(940, 225, 1060, 255), kParamCompressorAutoGain, "AUTO GAIN", technicalCompactStyle),
      kNoTag, "formal");
    const std::array<int, 7> compressorParams {
      kParamCompressorThreshold, kParamCompressorRatio, kParamCompressorAttack,
      kParamCompressorRelease, kParamCompressorKnee, kParamCompressorMakeup,
      kParamCompressorMix
    };
    const std::array<const char*, 7> compressorLabels {
      "THRESH", "RATIO", "ATTACK", "RELEASE", "KNEE", "MAKEUP", "MIX"
    };
    for (int i = 0; i < 7; ++i) {
      graphics->AttachControl(new IVKnobControl(
        IRECT(denseKnobX[i], 260, denseKnobX[i] + 48, 350), compressorParams[i],
        compressorLabels[i], technicalCompactStyle), kNoTag, "formal");
    }

    graphics->AttachControl(new IVToggleControl(
      IRECT(800, 365, 950, 395), kParamReverbEnabled, "REVERB", technicalStyle),
      kNoTag, "formal");
    const std::array<int, 7> reverbParams {
      kParamReverbPreDelay, kParamReverbDecay, kParamReverbSize,
      kParamReverbDamping, kParamReverbDiffusion, kParamReverbWidth,
      kParamReverbMix
    };
    const std::array<const char*, 7> reverbLabels {
      "PRE", "DECAY", "SIZE", "DAMP", "DIFF", "WIDTH", "MIX"
    };
    for (int i = 0; i < 7; ++i) {
      graphics->AttachControl(new IVKnobControl(
        IRECT(denseKnobX[i], 400, denseKnobX[i] + 48, 490), reverbParams[i],
        reverbLabels[i], technicalCompactStyle), kNoTag, "formal");
    }

    graphics->AttachControl(new IVToggleControl(
      IRECT(800, 505, 915, 535), kParamLimiterEnabled, "LIMITER", technicalCompactStyle),
      kNoTag, "formal");
    graphics->AttachControl(new IVToggleControl(
      IRECT(920, 505, 1035, 535), kParamLimiterTruePeak, "TRUE PEAK", technicalCompactStyle),
      kNoTag, "formal");
    graphics->AttachControl(new IVMenuButtonControl(
      IRECT(1040, 505, 1155, 535), kParamLimiterOversampling, "OVERSAMPLE", technicalCompactStyle),
      kNoTag, "formal");
    graphics->AttachControl(new IVKnobControl(
      IRECT(knobX[0], 540, knobX[0] + 76, 630), kParamLimiterCeiling,
      "CEILING", technicalStyle), kNoTag, "formal");
    graphics->AttachControl(new IVKnobControl(
      IRECT(knobX[1], 540, knobX[1] + 76, 630), kParamLimiterInputGain,
      "DRIVE", technicalStyle), kNoTag, "formal");
    graphics->AttachControl(new IVKnobControl(
      IRECT(knobX[2], 540, knobX[2] + 76, 630), kParamLimiterRelease,
      "RELEASE", technicalStyle), kNoTag, "formal");
    graphics->AttachControl(new IVKnobControl(
      IRECT(knobX[3], 540, knobX[3] + 76, 630), kParamLimiterLookahead,
      "LOOK", technicalStyle), kNoTag, "formal");

    graphics->AttachControl(new IVKnobControl(
      IRECT(800, 642, 880, 712), kParamInputGain, "INPUT", technicalStyle),
      kNoTag, "formal");
    graphics->AttachControl(new IVToggleControl(
      IRECT(900, 658, 1040, 700), kParamLoudnessEnabled, "LOUDNESS", technicalStyle),
      kNoTag, "formal");
    graphics->AttachControl(new IVKnobControl(
      IRECT(1070, 642, 1150, 712), kParamOutputGain, "OUTPUT", technicalStyle),
      kNoTag, "formal");
    updateModeVisibility();
  };
#endif
}

void VaporwaveSuite::initializeParameters() {
  GetParam(kParamViewMode)->InitDouble("View Mode", 0.0, 0.0, 1.0, 0.001);
  GetParam(kParamViewMode)->SetDisplayText(0.0, "Simple");
  GetParam(kParamViewMode)->SetDisplayText(1.0, "Formal");
  GetParam(kParamMasterBypass)->InitBool("Master Bypass", false);
  GetParam(kParamInputGain)->InitDouble("Input Gain", 0.0, -24.0, 24.0, 0.1, "dB");
  GetParam(kParamOutputGain)->InitDouble("Output Gain", 0.0, -24.0, 24.0, 0.1, "dB");
  GetParam(kParamAutoClean)->InitBool("Auto Clean", false);
  GetParam(kParamLoudnessTarget)->InitEnum(
    "Loudness Target", 0, {"Streaming -14", "Podcast -16", "Broadcast -23"});
  GetParam(kParamEqPreset)->InitEnum(
    "EQ Character", 0, {"Flat", "Warm", "Bright", "Vocal", "Bass", "Air", "Custom"});
  GetParam(kParamReverbPreset)->InitEnum(
    "Space", 0, {"Dry", "Room", "Studio", "Plate", "Hall", "Dream", "Custom"});

  GetParam(kParamDenoiseEnabled)->InitBool("Denoise Enabled", false);
  GetParam(kParamDenoiseEngine)->InitEnum(
    "Denoise Engine", 0, {"Mint RT <10ms", "Live", "Voice Live", "Voice AI"});
  GetParam(kParamContentMode)->InitEnum("Content", 0, {"Auto", "Voice", "Music"});
  GetParam(kParamDenoiseReduction)->InitDouble(
    "Denoise Reduction", 12.0, 0.0, 30.0, 0.1, "dB");
  GetParam(kParamDenoiseSensitivity)->InitDouble(
    "Denoise Sensitivity", 55.0, 0.0, 100.0, 0.1, "%");
  GetParam(kParamDenoiseSmoothing)->InitDouble(
    "Artifact Smoothing", 65.0, 0.0, 99.0, 0.1, "%");
  GetParam(kParamDenoiseMix)->InitDouble("Denoise Mix", 100.0, 0.0, 100.0, 0.1, "%");
  GetParam(kParamNoiseLearn)->InitBool("Noise Learn", false);

  GetParam(kParamEqEnabled)->InitBool("EQ Enabled", true);
  for (int band = 0; band < 12; ++band) {
    GetParam(EqParam(band, kEqEnabled))->InitBool(
      bandParameterName(band, "Enabled"), false);
    GetParam(EqParam(band, kEqType))->InitEnum(
      bandParameterName(band, "Type"), 0,
      {"Bell", "Low Shelf", "High Shelf", "Low Pass", "High Pass", "Notch", "Band Pass"});
    GetParam(EqParam(band, kEqChannel))->InitEnum(
      bandParameterName(band, "Channel"), 0,
      {"Stereo", "Left", "Right", "Mid", "Side"});
    GetParam(EqParam(band, kEqFrequency))->InitDouble(
      bandParameterName(band, "Frequency"), 1000.0, 20.0, 20000.0, 0.1, "Hz", 0,
      "", IParam::ShapePowCurve(3.0));
    GetParam(EqParam(band, kEqGain))->InitDouble(
      bandParameterName(band, "Gain"), 0.0, -24.0, 24.0, 0.1, "dB");
    GetParam(EqParam(band, kEqQ))->InitDouble(
      bandParameterName(band, "Q"), 0.707, 0.1, 24.0, 0.001);
  }

  GetParam(kParamCompressorEnabled)->InitBool("Compressor Enabled", false);
  GetParam(kParamCompressorThreshold)->InitDouble("Compressor Threshold", -18, -60, 0, 0.1, "dB");
  GetParam(kParamCompressorRatio)->InitDouble("Compressor Ratio", 3, 1, 20, 0.1, ":1");
  GetParam(kParamCompressorAttack)->InitDouble("Compressor Attack", 15, 0.1, 200, 0.1, "ms");
  GetParam(kParamCompressorRelease)->InitDouble("Compressor Release", 120, 10, 2000, 1, "ms");
  GetParam(kParamCompressorKnee)->InitDouble("Compressor Knee", 6, 0, 24, 0.1, "dB");
  GetParam(kParamCompressorMakeup)->InitDouble("Compressor Makeup", 0, -12, 24, 0.1, "dB");
  GetParam(kParamCompressorAutoGain)->InitBool("Compressor Auto Gain", false);
  GetParam(kParamCompressorMix)->InitDouble("Compressor Mix", 100, 0, 100, 0.1, "%");

  GetParam(kParamReverbEnabled)->InitBool("Reverb Enabled", false);
  GetParam(kParamReverbPreDelay)->InitDouble("Reverb Pre-delay", 12, 0, 200, 0.1, "ms");
  GetParam(kParamReverbDecay)->InitDouble("Reverb Decay", 1.2, 0.1, 12, 0.01, "s");
  GetParam(kParamReverbSize)->InitDouble("Reverb Size", 45, 0, 100, 0.1, "%");
  GetParam(kParamReverbDamping)->InitDouble("Reverb Damping", 55, 0, 100, 0.1, "%");
  GetParam(kParamReverbDiffusion)->InitDouble("Reverb Diffusion", 70, 0, 100, 0.1, "%");
  GetParam(kParamReverbWidth)->InitDouble("Reverb Width", 100, 0, 200, 0.1, "%");
  GetParam(kParamReverbMix)->InitDouble("Reverb Mix", 0, 0, 100, 0.1, "%");
  GetParam(kParamLoudnessEnabled)->InitBool("Loudness Follow", false);

  GetParam(kParamLimiterEnabled)->InitBool("Limiter Enabled", false);
  GetParam(kParamLimiterCeiling)->InitDouble("Limiter Ceiling", -1, -12, 0, 0.1, "dBTP");
  GetParam(kParamLimiterInputGain)->InitDouble("Limiter Input Gain", 0, -12, 24, 0.1, "dB");
  GetParam(kParamLimiterRelease)->InitDouble("Limiter Release", 80, 10, 1000, 1, "ms");
  GetParam(kParamLimiterLookahead)->InitDouble("Limiter Lookahead", 2, 0.1, 4, 0.1, "ms");
  GetParam(kParamLimiterTruePeak)->InitBool("Limiter True Peak", true);
  GetParam(kParamLimiterOversampling)->InitEnum("Limiter Oversampling", 1, {"1x", "2x", "4x"});
  GetParam(kParamLanguage)->InitDouble("Interface Language", 0.0, 0.0, 1.0, 0.001);
  GetParam(kParamLanguage)->SetDisplayText(0.0, "English");
  GetParam(kParamLanguage)->SetDisplayText(1.0, "Chinese");
}

vaporwave::ParameterState VaporwaveSuite::parameterSnapshot() const noexcept {
  using namespace vaporwave;
  ParameterState p;
  p.viewMode = static_cast<ViewMode>(GetParam(kParamViewMode)->Int());
  p.masterBypass = GetParam(kParamMasterBypass)->Bool();
  p.inputGainDb = static_cast<float>(GetParam(kParamInputGain)->Value());
  p.outputGainDb = static_cast<float>(GetParam(kParamOutputGain)->Value());
  p.autoClean = GetParam(kParamAutoClean)->Bool();
  p.loudnessTarget = static_cast<LoudnessTarget>(GetParam(kParamLoudnessTarget)->Int());
  p.denoiseEnabled = GetParam(kParamDenoiseEnabled)->Bool();
  p.denoiseEngine = static_cast<DenoiseEngine>(GetParam(kParamDenoiseEngine)->Int());
  p.contentMode = static_cast<ContentMode>(GetParam(kParamContentMode)->Int());
  p.denoiseReductionDb = static_cast<float>(GetParam(kParamDenoiseReduction)->Value());
  p.denoiseSensitivity = static_cast<float>(GetParam(kParamDenoiseSensitivity)->Value() * 0.01);
  p.denoiseSmoothing = static_cast<float>(GetParam(kParamDenoiseSmoothing)->Value() * 0.01);
  p.denoiseMix = static_cast<float>(GetParam(kParamDenoiseMix)->Value() * 0.01);
  p.noiseLearn = GetParam(kParamNoiseLearn)->Bool();
  p.eqEnabled = GetParam(kParamEqEnabled)->Bool();
  for (int band = 0; band < 12; ++band) {
    auto& eq = p.eq[static_cast<std::size_t>(band)];
    eq.enabled = GetParam(EqParam(band, kEqEnabled))->Bool();
    eq.type = static_cast<FilterType>(GetParam(EqParam(band, kEqType))->Int());
    eq.channel = static_cast<ChannelMode>(GetParam(EqParam(band, kEqChannel))->Int());
    eq.frequencyHz = static_cast<float>(GetParam(EqParam(band, kEqFrequency))->Value());
    eq.gainDb = static_cast<float>(GetParam(EqParam(band, kEqGain))->Value());
    eq.q = static_cast<float>(GetParam(EqParam(band, kEqQ))->Value());
  }
  p.compressorEnabled = GetParam(kParamCompressorEnabled)->Bool();
  p.compressorThresholdDb = static_cast<float>(GetParam(kParamCompressorThreshold)->Value());
  p.compressorRatio = static_cast<float>(GetParam(kParamCompressorRatio)->Value());
  p.compressorAttackMs = static_cast<float>(GetParam(kParamCompressorAttack)->Value());
  p.compressorReleaseMs = static_cast<float>(GetParam(kParamCompressorRelease)->Value());
  p.compressorKneeDb = static_cast<float>(GetParam(kParamCompressorKnee)->Value());
  p.compressorMakeupDb = static_cast<float>(GetParam(kParamCompressorMakeup)->Value());
  p.compressorAutoGain = GetParam(kParamCompressorAutoGain)->Bool();
  p.compressorMix = static_cast<float>(GetParam(kParamCompressorMix)->Value() * 0.01);
  p.reverbEnabled = GetParam(kParamReverbEnabled)->Bool();
  p.reverbPreDelayMs = static_cast<float>(GetParam(kParamReverbPreDelay)->Value());
  p.reverbDecaySeconds = static_cast<float>(GetParam(kParamReverbDecay)->Value());
  p.reverbSize = static_cast<float>(GetParam(kParamReverbSize)->Value() * 0.01);
  p.reverbDamping = static_cast<float>(GetParam(kParamReverbDamping)->Value() * 0.01);
  p.reverbDiffusion = static_cast<float>(GetParam(kParamReverbDiffusion)->Value() * 0.01);
  p.reverbWidth = static_cast<float>(GetParam(kParamReverbWidth)->Value() * 0.01);
  p.reverbMix = static_cast<float>(GetParam(kParamReverbMix)->Value() * 0.01);
  p.loudnessEnabled = GetParam(kParamLoudnessEnabled)->Bool();
  p.limiterEnabled = GetParam(kParamLimiterEnabled)->Bool();
  p.limiterCeilingDb = static_cast<float>(GetParam(kParamLimiterCeiling)->Value());
  p.limiterInputGainDb = static_cast<float>(GetParam(kParamLimiterInputGain)->Value());
  p.limiterReleaseMs = static_cast<float>(GetParam(kParamLimiterRelease)->Value());
  p.limiterLookaheadMs = static_cast<float>(GetParam(kParamLimiterLookahead)->Value());
  p.limiterTruePeak = GetParam(kParamLimiterTruePeak)->Bool();
  p.limiterOversampling = 1 << GetParam(kParamLimiterOversampling)->Int();
  return p;
}

#if IPLUG_DSP
void VaporwaveSuite::OnReset() {
  scratchCapacity_ = std::max(GetBlockSize(), 4096);
  for (auto& buffer : inputScratch_) buffer.assign(static_cast<std::size_t>(scratchCapacity_), 0.0f);
  for (auto& buffer : outputScratch_) buffer.assign(static_cast<std::size_t>(scratchCapacity_), 0.0f);
  processor_.prepare(GetSampleRate(), scratchCapacity_, 2);
  processor_.setParameters(parameterSnapshot());
  lastLatency_ = processor_.latencySamples();
  SetLatency(lastLatency_);
}

void VaporwaveSuite::ProcessBlock(sample** inputs, sample** outputs, int nFrames) {
  processor_.setParameters(parameterSnapshot());
  const int inputChannels = std::max(1, NInChansConnected());
  const int outputChannels = std::max(1, NOutChansConnected());
  int offset = 0;
  while (offset < nFrames) {
    const int count = std::min(scratchCapacity_, nFrames - offset);
    for (int i = 0; i < count; ++i) {
      inputScratch_[0][static_cast<std::size_t>(i)] =
        static_cast<float>(inputs[0][offset + i]);
      inputScratch_[1][static_cast<std::size_t>(i)] = inputChannels > 1
        ? static_cast<float>(inputs[1][offset + i])
        : inputScratch_[0][static_cast<std::size_t>(i)];
    }
    const float* in[] {inputScratch_[0].data(), inputScratch_[1].data()};
    float* out[] {outputScratch_[0].data(), outputScratch_[1].data()};
    processor_.process(in, out, 2, count);
    const auto meters = processor_.meters();
    meterInputDb_.store(meters.inputPeakDb, std::memory_order_relaxed);
    meterOutputDb_.store(meters.outputPeakDb, std::memory_order_relaxed);
    meterLufs_.store(meters.momentaryLufs, std::memory_order_relaxed);
    meterReductionDb_.store(meters.denoiseReductionDb, std::memory_order_relaxed);
    for (int i = 0; i < count; ++i) {
      outputs[0][offset + i] = static_cast<sample>(outputScratch_[0][static_cast<std::size_t>(i)]);
      if (outputChannels > 1) {
        outputs[1][offset + i] =
          static_cast<sample>(outputScratch_[1][static_cast<std::size_t>(i)]);
      }
    }
    offset += count;
  }
}

void VaporwaveSuite::OnParamChange(int paramIdx, EParamSource, int) {
  if (paramIdx == kParamEqPreset && GetParam(kParamEqPreset)->Int() < 6) {
    applyEqPreset(GetParam(kParamEqPreset)->Int());
  } else if (paramIdx == kParamReverbPreset && GetParam(kParamReverbPreset)->Int() < 6) {
    applyReverbPreset(GetParam(kParamReverbPreset)->Int());
  }
}
#endif

void VaporwaveSuite::applyEqPreset(int preset) {
  for (int band = 0; band < 12; ++band) {
    GetParam(EqParam(band, kEqEnabled))->Set(0.0);
    GetParam(EqParam(band, kEqGain))->Set(0.0);
    GetParam(EqParam(band, kEqQ))->Set(0.707);
  }
  auto setBand = [&](int band, int type, double frequency, double gain, double q) {
    GetParam(EqParam(band, kEqEnabled))->Set(1.0);
    GetParam(EqParam(band, kEqType))->Set(static_cast<double>(type));
    GetParam(EqParam(band, kEqFrequency))->Set(frequency);
    GetParam(EqParam(band, kEqGain))->Set(gain);
    GetParam(EqParam(band, kEqQ))->Set(q);
  };
  switch (preset) {
    case 1:
      setBand(0, 1, 180, 2.0, 0.7); setBand(1, 2, 9000, -1.0, 0.7); break;
    case 2:
      setBand(0, 0, 3200, 1.5, 1.0); setBand(1, 2, 8500, 2.5, 0.7); break;
    case 3:
      setBand(0, 4, 75, 0.0, 0.7); setBand(1, 0, 280, -2.0, 1.2);
      setBand(2, 0, 3500, 2.0, 1.0); break;
    case 4:
      setBand(0, 1, 110, 3.0, 0.7); setBand(1, 0, 350, -1.5, 1.1); break;
    case 5:
      setBand(0, 2, 12000, 4.0, 0.6); break;
    default: break;
  }
}

void VaporwaveSuite::applyReverbPreset(int preset) {
  struct Preset { double pre; double decay; double size; double damping; double mix; };
  static constexpr std::array<Preset, 6> presets {{
    {0, 0.5, 30, 60, 0}, {8, 0.75, 34, 62, 18},
    {14, 1.3, 48, 55, 24}, {20, 2.0, 58, 35, 30},
    {28, 3.5, 84, 45, 38}, {45, 5.8, 100, 22, 48}
  }};
  const auto& value = presets[static_cast<std::size_t>(std::clamp(preset, 0, 5))];
  GetParam(kParamReverbPreDelay)->Set(value.pre);
  GetParam(kParamReverbDecay)->Set(value.decay);
  GetParam(kParamReverbSize)->Set(value.size);
  GetParam(kParamReverbDamping)->Set(value.damping);
  GetParam(kParamReverbMix)->Set(value.mix);
  GetParam(kParamReverbEnabled)->Set(preset == 0 ? 0.0 : 1.0);
}

#if IPLUG_EDITOR
void VaporwaveSuite::OnParamChangeUI(int paramIdx, EParamSource) {
  if (paramIdx == kParamLanguage) {
    languageRefreshPending_.store(true, std::memory_order_release);
  } else if (paramIdx == kParamViewMode) {
    updateModeVisibility();
  }
}

void VaporwaveSuite::OnIdle() {
  auto* graphics = GetUI();
  if (graphics == nullptr) return;
  if (languageRefreshPending_.exchange(false, std::memory_order_acq_rel)) {
    graphics->SetStrictDrawing(false);
    graphics->RemoveAllControls();
    mLayoutFunc(graphics);
    SendCurrentParamValuesFromDelegate();
    graphics->SetAllControlsDirty();
    fullRedrawFramesRemaining_ = 2;
  } else if (fullRedrawFramesRemaining_ > 0 && --fullRedrawFramesRemaining_ == 0) {
    graphics->SetStrictDrawing(true);
  }
  auto* meter = graphics->GetControlWithTag(kCtrlMeters);
  if (meter == nullptr) return;
  if (GetParam(kParamLanguage)->Bool()) {
    meter->As<ITextControl>()->SetStrFmt(
      160, "输入 %.1f   输出 %.1f   响度 %.1f   降噪 %.1f dB",
      meterInputDb_.load(std::memory_order_relaxed),
      meterOutputDb_.load(std::memory_order_relaxed),
      meterLufs_.load(std::memory_order_relaxed),
      meterReductionDb_.load(std::memory_order_relaxed));
  } else {
    meter->As<ITextControl>()->SetStrFmt(
      160, "IN  %.1f     OUT  %.1f     LUFS  %.1f     NR  %.1f dB",
      meterInputDb_.load(std::memory_order_relaxed),
      meterOutputDb_.load(std::memory_order_relaxed),
      meterLufs_.load(std::memory_order_relaxed),
      meterReductionDb_.load(std::memory_order_relaxed));
  }
}

void VaporwaveSuite::updateModeVisibility() {
  auto* graphics = GetUI();
  if (graphics == nullptr) return;
  const bool simple = GetParam(kParamViewMode)->Int() == 0;
  graphics->ForControlInGroup("simple", [simple](IControl* control) {
    control->Hide(!simple);
  });
  graphics->ForControlInGroup("formal", [simple](IControl* control) {
    control->Hide(simple);
  });
  graphics->SetAllControlsDirty();
}
#endif
