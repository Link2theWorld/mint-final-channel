#include "vaporwave/Processor.h"
#include "vaporwave/SpectralDenoiser.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace vaporwave {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float dbToGain(float db) noexcept {
  return std::pow(10.0f, db / 20.0f);
}

float gainToDb(float gain) noexcept {
  return 20.0f * std::log10(std::max(gain, 1.0e-6f));
}

float timeCoefficient(float milliseconds, double sampleRate) noexcept {
  return std::exp(-1.0f /
    (std::max(milliseconds, 0.01f) * 0.001f * static_cast<float>(sampleRate)));
}

struct Biquad {
  float b0 = 1.0f;
  float b1 = 0.0f;
  float b2 = 0.0f;
  float a1 = 0.0f;
  float a2 = 0.0f;
  float z1 = 0.0f;
  float z2 = 0.0f;

  void reset() noexcept { z1 = z2 = 0.0f; }

  float process(float x) noexcept {
    const float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }

  void configure(const EqBand& band, double sampleRate) noexcept {
    const float frequency = std::clamp(
      band.frequencyHz, 10.0f, static_cast<float>(sampleRate * 0.49));
    const float q = std::clamp(band.q, 0.1f, 24.0f);
    const float omega = 2.0f * kPi * frequency / static_cast<float>(sampleRate);
    const float sine = std::sin(omega);
    const float cosine = std::cos(omega);
    const float alpha = sine / (2.0f * q);
    const float amplitude = std::pow(10.0f, band.gainDb / 40.0f);
    float nb0 = 1.0f;
    float nb1 = 0.0f;
    float nb2 = 0.0f;
    float na0 = 1.0f;
    float na1 = 0.0f;
    float na2 = 0.0f;

    switch (band.type) {
      case FilterType::LowPass:
        nb0 = (1.0f - cosine) * 0.5f;
        nb1 = 1.0f - cosine;
        nb2 = nb0;
        na0 = 1.0f + alpha;
        na1 = -2.0f * cosine;
        na2 = 1.0f - alpha;
        break;
      case FilterType::HighPass:
        nb0 = (1.0f + cosine) * 0.5f;
        nb1 = -(1.0f + cosine);
        nb2 = nb0;
        na0 = 1.0f + alpha;
        na1 = -2.0f * cosine;
        na2 = 1.0f - alpha;
        break;
      case FilterType::Notch:
        nb0 = 1.0f;
        nb1 = -2.0f * cosine;
        nb2 = 1.0f;
        na0 = 1.0f + alpha;
        na1 = -2.0f * cosine;
        na2 = 1.0f - alpha;
        break;
      case FilterType::BandPass:
        nb0 = alpha;
        nb1 = 0.0f;
        nb2 = -alpha;
        na0 = 1.0f + alpha;
        na1 = -2.0f * cosine;
        na2 = 1.0f - alpha;
        break;
      case FilterType::LowShelf: {
        const float twoSqrtAAlpha = 2.0f * std::sqrt(amplitude) * alpha;
        nb0 = amplitude * ((amplitude + 1.0f) -
              (amplitude - 1.0f) * cosine + twoSqrtAAlpha);
        nb1 = 2.0f * amplitude * ((amplitude - 1.0f) -
              (amplitude + 1.0f) * cosine);
        nb2 = amplitude * ((amplitude + 1.0f) -
              (amplitude - 1.0f) * cosine - twoSqrtAAlpha);
        na0 = (amplitude + 1.0f) + (amplitude - 1.0f) * cosine +
              twoSqrtAAlpha;
        na1 = -2.0f * ((amplitude - 1.0f) +
              (amplitude + 1.0f) * cosine);
        na2 = (amplitude + 1.0f) + (amplitude - 1.0f) * cosine -
              twoSqrtAAlpha;
        break;
      }
      case FilterType::HighShelf: {
        const float twoSqrtAAlpha = 2.0f * std::sqrt(amplitude) * alpha;
        nb0 = amplitude * ((amplitude + 1.0f) +
              (amplitude - 1.0f) * cosine + twoSqrtAAlpha);
        nb1 = -2.0f * amplitude * ((amplitude - 1.0f) +
              (amplitude + 1.0f) * cosine);
        nb2 = amplitude * ((amplitude + 1.0f) +
              (amplitude - 1.0f) * cosine - twoSqrtAAlpha);
        na0 = (amplitude + 1.0f) - (amplitude - 1.0f) * cosine +
              twoSqrtAAlpha;
        na1 = 2.0f * ((amplitude - 1.0f) -
              (amplitude + 1.0f) * cosine);
        na2 = (amplitude + 1.0f) - (amplitude - 1.0f) * cosine -
              twoSqrtAAlpha;
        break;
      }
      case FilterType::Bell:
      default:
        nb0 = 1.0f + alpha * amplitude;
        nb1 = -2.0f * cosine;
        nb2 = 1.0f - alpha * amplitude;
        na0 = 1.0f + alpha / amplitude;
        na1 = -2.0f * cosine;
        na2 = 1.0f - alpha / amplitude;
        break;
    }
    b0 = nb0 / na0;
    b1 = nb1 / na0;
    b2 = nb2 / na0;
    a1 = na1 / na0;
    a2 = na2 / na0;
  }
};

class StereoCompressor {
public:
  void prepare(double sampleRate) noexcept { sampleRate_ = sampleRate; reset(); }
  void reset() noexcept { envelope_ = 0.0f; gainReductionDb_ = 0.0f; }

  void process(float& left, float& right, const ParameterState& p) noexcept {
    const float detector = std::max(std::abs(left), std::abs(right));
    const float attack = timeCoefficient(p.compressorAttackMs, sampleRate_);
    const float release = timeCoefficient(p.compressorReleaseMs, sampleRate_);
    const float coefficient = detector > envelope_ ? attack : release;
    envelope_ = coefficient * envelope_ + (1.0f - coefficient) * detector;
    const float levelDb = gainToDb(envelope_);
    const float over = levelDb - p.compressorThresholdDb;
    const float knee = std::max(p.compressorKneeDb, 0.0f);
    float compressedOver = 0.0f;
    if (knee > 0.0f && over > -knee * 0.5f && over < knee * 0.5f) {
      const float x = over + knee * 0.5f;
      compressedOver = (1.0f - 1.0f / p.compressorRatio) *
                       x * x / (2.0f * knee);
    } else if (over >= knee * 0.5f) {
      compressedOver = over * (1.0f - 1.0f / p.compressorRatio);
    }
    const float autoMakeup = p.compressorAutoGain
      ? -p.compressorThresholdDb * (1.0f - 1.0f / p.compressorRatio) * 0.5f
      : 0.0f;
    const float gainDb = -compressedOver + p.compressorMakeupDb + autoMakeup;
    const float gain = dbToGain(gainDb);
    const float dryLeft = left;
    const float dryRight = right;
    left = dryLeft + (dryLeft * gain - dryLeft) * p.compressorMix;
    right = dryRight + (dryRight * gain - dryRight) * p.compressorMix;
    gainReductionDb_ = 0.95f * gainReductionDb_ + 0.05f * compressedOver;
  }

  float gainReductionDb() const noexcept { return gainReductionDb_; }

private:
  double sampleRate_ = 48000.0;
  float envelope_ = 0.0f;
  float gainReductionDb_ = 0.0f;
};

class FdnReverb {
public:
  void prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    const std::array<float, 8> timesMs { 31.1f, 37.7f, 41.1f, 43.7f,
                                        47.9f, 53.3f, 59.3f, 61.7f };
    for (std::size_t i = 0; i < lines_.size(); ++i) {
      const auto length = static_cast<std::size_t>(
        std::ceil(timesMs[i] * 0.001 * sampleRate_ * 1.9));
      lines_[i].assign(std::max<std::size_t>(length, 8), 0.0f);
      indices_[i] = 0;
      lowpass_[i] = 0.0f;
    }
    const auto maxPreDelay = static_cast<std::size_t>(sampleRate_ * 0.25);
    preDelayL_.assign(maxPreDelay + 1, 0.0f);
    preDelayR_.assign(maxPreDelay + 1, 0.0f);
    preIndex_ = 0;
  }

  void reset() noexcept {
    for (auto& line : lines_) std::fill(line.begin(), line.end(), 0.0f);
    std::fill(preDelayL_.begin(), preDelayL_.end(), 0.0f);
    std::fill(preDelayR_.begin(), preDelayR_.end(), 0.0f);
    indices_.fill(0);
    lowpass_.fill(0.0f);
    preIndex_ = 0;
  }

  void process(float& left, float& right, const ParameterState& p) noexcept {
    if (preDelayL_.empty()) return;
    const std::size_t preSamples = std::min<std::size_t>(
      static_cast<std::size_t>(p.reverbPreDelayMs * 0.001 * sampleRate_),
      preDelayL_.size() - 1);
    const std::size_t read = (preIndex_ + preDelayL_.size() - preSamples) %
                             preDelayL_.size();
    const float preL = preSamples == 0 ? left : preDelayL_[read];
    const float preR = preSamples == 0 ? right : preDelayR_[read];
    preDelayL_[preIndex_] = left;
    preDelayR_[preIndex_] = right;
    preIndex_ = (preIndex_ + 1) % preDelayL_.size();

    std::array<float, 8> taps {};
    float sum = 0.0f;
    const float sizeScale = 0.55f + 0.9f * std::clamp(p.reverbSize, 0.0f, 1.0f);
    for (std::size_t i = 0; i < lines_.size(); ++i) {
      auto& line = lines_[i];
      const auto effective = std::clamp<std::size_t>(
        static_cast<std::size_t>(line.size() * sizeScale), 2, line.size());
      indices_[i] %= effective;
      taps[i] = line[indices_[i]];
      lowpass_[i] += (1.0f - p.reverbDamping) * 0.25f *
                     (taps[i] - lowpass_[i]);
      taps[i] = lowpass_[i];
      sum += taps[i];
    }

    const float decay = std::clamp(
      std::pow(0.001f, 0.04f / std::max(p.reverbDecaySeconds, 0.1f)),
      0.0f, 0.995f);
    const float diffusion = std::clamp(p.reverbDiffusion, 0.0f, 1.0f);
    for (std::size_t i = 0; i < lines_.size(); ++i) {
      const float householder = taps[i] - diffusion * 0.25f * sum;
      const float input = (i & 1U) ? preR : preL;
      lines_[i][indices_[i]] = input * 0.22f + householder * decay;
      ++indices_[i];
    }

    const float wetL = (taps[0] + taps[2] + taps[5] + taps[7]) * 0.25f;
    const float wetR = (taps[1] + taps[3] + taps[4] + taps[6]) * 0.25f;
    const float mid = 0.5f * (wetL + wetR);
    const float side = 0.5f * (wetL - wetR) * p.reverbWidth;
    left += ((mid + side) - left) * p.reverbMix;
    right += ((mid - side) - right) * p.reverbMix;
  }

private:
  double sampleRate_ = 48000.0;
  std::array<std::vector<float>, 8> lines_;
  std::array<std::size_t, 8> indices_ {};
  std::array<float, 8> lowpass_ {};
  std::vector<float> preDelayL_;
  std::vector<float> preDelayR_;
  std::size_t preIndex_ = 0;
};

class LoudnessFollower {
public:
  void prepare(double sampleRate) noexcept { sampleRate_ = sampleRate; reset(); }
  void reset() noexcept { power_ = 1.0e-9f; gainDb_ = 0.0f; lufs_ = -120.0f; }

  void process(float& left, float& right, const ParameterState& p) noexcept {
    const float monoPower = 0.5f * (left * left + right * right);
    const float meterCoeff = std::exp(-1.0f / static_cast<float>(sampleRate_ * 3.0));
    power_ = meterCoeff * power_ + (1.0f - meterCoeff) * monoPower;
    lufs_ = -0.691f + 10.0f * std::log10(std::max(power_, 1.0e-12f));
    const float desired = std::clamp(targetLufs(p.loudnessTarget) - lufs_,
                                     -12.0f, 12.0f);
    const float maxStep = 1.0f / static_cast<float>(sampleRate_);
    gainDb_ += std::clamp(desired - gainDb_, -maxStep, maxStep);
    const float gain = dbToGain(gainDb_);
    left *= gain;
    right *= gain;
  }

  float lufs() const noexcept { return lufs_; }

private:
  double sampleRate_ = 48000.0;
  float power_ = 1.0e-9f;
  float gainDb_ = 0.0f;
  float lufs_ = -120.0f;
};

class LookaheadLimiter {
public:
  void prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    const auto capacity = static_cast<std::size_t>(sampleRate_ * 0.1) + 8;
    delayL_.assign(capacity, 0.0f);
    delayR_.assign(capacity, 0.0f);
    reset();
  }

  void reset() noexcept {
    std::fill(delayL_.begin(), delayL_.end(), 0.0f);
    std::fill(delayR_.begin(), delayR_.end(), 0.0f);
    write_ = 0;
    gain_ = 1.0f;
    previousL_ = previousR_ = 0.0f;
    gainReductionDb_ = 0.0f;
  }

  void process(float& left, float& right, const ParameterState& p) noexcept {
    if (delayL_.empty()) return;
    const std::size_t lookahead = std::clamp<std::size_t>(
      static_cast<std::size_t>(p.limiterLookaheadMs * 0.001 * sampleRate_),
      1, delayL_.size() - 1);
    const std::size_t read = (write_ + delayL_.size() - lookahead) %
                             delayL_.size();
    const float delayedL = delayL_[read];
    const float delayedR = delayR_[read];
    delayL_[write_] = left;
    delayR_[write_] = right;
    write_ = (write_ + 1) % delayL_.size();

    float peak = std::max(std::abs(left), std::abs(right));
    if (p.limiterTruePeak) {
      peak = std::max(peak, std::abs(0.5f * (left + previousL_)));
      peak = std::max(peak, std::abs(0.5f * (right + previousR_)));
    }
    previousL_ = left;
    previousR_ = right;
    const float ceiling = dbToGain(p.limiterCeilingDb);
    const float desired = peak > ceiling ? ceiling / peak : 1.0f;
    const float release = timeCoefficient(p.limiterReleaseMs, sampleRate_);
    gain_ = desired < gain_ ? desired : release * gain_ + (1.0f - release);
    left = std::clamp(delayedL * gain_, -ceiling, ceiling);
    right = std::clamp(delayedR * gain_, -ceiling, ceiling);
    gainReductionDb_ = -gainToDb(gain_);
  }

  float gainReductionDb() const noexcept { return gainReductionDb_; }

private:
  double sampleRate_ = 48000.0;
  std::vector<float> delayL_;
  std::vector<float> delayR_;
  std::size_t write_ = 0;
  float gain_ = 1.0f;
  float previousL_ = 0.0f;
  float previousR_ = 0.0f;
  float gainReductionDb_ = 0.0f;
};

} // namespace

class Processor::Impl {
public:
  void prepare(double sampleRate, int maxBlockSize, int channels) {
    sampleRate_ = std::max(sampleRate, 8000.0);
    maxBlockSize_ = std::max(maxBlockSize, 1);
    channels_ = std::clamp(channels, 1, 2);
    for (auto& denoiser : denoisers_) denoiser.prepare(sampleRate_);
    compressor_.prepare(sampleRate_);
    reverb_.prepare(sampleRate_);
    loudness_.prepare(sampleRate_);
    limiter_.prepare(sampleRate_);
    reset();
  }

  void reset() noexcept {
    for (auto& denoiser : denoisers_) denoiser.reset();
    for (auto& channel : eq_) {
      for (auto& band : channel) band.reset();
    }
    compressor_.reset();
    reverb_.reset();
    loudness_.reset();
    limiter_.reset();
    meters_ = {};
  }

  void setParameters(const ParameterState& state) noexcept { params_ = state; }

  void process(const float* const* inputs, float* const* outputs,
               int channels, int frames) noexcept {
    if (inputs == nullptr || outputs == nullptr || frames <= 0) return;
    const int usedChannels = std::clamp(channels, 1, channels_);
    configureEq();
    for (auto& denoiser : denoisers_) {
      denoiser.setParameters(params_.denoiseReductionDb,
        params_.denoiseSensitivity, params_.denoiseSmoothing,
        params_.denoiseMix, params_.noiseLearn);
    }

    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
    const float inputGain = dbToGain(params_.inputGainDb);
    const float outputGain = dbToGain(params_.outputGainDb);
    const float limiterInput = dbToGain(params_.limiterInputGainDb);

    for (int sample = 0; sample < frames; ++sample) {
      float left = inputs[0] != nullptr ? inputs[0][sample] : 0.0f;
      float right = usedChannels > 1 && inputs[1] != nullptr
        ? inputs[1][sample] : left;
      inputPeak = std::max({inputPeak, std::abs(left), std::abs(right)});

      if (!params_.masterBypass) {
        left *= inputGain;
        right *= inputGain;
        if (params_.denoiseEnabled || params_.autoClean) {
          left = denoisers_[0].process(left);
          right = denoisers_[1].process(right);
        }
        if (params_.eqEnabled) processEq(left, right);
        if (params_.compressorEnabled) compressor_.process(left, right, params_);
        if (params_.reverbEnabled) reverb_.process(left, right, params_);
        if (params_.loudnessEnabled || params_.autoClean) {
          loudness_.process(left, right, params_);
        }
        left *= limiterInput;
        right *= limiterInput;
        if (params_.limiterEnabled || params_.autoClean) {
          limiter_.process(left, right, params_);
        }
        left *= outputGain;
        right *= outputGain;
        if (params_.limiterEnabled || params_.autoClean) {
          const float ceiling = dbToGain(params_.limiterCeilingDb);
          left = std::clamp(left, -ceiling, ceiling);
          right = std::clamp(right, -ceiling, ceiling);
        }
      }

      if (outputs[0] != nullptr) outputs[0][sample] = left;
      if (usedChannels > 1 && outputs[1] != nullptr) outputs[1][sample] = right;
      outputPeak = std::max({outputPeak, std::abs(left), std::abs(right)});
    }

    meters_.inputPeakDb = gainToDb(inputPeak);
    meters_.outputPeakDb = gainToDb(outputPeak);
    meters_.momentaryLufs = loudness_.lufs();
    meters_.denoiseReductionDb = 0.5f *
      (denoisers_[0].reductionMeterDb() + denoisers_[1].reductionMeterDb());
    meters_.compressorGainReductionDb = compressor_.gainReductionDb();
    meters_.limiterGainReductionDb = limiter_.gainReductionDb();
    meters_.clipped = outputPeak > 1.0f;
  }

  void configureEq() noexcept {
    for (std::size_t band = 0; band < ParameterState::kEqBands; ++band) {
      eq_[0][band].configure(params_.eq[band], sampleRate_);
      eq_[1][band].configure(params_.eq[band], sampleRate_);
    }
  }

  void processEq(float& left, float& right) noexcept {
    constexpr float invSqrt2 = 0.70710678118f;
    for (std::size_t band = 0; band < ParameterState::kEqBands; ++band) {
      const auto& setting = params_.eq[band];
      if (!setting.enabled) continue;
      switch (setting.channel) {
        case ChannelMode::Left:
          left = eq_[0][band].process(left);
          break;
        case ChannelMode::Right:
          right = eq_[1][band].process(right);
          break;
        case ChannelMode::Mid: {
          float mid = (left + right) * invSqrt2;
          const float side = (left - right) * invSqrt2;
          mid = eq_[0][band].process(mid);
          left = (mid + side) * invSqrt2;
          right = (mid - side) * invSqrt2;
          break;
        }
        case ChannelMode::Side: {
          const float mid = (left + right) * invSqrt2;
          float side = (left - right) * invSqrt2;
          side = eq_[1][band].process(side);
          left = (mid + side) * invSqrt2;
          right = (mid - side) * invSqrt2;
          break;
        }
        case ChannelMode::Stereo:
        default:
          left = eq_[0][band].process(left);
          right = eq_[1][band].process(right);
          break;
      }
    }
  }

  double sampleRate_ = 48000.0;
  int maxBlockSize_ = 512;
  int channels_ = 2;
  ParameterState params_;
  MeterState meters_;
  std::array<SpectralDenoiser, 2> denoisers_;
  std::array<std::array<Biquad, ParameterState::kEqBands>, 2> eq_;
  StereoCompressor compressor_;
  FdnReverb reverb_;
  LoudnessFollower loudness_;
  LookaheadLimiter limiter_;
};

Processor::Processor() : impl_(std::make_unique<Impl>()) {}
Processor::~Processor() = default;
Processor::Processor(Processor&&) noexcept = default;
Processor& Processor::operator=(Processor&&) noexcept = default;

void Processor::prepare(double sampleRate, int maxBlockSize, int channels) {
  impl_->prepare(sampleRate, maxBlockSize, channels);
}

void Processor::reset() noexcept { impl_->reset(); }

void Processor::setParameters(const ParameterState& state) noexcept {
  impl_->setParameters(state);
}

const ParameterState& Processor::parameters() const noexcept {
  return impl_->params_;
}

int Processor::latencySamples() const noexcept {
  const auto& p = impl_->params_;
  if (p.masterBypass) return 0;
  const int denoiseLatency = (p.denoiseEnabled || p.autoClean)
    ? SpectralDenoiser::kFftSize : 0;
  const int limiterLatency = (p.limiterEnabled || p.autoClean)
    ? static_cast<int>(p.limiterLookaheadMs * 0.001 * impl_->sampleRate_) : 0;
  return denoiseLatency + limiterLatency;
}

MeterState Processor::meters() const noexcept { return impl_->meters_; }

void Processor::process(const float* const* inputs, float* const* outputs,
                        int channels, int frames) noexcept {
  impl_->process(inputs, outputs, channels, frames);
}

} // namespace vaporwave
