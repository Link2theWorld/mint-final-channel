#pragma once

#include "IControls.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

using namespace iplug;
using namespace igraphics;

namespace vaporwave_ui {

inline const IColor kBackground = IColor(255, 7, 10, 28);
inline const IColor kPanel = IColor(255, 15, 23, 52);
inline const IColor kBlue = IColor(255, 55, 226, 255);
inline const IColor kPink = IColor(255, 255, 79, 197);
inline const IColor kPurple = IColor(255, 148, 87, 255);
inline const IColor kLiveGreen = IColor(255, 75, 255, 151);
inline const IColor kBypassRed = IColor(255, 255, 86, 105);
inline const IColor kMuted = IColor(255, 205, 219, 242);
inline const IColor kWhite = IColor(255, 250, 252, 255);
inline constexpr const char* kFont = "MintUI";
inline constexpr const char* kCJKFont = "MintCJK";

inline IVStyle Style(const char* font = kFont, bool cjk = false) {
  return DEFAULT_STYLE
    .WithColor(kBG, kPanel)
    .WithColor(kFG, kBlue)
    .WithColor(kPR, kPink)
    .WithColor(kFR, IColor(255, 41, 70, 115))
    .WithColor(kHL, kPurple)
    .WithLabelText(IText(cjk ? 11.5f : 10.0f, kMuted, font))
    .WithValueText(IText(cjk ? 13.5f : 12.0f, kWhite, font))
    .WithDrawShadows(false)
    .WithRoundness(0.18f);
}

inline IVStyle CompactStyle(const char* font = kFont, bool cjk = false) {
  return Style(font, cjk)
    .WithLabelText(IText(cjk ? 8.5f : 7.0f, kMuted, font))
    .WithValueText(IText(cjk ? 10.0f : 8.0f, kWhite, font));
}

class HardwareBypassControl final : public ISwitchControlBase {
public:
  HardwareBypassControl(const IRECT& bounds, int paramIdx, const char* font,
                        bool chinese)
    : ISwitchControlBase(bounds, paramIdx, SplashClickActionFunc),
      font_(font), chinese_(chinese) {}

  void Draw(IGraphics& g) override {
    const bool bypassed = GetValue() >= 0.5;
    const IColor led = bypassed ? kBypassRed : kLiveGreen;
    const float pressedOffset = bypassed ? 2.0f : 0.0f;
    const IRECT shadow(mRECT.L + 2.0f, mRECT.T + 5.0f,
                       mRECT.R - 2.0f, mRECT.B + 1.0f);
    const IRECT bezel(mRECT.L, mRECT.T, mRECT.R, mRECT.B - 3.0f);
    const IRECT face(mRECT.L + 3.0f, mRECT.T + 3.0f + pressedOffset,
                     mRECT.R - 3.0f, mRECT.B - 6.0f + pressedOffset);

    g.FillRoundRect(IColor(180, 0, 0, 0), shadow, 8.0f);
    g.FillRoundRect(IColor(255, 5, 8, 20), bezel, 8.0f);
    g.DrawRoundRect(IColor(255, 69, 82, 119), bezel, 8.0f, nullptr, 1.5f);
    g.FillRoundRect(bypassed ? IColor(255, 47, 19, 35)
                              : IColor(255, 14, 48, 48),
                    face, 6.0f);
    g.DrawRoundRect(led, face, 6.0f, nullptr, 1.5f);

    const float ledX = face.L + 14.0f;
    const float ledY = face.MH();
    g.FillCircle(IColor(48, led.R, led.G, led.B), ledX, ledY, 8.0f);
    g.FillCircle(led, ledX, ledY, 4.5f);
    g.FillCircle(IColor(210, 255, 255, 255), ledX - 1.5f, ledY - 1.5f, 1.2f);

    const IRECT textRect(face.L + 27.0f, face.T + 1.0f,
                         face.R - 4.0f, face.B - 1.0f);
    const char* primary = bypassed
      ? (chinese_ ? "已旁路" : "BYPASSED")
      : (chinese_ ? "启用" : "ACTIVE");
    const char* secondary = bypassed
      ? (chinese_ ? "原始信号" : "DRY SIGNAL")
      : (chinese_ ? "正在处理" : "PROCESSING");
    g.DrawText(IText(chinese_ ? 10.5f : 9.5f, led, font_, EAlign::Near,
                     EVAlign::Top), primary, textRect);
    g.DrawText(IText(chinese_ ? 8.5f : 7.5f, kWhite, font_, EAlign::Near,
                     EVAlign::Bottom), secondary, textRect);
  }

private:
  const char* font_ = kFont;
  bool chinese_ = false;
};

class EqGraphControl final : public IControl {
public:
  explicit EqGraphControl(const IRECT& bounds) : IControl(bounds) {}

  void Draw(IGraphics& g) override {
    g.FillRoundRect(kPanel, mRECT, 10.0f);
    const IColor grid(80, 70, 102, 148);
    for (int i = 1; i < 8; ++i) {
      const float x = mRECT.L + mRECT.W() * static_cast<float>(i) / 8.0f;
      g.DrawLine(grid, x, mRECT.T, x, mRECT.B, nullptr, 1.0f);
    }
    for (int i = 1; i < 6; ++i) {
      const float y = mRECT.T + mRECT.H() * static_cast<float>(i) / 6.0f;
      g.DrawLine(grid, mRECT.L, y, mRECT.R, y, nullptr, 1.0f);
    }
    const float mid = mRECT.MH();
    float previousX = mRECT.L;
    float previousY = mid;
    for (int i = 1; i <= 100; ++i) {
      const float t = static_cast<float>(i) / 100.0f;
      const float x = mRECT.L + t * mRECT.W();
      float gain = 0.0f;
      for (int band = 0; band < 12; ++band) {
        if (!GetDelegate()->GetParam(EqParam(band, kEqEnabled))->Bool()) continue;
        const float frequency = static_cast<float>(
          GetDelegate()->GetParam(EqParam(band, kEqFrequency))->Value());
        const float bandGain = static_cast<float>(
          GetDelegate()->GetParam(EqParam(band, kEqGain))->Value());
        const float q = static_cast<float>(
          GetDelegate()->GetParam(EqParam(band, kEqQ))->Value());
        const float centre = std::log10(frequency / 20.0f) / 3.0f;
        const float distance = (t - centre) * (2.0f + q * 1.5f);
        gain += bandGain * std::exp(-distance * distance * 4.0f);
      }
      const float y = mid - gain * mRECT.H() / 48.0f;
      g.DrawLine(kBlue, previousX, previousY, x, y, nullptr, 2.0f);
      previousX = x;
      previousY = y;
    }
    for (int band = 0; band < 12; ++band) {
      if (!GetDelegate()->GetParam(EqParam(band, kEqEnabled))->Bool()) continue;
      const float frequency = static_cast<float>(
        GetDelegate()->GetParam(EqParam(band, kEqFrequency))->Value());
      const float gain = static_cast<float>(
        GetDelegate()->GetParam(EqParam(band, kEqGain))->Value());
      const float x = frequencyToX(frequency);
      const float y = gainToY(gain);
      const IColor color = band == selected_ ? kPink : kBlue;
      g.FillCircle(color, x, y, band == selected_ ? 8.0f : 6.0f);
      g.DrawText(IText(9.0f, kBackground, kFont), std::to_string(band + 1).c_str(),
                 IRECT(x - 8, y - 8, x + 8, y + 8));
    }
    g.DrawText(IText(12.0f, kMuted, kFont), "20 Hz        100        1k        10k       20k",
               mRECT.GetFromBottom(22.0f));
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override {
    selected_ = nearestBand(x, y);
    if (selected_ < 0) {
      for (int band = 0; band < 12; ++band) {
        if (!GetDelegate()->GetParam(EqParam(band, kEqEnabled))->Bool()) {
          selected_ = band;
          break;
        }
      }
    }
    if (selected_ < 0) selected_ = 0;
    GetDelegate()->BeginInformHostOfParamChangeFromUI(EqParam(selected_, kEqFrequency));
    GetDelegate()->BeginInformHostOfParamChangeFromUI(EqParam(selected_, kEqGain));
    GetDelegate()->SendParameterValueFromUI(EqParam(selected_, kEqEnabled), 1.0);
    GetDelegate()->SendParameterValueFromUI(kParamEqEnabled, 1.0);
    GetDelegate()->SendParameterValueFromUI(kParamEqPreset, 1.0);
    updateNode(x, y);
  }

  void OnMouseDrag(float x, float y, float, float, const IMouseMod&) override {
    if (selected_ >= 0) updateNode(x, y);
  }

  void OnMouseUp(float, float, const IMouseMod&) override {
    if (selected_ < 0) return;
    GetDelegate()->EndInformHostOfParamChangeFromUI(EqParam(selected_, kEqFrequency));
    GetDelegate()->EndInformHostOfParamChangeFromUI(EqParam(selected_, kEqGain));
  }

private:
  float frequencyToX(float frequency) const {
    const float normalized = std::log10(std::clamp(frequency, 20.0f, 20000.0f) / 20.0f) / 3.0f;
    return mRECT.L + normalized * mRECT.W();
  }

  float gainToY(float gain) const {
    return mRECT.MH() - std::clamp(gain, -24.0f, 24.0f) * mRECT.H() / 48.0f;
  }

  int nearestBand(float x, float y) {
    int nearest = -1;
    float best = 18.0f * 18.0f;
    for (int band = 0; band < 12; ++band) {
      if (!GetDelegate()->GetParam(EqParam(band, kEqEnabled))->Bool()) continue;
      const float px = frequencyToX(static_cast<float>(
        GetDelegate()->GetParam(EqParam(band, kEqFrequency))->Value()));
      const float py = gainToY(static_cast<float>(
        GetDelegate()->GetParam(EqParam(band, kEqGain))->Value()));
      const float distance = (x - px) * (x - px) + (y - py) * (y - py);
      if (distance < best) {
        best = distance;
        nearest = band;
      }
    }
    return nearest;
  }

  void updateNode(float x, float y) {
    const float normalizedX = std::clamp((x - mRECT.L) / mRECT.W(), 0.0f, 1.0f);
    const float frequency = 20.0f * std::pow(1000.0f, normalizedX);
    const float gain = std::clamp((mRECT.MH() - y) * 48.0f / mRECT.H(),
                                  -24.0f, 24.0f);
    auto* frequencyParam = GetDelegate()->GetParam(EqParam(selected_, kEqFrequency));
    auto* gainParam = GetDelegate()->GetParam(EqParam(selected_, kEqGain));
    GetDelegate()->SendParameterValueFromUI(
      EqParam(selected_, kEqFrequency), frequencyParam->ToNormalized(frequency));
    GetDelegate()->SendParameterValueFromUI(
      EqParam(selected_, kEqGain), gainParam->ToNormalized(gain));
    SetDirty(false);
  }

  int selected_ = -1;
};

} // namespace vaporwave_ui
