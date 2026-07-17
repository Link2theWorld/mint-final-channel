#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "vaporwave/Processor.h"

#include <array>
#include <atomic>
#include <vector>

enum EParams {
  kParamViewMode = 0,
  kParamMasterBypass,
  kParamInputGain,
  kParamOutputGain,
  kParamAutoClean,
  kParamLoudnessTarget,
  kParamEqPreset,
  kParamReverbPreset,

  kParamDenoiseEnabled,
  kParamDenoiseEngine,
  kParamContentMode,
  kParamDenoiseReduction,
  kParamDenoiseSensitivity,
  kParamDenoiseSmoothing,
  kParamDenoiseMix,
  kParamNoiseLearn,

  kParamEqEnabled,
  kParamEqBandBase,
  kParamEqBandEnd = kParamEqBandBase + 12 * 6,

  kParamCompressorEnabled = kParamEqBandEnd,
  kParamCompressorThreshold,
  kParamCompressorRatio,
  kParamCompressorAttack,
  kParamCompressorRelease,
  kParamCompressorKnee,
  kParamCompressorMakeup,
  kParamCompressorAutoGain,
  kParamCompressorMix,

  kParamReverbEnabled,
  kParamReverbPreDelay,
  kParamReverbDecay,
  kParamReverbSize,
  kParamReverbDamping,
  kParamReverbDiffusion,
  kParamReverbWidth,
  kParamReverbMix,

  kParamLoudnessEnabled,

  kParamLimiterEnabled,
  kParamLimiterCeiling,
  kParamLimiterInputGain,
  kParamLimiterRelease,
  kParamLimiterLookahead,
  kParamLimiterTruePeak,
  kParamLimiterOversampling,
  kParamLanguage,
  kNumParams
};

inline constexpr int EqParam(int band, int field) {
  return kParamEqBandBase + band * 6 + field;
}

enum EEqParamField {
  kEqEnabled = 0,
  kEqType,
  kEqChannel,
  kEqFrequency,
  kEqGain,
  kEqQ
};

enum EControlTags {
  kCtrlTitle = 1000,
  kCtrlMode,
  kCtrlMeters
};

using namespace iplug;
using namespace igraphics;

class VaporwaveSuite final : public Plugin {
public:
  explicit VaporwaveSuite(const InstanceInfo& info);

#if IPLUG_DSP
  void OnReset() override;
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnParamChange(int paramIdx, EParamSource source, int sampleOffset) override;
#endif

#if IPLUG_EDITOR
  bool OnHostRequestingSupportedViewConfiguration(int width, int height) override {
    return width >= PLUG_MIN_WIDTH && height >= PLUG_MIN_HEIGHT;
  }
  void OnParamChangeUI(int paramIdx, EParamSource source) override;
  void OnIdle() override;
#endif

private:
  void initializeParameters();
  vaporwave::ParameterState parameterSnapshot() const noexcept;
  void applyEqPreset(int preset);
  void applyReverbPreset(int preset);
#if IPLUG_EDITOR
  void updateModeVisibility();
#endif

  vaporwave::Processor processor_;
  std::array<std::vector<float>, 2> inputScratch_;
  std::array<std::vector<float>, 2> outputScratch_;
  int scratchCapacity_ = 0;
  int lastLatency_ = -1;
  std::atomic<float> meterInputDb_ {-120.0f};
  std::atomic<float> meterOutputDb_ {-120.0f};
  std::atomic<float> meterLufs_ {-120.0f};
  std::atomic<float> meterReductionDb_ {0.0f};
  std::atomic<bool> languageRefreshPending_ {false};
  int fullRedrawFramesRemaining_ = 0;
};
