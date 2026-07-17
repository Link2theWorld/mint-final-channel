#pragma once

#include <array>
#include <complex>
#include <cstddef>

namespace vaporwave {

class SpectralDenoiser {
public:
  // 5.33 ms algorithmic latency at 48 kHz, 5.80 ms at 44.1 kHz.
  static constexpr int kFftSize = 256;
  static constexpr int kHopSize = 128;

  void prepare(double sampleRate) noexcept;
  void reset() noexcept;
  void setParameters(float reductionDb, float sensitivity,
                     float smoothing, float mix, bool learn) noexcept;
  [[nodiscard]] float process(float input) noexcept;
  [[nodiscard]] float reductionMeterDb() const noexcept;
  [[nodiscard]] int latencySamples() const noexcept { return kFftSize; }

private:
  void processFrame() noexcept;
  void fft(std::array<std::complex<float>, kFftSize>& data,
           bool inverse) noexcept;

  double sampleRate_ = 48000.0;
  float reductionDb_ = 12.0f;
  float sensitivity_ = 0.55f;
  float smoothing_ = 0.65f;
  float mix_ = 1.0f;
  bool learn_ = false;
  bool primed_ = false;
  int hopWrite_ = 0;
  int outputRead_ = 0;
  int outputWrite_ = 0;
  int outputCount_ = 0;
  int dryWrite_ = 0;
  float meterReductionDb_ = 0.0f;

  std::array<float, kFftSize> frame_ {};
  std::array<float, kHopSize> hop_ {};
  std::array<float, kFftSize> overlap_ {};
  std::array<float, kFftSize * 2> outputFifo_ {};
  std::array<float, kFftSize> dryDelay_ {};
  std::array<float, kFftSize> window_ {};
  std::array<float, kFftSize / 2 + 1> noisePower_ {};
  std::array<float, kFftSize / 2 + 1> smoothedGain_ {};
  std::array<std::complex<float>, kFftSize> spectrum_ {};
};

} // namespace vaporwave
