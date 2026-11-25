/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* The look of a Hydrasynth's top panel: satin black steel with a fine texture, recessed section wells with orange
 * titles, brushed aluminium knob caps on black knurled skirts, endless encoders with white LED rings, black
 * rubber-topped buttons whose legends light (white, or the module buttons' orange), white-on-black OLEDs. The fonts
 * are embedded (Fira Sans, Anonymous Pro: SIL OFL, fonts/). */
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

namespace hyui
{
namespace col
{
const juce::Colour panel{0xff17171a}, well{0xff111113}, wellEdge{0xff2a2a2e}, title{0xffff6a1c}, legend{0xffe8e8e6},
    dim{0xffa8a8ac}, orangeLit{0xffff8a2a}, orangeDim{0xffb05a1e}, whiteLit{0xfff4f8ff}, blueLit{0xff9fd0ff},
    oledBg{0xff050506}, oled{0xffeef4ff}, oledDim{0xff6c7480}, line{0xffd9d9d9};
}

enum class Face { label, bold, heavy, mono };

struct UiResources {
    UiResources() = default;
    ~UiResources();
};
juce::Font font(Face f, float height);

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font &) override;
    juce::Font getPopupMenuFont() override { return font(Face::label, 15.0f); }
    void drawPopupMenuBackground(juce::Graphics &, int w, int h) override;
};

/* the panel's steel and its wells (sections) */
void drawPanel(juce::Graphics &, juce::Rectangle<float> r, float density);
void drawWell(juce::Graphics &, juce::Rectangle<float> r, const juce::String &title);
/* the panel's surface over everything printed: grain, brushing, scratches, dust, the light's fall */
void drawSurface(juce::Graphics &, juce::Rectangle<float> r, float density);
/* an aluminium knob (the cap kept as a picture per size): pos 0..1; pointer drawn on it */
void drawKnobBody(juce::Graphics &, juce::Rectangle<float> r);
void drawKnobPointer(juce::Graphics &, juce::Rectangle<float> r, float pos);
/* an encoder's LED ring: 15 dots, lit to pos (bipolar: from the top) */
void drawLedRing(juce::Graphics &, juce::Point<float> c, float radius, float pos, bool bipolar, bool active);
/* a button: black, its legend lit (white, orange or blue) when on */
enum class Lit { white, orange, blue };
void drawButton(juce::Graphics &, juce::Rectangle<float> r, const juce::String &text, bool on, bool down, Lit kind,
                float textSize = 10.0f, bool dimLegend = true);
void drawLed(juce::Graphics &, juce::Point<float> c, float r, float level, juce::Colour colour);
void drawLegend(juce::Graphics &, const juce::String &, juce::Rectangle<float>, float size = 12.0f,
                juce::Justification j = juce::Justification::centred, juce::Colour c = col::legend);
void drawTag(juce::Graphics &, juce::Rectangle<float> r);
} // namespace hyui
