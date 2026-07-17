#pragma once

#include "vaporwave/Parameters.h"

#include <array>
#include <complex>
#include <cstddef>
#include <memory>
#include <vector>

namespace vaporwave {

struct MeterState {
  float inputPeakDb = -120.0f;
  float outputPeakDb = -120.0f;
  float momentaryLufs = -120.0f;
  float denoiseReductionDb = 0.0f;
  float compressorGainReductionDb = 0.0f;
  float limiterGainReductionDb = 0.0f;
  bool clipped = false;
};

class Processor {
public:
  Processor();
  ~Processor();
  Processor(Processor&&) noexcept;
  Processor& operator=(Processor&&) noexcept;
  Processor(const Processor&) = delete;
  Processor& operator=(const Processor&) = delete;

  void prepare(double sampleRate, int maxBlockSize, int channels);
  void reset() noexcept;
  void setParameters(const ParameterState& state) noexcept;
  [[nodiscard]] const ParameterState& parameters() const noexcept;
  [[nodiscard]] int latencySamples() const noexcept;
  [[nodiscard]] MeterState meters() const noexcept;

  void process(const float* const* inputs, float* const* outputs,
               int channels, int frames) noexcept;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace vaporwave

