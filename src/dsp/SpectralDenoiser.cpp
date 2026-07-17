#include "vaporwave/SpectralDenoiser.h"

#include <algorithm>
#include <cmath>

namespace vaporwave {
namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 1.0e-12f;
}

void SpectralDenoiser::prepare(double sampleRate) noexcept {
  sampleRate_ = sampleRate;
  for (int i = 0; i < kFftSize; ++i) {
    const float phase = (2.0f * kPi * static_cast<float>(i)) /
                        static_cast<float>(kFftSize);
    window_[static_cast<std::size_t>(i)] = std::sqrt(0.5f - 0.5f * std::cos(phase));
  }
  reset();
}

void SpectralDenoiser::reset() noexcept {
  frame_.fill(0.0f);
  hop_.fill(0.0f);
  overlap_.fill(0.0f);
  outputFifo_.fill(0.0f);
  dryDelay_.fill(0.0f);
  spectrum_.fill({});
  noisePower_.fill(1.0e-6f);
  smoothedGain_.fill(1.0f);
  hopWrite_ = 0;
  outputRead_ = 0;
  outputWrite_ = 0;
  outputCount_ = 0;
  dryWrite_ = 0;
  meterReductionDb_ = 0.0f;
  primed_ = false;
}

void SpectralDenoiser::setParameters(float reductionDb, float sensitivity,
                                     float smoothing, float mix,
                                     bool learn) noexcept {
  reductionDb_ = std::clamp(reductionDb, 0.0f, 30.0f);
  sensitivity_ = std::clamp(sensitivity, 0.0f, 1.0f);
  smoothing_ = std::clamp(smoothing, 0.0f, 0.99f);
  mix_ = std::clamp(mix, 0.0f, 1.0f);
  learn_ = learn;
}

float SpectralDenoiser::process(float input) noexcept {
  const float delayedDry = dryDelay_[static_cast<std::size_t>(dryWrite_)];
  dryDelay_[static_cast<std::size_t>(dryWrite_)] = input;
  dryWrite_ = (dryWrite_ + 1) % kFftSize;

  hop_[static_cast<std::size_t>(hopWrite_++)] = input;
  if (hopWrite_ == kHopSize) {
    std::move(frame_.begin() + kHopSize, frame_.end(), frame_.begin());
    std::copy(hop_.begin(), hop_.end(), frame_.end() - kHopSize);
    hopWrite_ = 0;
    processFrame();
    primed_ = true;
  }

  float wet = 0.0f;
  if (outputCount_ > 0) {
    wet = outputFifo_[static_cast<std::size_t>(outputRead_)];
    outputRead_ = (outputRead_ + 1) % static_cast<int>(outputFifo_.size());
    --outputCount_;
  }

  // Both paths carry the same declared latency so a mix change is phase safe.
  return primed_ ? delayedDry + (wet - delayedDry) * mix_ : 0.0f;
}

float SpectralDenoiser::reductionMeterDb() const noexcept {
  return meterReductionDb_;
}

void SpectralDenoiser::processFrame() noexcept {
  for (int i = 0; i < kFftSize; ++i) {
    spectrum_[static_cast<std::size_t>(i)] = {
      frame_[static_cast<std::size_t>(i)] * window_[static_cast<std::size_t>(i)],
      0.0f
    };
  }
  fft(spectrum_, false);

  const float floorGain = std::pow(10.0f, -reductionDb_ / 20.0f);
  const float noiseRise = 0.985f + 0.014f * (1.0f - sensitivity_);
  const float noiseFall = 0.80f + 0.18f * (1.0f - sensitivity_);
  float gainSum = 0.0f;

  for (int bin = 0; bin <= kFftSize / 2; ++bin) {
    const auto index = static_cast<std::size_t>(bin);
    const float power = std::norm(spectrum_[index]) + kEpsilon;
    const float oldNoise = noisePower_[index];
    const bool likelyNoise = learn_ || power < oldNoise * (1.5f + 6.0f * sensitivity_);
    const float coefficient = likelyNoise ? noiseFall : noiseRise;
    noisePower_[index] = coefficient * oldNoise + (1.0f - coefficient) * power;

    const float overSubtraction = 1.0f + 2.5f * sensitivity_;
    const float raw = std::clamp(
      1.0f - overSubtraction * noisePower_[index] / power, floorGain, 1.0f);
    const float gain = smoothing_ * smoothedGain_[index] +
                       (1.0f - smoothing_) * raw;
    smoothedGain_[index] = gain;
    spectrum_[index] *= gain;
    if (bin > 0 && bin < kFftSize / 2) {
      spectrum_[static_cast<std::size_t>(kFftSize - bin)] =
        std::conj(spectrum_[index]);
    }
    gainSum += gain;
  }

  fft(spectrum_, true);
  for (int i = 0; i < kFftSize; ++i) {
    overlap_[static_cast<std::size_t>(i)] +=
      spectrum_[static_cast<std::size_t>(i)].real() *
      window_[static_cast<std::size_t>(i)];
  }

  for (int i = 0; i < kHopSize; ++i) {
    if (outputCount_ < static_cast<int>(outputFifo_.size())) {
      // Store the wet signal. The frame input is also placed kFftSize ahead
      // in the same fixed ring to provide a latency-aligned dry reference.
      outputFifo_[static_cast<std::size_t>(outputWrite_)] =
        overlap_[static_cast<std::size_t>(i)];
      outputWrite_ = (outputWrite_ + 1) % static_cast<int>(outputFifo_.size());
      ++outputCount_;
    }
  }
  std::move(overlap_.begin() + kHopSize, overlap_.end(), overlap_.begin());
  std::fill(overlap_.end() - kHopSize, overlap_.end(), 0.0f);

  const float meanGain = gainSum / static_cast<float>(kFftSize / 2 + 1);
  const float reduction = -20.0f * std::log10(std::max(meanGain, 1.0e-5f));
  meterReductionDb_ = 0.9f * meterReductionDb_ + 0.1f * reduction;
}

void SpectralDenoiser::fft(
    std::array<std::complex<float>, kFftSize>& data, bool inverse) noexcept {
  for (int i = 1, j = 0; i < kFftSize; ++i) {
    int bit = kFftSize >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      std::swap(data[static_cast<std::size_t>(i)],
                data[static_cast<std::size_t>(j)]);
    }
  }

  for (int length = 2; length <= kFftSize; length <<= 1) {
    const float angle = (inverse ? 2.0f : -2.0f) * kPi /
                        static_cast<float>(length);
    const std::complex<float> root(std::cos(angle), std::sin(angle));
    for (int i = 0; i < kFftSize; i += length) {
      std::complex<float> weight(1.0f, 0.0f);
      const int half = length / 2;
      for (int j = 0; j < half; ++j) {
        const auto even = data[static_cast<std::size_t>(i + j)];
        const auto odd = data[static_cast<std::size_t>(i + j + half)] * weight;
        data[static_cast<std::size_t>(i + j)] = even + odd;
        data[static_cast<std::size_t>(i + j + half)] = even - odd;
        weight *= root;
      }
    }
  }
  if (inverse) {
    for (auto& value : data) {
      value /= static_cast<float>(kFftSize);
    }
  }
}

} // namespace vaporwave
