#include "vaporwave/Processor.h"
#include "vaporwave/SpectralDenoiser.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

struct StereoBuffer {
  explicit StereoBuffer(int frames)
    : left(static_cast<std::size_t>(frames)),
      right(static_cast<std::size_t>(frames)),
      outLeft(static_cast<std::size_t>(frames)),
      outRight(static_cast<std::size_t>(frames)) {}

  std::vector<float> left;
  std::vector<float> right;
  std::vector<float> outLeft;
  std::vector<float> outRight;

  void process(vaporwave::Processor& processor) {
    const float* inputs[] { left.data(), right.data() };
    float* outputs[] { outLeft.data(), outRight.data() };
    processor.process(inputs, outputs, 2, static_cast<int>(left.size()));
  }
};

vaporwave::ParameterState neutralState() {
  vaporwave::ParameterState state;
  state.denoiseEnabled = false;
  state.eqEnabled = false;
  state.compressorEnabled = false;
  state.reverbEnabled = false;
  state.loudnessEnabled = false;
  state.limiterEnabled = false;
  state.autoClean = false;
  return state;
}

void testBypassTransparency() {
  vaporwave::Processor processor;
  processor.prepare(48000.0, 1024, 2);
  auto state = neutralState();
  state.masterBypass = true;
  state.inputGainDb = 12.0f;
  processor.setParameters(state);
  StereoBuffer buffer(4096);
  for (std::size_t i = 0; i < buffer.left.size(); ++i) {
    buffer.left[i] = std::sin(static_cast<float>(i) * 0.031f) * 0.8f;
    buffer.right[i] = std::cos(static_cast<float>(i) * 0.017f) * 0.6f;
  }
  buffer.process(processor);
  const auto latency = static_cast<std::size_t>(processor.latencySamples());
  bool transparent = true;
  for (std::size_t i = latency; i < buffer.left.size(); ++i) {
    transparent = transparent && buffer.outLeft[i] == buffer.left[i - latency];
    transparent = transparent && buffer.outRight[i] == buffer.right[i - latency];
  }
  expect(transparent, "master bypass must be transparent after fixed latency compensation");
}

void testEqBoost() {
  vaporwave::Processor processor;
  processor.prepare(48000.0, 8192, 2);
  auto state = neutralState();
  state.eqEnabled = true;
  state.eq[0].enabled = true;
  state.eq[0].type = vaporwave::FilterType::Bell;
  state.eq[0].frequencyHz = 1000.0f;
  state.eq[0].gainDb = 6.0f;
  state.eq[0].q = 1.0f;
  processor.setParameters(state);
  StereoBuffer buffer(8192);
  for (std::size_t i = 0; i < buffer.left.size(); ++i) {
    const float phase = 2.0f * std::numbers::pi_v<float> * 1000.0f *
                        static_cast<float>(i) / 48000.0f;
    buffer.left[i] = buffer.right[i] = 0.1f * std::sin(phase);
  }
  buffer.process(processor);
  float inPower = 0.0f;
  float outPower = 0.0f;
  for (std::size_t i = 4096; i < buffer.left.size(); ++i) {
    inPower += buffer.left[i] * buffer.left[i];
    outPower += buffer.outLeft[i] * buffer.outLeft[i];
  }
  const float ratio = std::sqrt(outPower / inPower);
  expect(ratio > 1.8f && ratio < 2.2f, "bell EQ should deliver approximately +6 dB");
}

void testEqLeftChannelRouting() {
  vaporwave::Processor processor;
  processor.prepare(48000.0, 8192, 2);
  auto state = neutralState();
  state.eqEnabled = true;
  state.eq[0].enabled = true;
  state.eq[0].type = vaporwave::FilterType::Bell;
  state.eq[0].channel = vaporwave::ChannelMode::Left;
  state.eq[0].frequencyHz = 1000.0f;
  state.eq[0].gainDb = 6.0f;
  state.eq[0].q = 1.0f;
  processor.setParameters(state);
  StereoBuffer buffer(8192);
  for (std::size_t i = 0; i < buffer.left.size(); ++i) {
    const float phase = 2.0f * std::numbers::pi_v<float> * 1000.0f *
                        static_cast<float>(i) / 48000.0f;
    buffer.left[i] = buffer.right[i] = 0.1f * std::sin(phase);
  }
  buffer.process(processor);
  float leftPower = 0.0f;
  float rightError = 0.0f;
  const auto latency = static_cast<std::size_t>(processor.latencySamples());
  for (std::size_t i = std::max<std::size_t>(4096, latency); i < buffer.left.size(); ++i) {
    leftPower += buffer.outLeft[i] * buffer.outLeft[i];
    rightError += std::abs(buffer.outRight[i] - buffer.right[i - latency]);
  }
  expect(leftPower > 3.0f * 0.5f * 0.01f * 4096.0f,
         "left-only EQ must boost the left channel");
  expect(rightError < 1.0e-6f,
         "left-only EQ must leave the right channel untouched");
}

void testCompressorReduction() {
  vaporwave::Processor processor;
  processor.prepare(48000.0, 8192, 2);
  auto state = neutralState();
  state.compressorEnabled = true;
  state.compressorThresholdDb = -30.0f;
  state.compressorRatio = 10.0f;
  state.compressorAttackMs = 1.0f;
  state.compressorReleaseMs = 50.0f;
  state.compressorKneeDb = 0.0f;
  state.compressorMix = 1.0f;
  processor.setParameters(state);
  StereoBuffer buffer(8192);
  std::fill(buffer.left.begin(), buffer.left.end(), 0.8f);
  std::fill(buffer.right.begin(), buffer.right.end(), -0.8f);
  buffer.process(processor);
  float latePeak = 0.0f;
  for (std::size_t i = 4096; i < buffer.outLeft.size(); ++i) {
    latePeak = std::max(latePeak, std::abs(buffer.outLeft[i]));
  }
  expect(latePeak < 0.2f,
         "compressor must reduce a sustained signal above threshold");
  expect(processor.meters().compressorGainReductionDb > 6.0f,
         "compressor gain-reduction meter must report activity");
}

void testReverbTail() {
  vaporwave::Processor processor;
  processor.prepare(48000.0, 48000, 2);
  auto state = neutralState();
  state.reverbEnabled = true;
  state.reverbPreDelayMs = 0.0f;
  state.reverbDecaySeconds = 1.5f;
  state.reverbSize = 0.7f;
  state.reverbDiffusion = 0.8f;
  state.reverbMix = 1.0f;
  processor.setParameters(state);
  StereoBuffer buffer(48000);
  buffer.left[0] = buffer.right[0] = 1.0f;
  buffer.process(processor);
  float tailEnergy = 0.0f;
  for (std::size_t i = 2000; i < buffer.outLeft.size(); ++i) {
    tailEnergy += buffer.outLeft[i] * buffer.outLeft[i];
    tailEnergy += buffer.outRight[i] * buffer.outRight[i];
  }
  expect(tailEnergy > 1.0e-6f,
         "reverb must produce a finite audible tail after an impulse");
}

void testLimiterCeiling() {
  vaporwave::Processor processor;
  processor.prepare(48000.0, 4096, 2);
  auto state = neutralState();
  state.limiterEnabled = true;
  state.limiterCeilingDb = -1.0f;
  state.limiterLookaheadMs = 1.0f;
  state.limiterReleaseMs = 50.0f;
  processor.setParameters(state);
  StereoBuffer buffer(4096);
  std::fill(buffer.left.begin(), buffer.left.end(), 2.0f);
  std::fill(buffer.right.begin(), buffer.right.end(), -2.0f);
  buffer.process(processor);
  const float peak = std::max(
    *std::max_element(buffer.outLeft.begin(), buffer.outLeft.end()),
    std::abs(*std::min_element(buffer.outRight.begin(), buffer.outRight.end())));
  expect(peak <= std::pow(10.0f, -1.0f / 20.0f) + 1.0e-5f,
         "limiter must not exceed its ceiling");
}

void testDenoiserStability() {
  vaporwave::Processor processor;
  processor.prepare(44100.0, 257, 2);
  auto state = neutralState();
  state.denoiseEnabled = true;
  state.denoiseReductionDb = 30.0f;
  state.denoiseSensitivity = 1.0f;
  state.denoiseSmoothing = 0.9f;
  processor.setParameters(state);
  StereoBuffer buffer(20000);
  std::uint32_t random = 0x12345678U;
  for (std::size_t i = 0; i < buffer.left.size(); ++i) {
    random = random * 1664525U + 1013904223U;
    const float noise = (static_cast<float>(random >> 8) / 8388608.0f - 1.0f) * 0.05f;
    buffer.left[i] = noise;
    buffer.right[i] = -noise;
  }
  buffer.process(processor);
  const bool finite = std::all_of(buffer.outLeft.begin(), buffer.outLeft.end(),
    [](float sample) { return std::isfinite(sample); });
  expect(finite, "denoiser must never emit NaN or infinity");
  const float denoiserLatencyMs =
    static_cast<float>(vaporwave::SpectralDenoiser::kFftSize) / 44100.0f * 1000.0f;
  expect(processor.latencySamples() >= 256,
         "denoiser latency must be reported");
  expect(denoiserLatencyMs < 10.0f,
         "realtime denoiser latency must remain below 10 ms at 44.1 kHz");
}

void testDefaultChainRealtimeLatency() {
  vaporwave::Processor processor;
  processor.prepare(44100.0, 512, 2);
  vaporwave::ParameterState state;
  state.denoiseEnabled = true;
  state.limiterEnabled = true;
  state.limiterLookaheadMs = 2.0f;
  processor.setParameters(state);
  const float latencyMs = static_cast<float>(processor.latencySamples()) /
                          44100.0f * 1000.0f;
  expect(latencyMs < 10.0f,
         "fixed worst-case latency compensation must remain below 10 ms");
}

void testLatencyIsStableAcrossModes() {
  vaporwave::Processor processor;
  processor.prepare(48000.0, 512, 2);
  auto state = neutralState();
  processor.setParameters(state);
  const int baseline = processor.latencySamples();
  state.masterBypass = true;
  processor.setParameters(state);
  expect(processor.latencySamples() == baseline,
         "master bypass must not change reported latency");
  state.masterBypass = false;
  state.autoClean = true;
  state.limiterLookaheadMs = 4.0f;
  processor.setParameters(state);
  expect(processor.latencySamples() == baseline,
         "Auto Clean and lookahead changes must not change reported latency");
}

void testAutoCleanEngagesProcessing() {
  vaporwave::Processor processor;
  processor.prepare(48000.0, 16384, 2);
  auto state = neutralState();
  state.autoClean = true;
  state.compressorThresholdDb = -30.0f;
  state.compressorRatio = 10.0f;
  state.compressorAttackMs = 1.0f;
  processor.setParameters(state);
  StereoBuffer buffer(16384);
  std::fill(buffer.left.begin(), buffer.left.end(), 0.8f);
  std::fill(buffer.right.begin(), buffer.right.end(), -0.8f);
  buffer.process(processor);
  expect(processor.meters().compressorGainReductionDb > 6.0f,
         "Auto Clean must engage the compressor stage");
}

void testStateDoesNotDependOnViewMode() {
  vaporwave::Processor simple;
  vaporwave::Processor formal;
  simple.prepare(48000.0, 1024, 2);
  formal.prepare(48000.0, 1024, 2);
  auto state = neutralState();
  state.eqEnabled = true;
  state.eq[0].enabled = true;
  state.eq[0].gainDb = 3.0f;
  auto formalState = state;
  state.viewMode = vaporwave::ViewMode::Simple;
  formalState.viewMode = vaporwave::ViewMode::Formal;
  simple.setParameters(state);
  formal.setParameters(formalState);
  StereoBuffer a(1024);
  StereoBuffer b(1024);
  for (std::size_t i = 0; i < a.left.size(); ++i) {
    a.left[i] = b.left[i] = std::sin(static_cast<float>(i) * 0.02f);
    a.right[i] = b.right[i] = std::cos(static_cast<float>(i) * 0.03f);
  }
  a.process(simple);
  b.process(formal);
  expect(a.outLeft == b.outLeft && a.outRight == b.outRight,
         "Simple/Formal view changes must not alter DSP");
}

} // namespace

int main() {
  testBypassTransparency();
  testEqBoost();
  testEqLeftChannelRouting();
  testCompressorReduction();
  testReverbTail();
  testLimiterCeiling();
  testDenoiserStability();
  testDefaultChainRealtimeLatency();
  testLatencyIsStableAcrossModes();
  testAutoCleanEngagesProcessing();
  testStateDoesNotDependOnViewMode();
  if (failures == 0) {
    std::cout << "All Vaporwave DSP tests passed.\n";
  }
  return failures == 0 ? 0 : 1;
}
