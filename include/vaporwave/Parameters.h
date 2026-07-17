#pragma once

#include <array>
#include <cstdint>

namespace vaporwave {

enum class ViewMode : std::uint8_t { Simple = 0, Formal = 1 };
enum class DenoiseEngine : std::uint8_t {
  Auto = 0,
  Live = 1,
  VoiceLive = 2,
  VoiceAI = 3
};
enum class ContentMode : std::uint8_t { Auto = 0, Voice = 1, Music = 2 };
enum class LoudnessTarget : std::uint8_t {
  Streaming = 0,
  Podcast = 1,
  Broadcast = 2
};
enum class FilterType : std::uint8_t {
  Bell = 0,
  LowShelf = 1,
  HighShelf = 2,
  LowPass = 3,
  HighPass = 4,
  Notch = 5,
  BandPass = 6
};
enum class ChannelMode : std::uint8_t {
  Stereo = 0,
  Left = 1,
  Right = 2,
  Mid = 3,
  Side = 4
};

struct EqBand {
  bool enabled = false;
  FilterType type = FilterType::Bell;
  ChannelMode channel = ChannelMode::Stereo;
  float frequencyHz = 1000.0f;
  float gainDb = 0.0f;
  float q = 0.707f;
};

struct ParameterState {
  static constexpr std::uint32_t kStateVersion = 1;
  static constexpr std::size_t kEqBands = 12;

  std::uint32_t stateVersion = kStateVersion;
  ViewMode viewMode = ViewMode::Simple;
  bool masterBypass = false;
  float inputGainDb = 0.0f;
  float outputGainDb = 0.0f;

  bool autoClean = false;
  bool denoiseEnabled = true;
  DenoiseEngine denoiseEngine = DenoiseEngine::Auto;
  ContentMode contentMode = ContentMode::Auto;
  float denoiseReductionDb = 12.0f;
  float denoiseSensitivity = 0.55f;
  float denoiseSmoothing = 0.65f;
  float denoiseMix = 1.0f;
  bool noiseLearn = false;

  bool eqEnabled = true;
  std::array<EqBand, kEqBands> eq {};

  bool compressorEnabled = false;
  float compressorThresholdDb = -18.0f;
  float compressorRatio = 3.0f;
  float compressorAttackMs = 15.0f;
  float compressorReleaseMs = 120.0f;
  float compressorKneeDb = 6.0f;
  float compressorMakeupDb = 0.0f;
  bool compressorAutoGain = false;
  float compressorSidechainHpfHz = 80.0f;
  float compressorMix = 1.0f;

  bool reverbEnabled = true;
  float reverbPreDelayMs = 12.0f;
  float reverbDecaySeconds = 1.2f;
  float reverbSize = 0.45f;
  float reverbDamping = 0.55f;
  float reverbDiffusion = 0.7f;
  float reverbWidth = 1.0f;
  float reverbLowCutHz = 120.0f;
  float reverbHighCutHz = 12000.0f;
  float reverbMix = 0.12f;

  bool loudnessEnabled = false;
  LoudnessTarget loudnessTarget = LoudnessTarget::Streaming;

  bool limiterEnabled = true;
  float limiterCeilingDb = -1.0f;
  float limiterInputGainDb = 0.0f;
  float limiterReleaseMs = 80.0f;
  float limiterLookaheadMs = 2.0f;
  bool limiterTruePeak = true;
  int limiterOversampling = 2;
};

inline constexpr float targetLufs(LoudnessTarget target) noexcept {
  switch (target) {
    case LoudnessTarget::Podcast: return -16.0f;
    case LoudnessTarget::Broadcast: return -23.0f;
    case LoudnessTarget::Streaming:
    default: return -14.0f;
  }
}

namespace param_ids {
inline constexpr int kViewMode = 0;
inline constexpr int kMasterBypass = 1;
inline constexpr int kInputGain = 2;
inline constexpr int kOutputGain = 3;
inline constexpr int kAutoClean = 10;
inline constexpr int kDenoiseEnabled = 20;
inline constexpr int kDenoiseEngine = 21;
inline constexpr int kDenoiseReduction = 22;
inline constexpr int kDenoiseSensitivity = 23;
inline constexpr int kDenoiseSmoothing = 24;
inline constexpr int kDenoiseMix = 25;
inline constexpr int kContentMode = 26;
inline constexpr int kNoiseLearn = 27;
inline constexpr int kEqEnabled = 100;
inline constexpr int kEqBandBase = 120;
inline constexpr int kEqBandStride = 8;
inline constexpr int kCompressorEnabled = 300;
inline constexpr int kReverbEnabled = 400;
inline constexpr int kLoudnessEnabled = 500;
inline constexpr int kLoudnessTarget = 501;
inline constexpr int kLimiterEnabled = 600;
inline constexpr int kLimiterCeiling = 601;
} // namespace param_ids

} // namespace vaporwave
