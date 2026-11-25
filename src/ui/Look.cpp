/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
#include "Look.h"
#include "BinaryData.h"
#include <map>

namespace hyui
{
/* the fonts, the tag and the cached pictures: plain pointers, made when first drawn and freed when the last editor
 * closes (never function statics: those are freed while the host unloads the DLL, under Windows' loader lock) */
struct Store {
    juce::Typeface::Ptr t[4];
    juce::Image tag;
    std::map<int, juce::Image> grain, knob;
};
static Store *store = nullptr;
static Store &get()
{
    if (store == nullptr) {
        store = new Store();
        store->t[0] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansSemiBold_ttf, BinaryData::FiraSansSemiBold_ttfSize);
        store->t[1] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansBold_ttf, BinaryData::FiraSansBold_ttfSize);
        store->t[2] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansExtraBold_ttf, BinaryData::FiraSansExtraBold_ttfSize);
        store->t[3] = juce::Typeface::createSystemTypefaceFor(BinaryData::AnonymousProBold_ttf, BinaryData::AnonymousProBold_ttfSize);
        store->tag = juce::ImageFileFormat::loadFrom(BinaryData::swtdr34mztag_png, BinaryData::swtdr34mztag_pngSize);
    }
    return *store;
}
UiResources::~UiResources()
{
    delete store;
    store = nullptr;
    juce::Typeface::clearTypefaceCache();
    juce::ImageCache::releaseUnusedImages();
}
juce::Font font(Face f, float height) { return juce::Font(juce::FontOptions(get().t[(int)f]).withHeight(height)); }

Look::Look()
{
    using namespace juce;
    setColour(ResizableWindow::backgroundColourId, col::panel);
    setColour(PopupMenu::backgroundColourId, Colour(0xff19191c));
    setColour(PopupMenu::textColourId, col::legend);
    setColour(PopupMenu::highlightedBackgroundColourId, Colour(0xffd2561a));
    setColour(PopupMenu::highlightedTextColourId, Colours::white);
    setColour(PopupMenu::headerTextColourId, col::title);
    setColour(AlertWindow::backgroundColourId, Colour(0xff19191c));
    setColour(AlertWindow::textColourId, col::legend);
    setColour(TextEditor::backgroundColourId, Colour(0xff050506));
    setColour(TextEditor::textColourId, col::oled);
    setColour(TextButton::buttonColourId, Colour(0xff2a2a2e));
}
juce::Typeface::Ptr Look::getTypefaceForFont(const juce::Font &) { return get().t[0]; }
void Look::drawPopupMenuBackground(juce::Graphics &g, int w, int h)
{
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(col::title.withAlpha(0.5f));
    g.drawRect(0, 0, w, h, 1);
}

/* ------------------------------------------------------------------------------ the panel --- */
static const juce::Image &grain(float density)
{
    int key = juce::jlimit(1, 8, (int)std::lround(density * 2.0f));
    auto &img = get().grain[key];
    if (img.isNull()) {
        int n = 256;
        img = juce::Image(juce::Image::ARGB, n, n, true);
        juce::Random rnd(1234 + key);
        juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                float v = rnd.nextFloat();
                uint8_t a = v < 0.5f ? (uint8_t)(rnd.nextFloat() * 22.0f) : (uint8_t)(rnd.nextFloat() * 14.0f);
                bd.setPixelColour(x, y, v < 0.5f ? juce::Colour::fromRGBA(255, 255, 255, a) : juce::Colour::fromRGBA(0, 0, 0, a));
            }
    }
    return img;
}
void drawPanel(juce::Graphics &g, juce::Rectangle<float> r, float density)
{
    using namespace juce;
    g.setGradientFill(ColourGradient(Colour(0xff1d1d21), r.getX(), r.getY(), Colour(0xff121214), r.getX(), r.getBottom(), false));
    g.fillRect(r);
    /* the powder coat's grain, at the screen's density */
    const Image &gr = grain(density);
    float tile = 256.0f / juce::jmax(1.0f, density * 2.0f) * 2.0f;
    Graphics::ScopedSaveState s(g);
    g.reduceClipRegion(r.toNearestInt());
    for (float y = r.getY(); y < r.getBottom(); y += tile)
        for (float x = r.getX(); x < r.getRight(); x += tile)
            g.drawImage(gr, Rectangle<float>(x, y, tile, tile), RectanglePlacement::stretchToFit);
    /* a soft sheen from the top left */
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.035f), r.getX(), r.getY(), Colours::transparentWhite,
                                     r.getX() + r.getWidth() * 0.5f, r.getY() + r.getHeight(), false));
    g.fillRect(r);
}
void drawWell(juce::Graphics &g, juce::Rectangle<float> r, const juce::String &title)
{
    using namespace juce;
    Path p;
    p.addRoundedRectangle(r, 7.0f);
    g.setColour(Colour(0xff0d0d0f).withAlpha(0.92f));
    g.fillPath(p);
    /* the recess: dark above, a faint light edge below */
    g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.55f), r.getX(), r.getY(), Colours::transparentBlack, r.getX(), r.getY() + 12.0f, false));
    g.fillPath(p);
    g.setColour(col::wellEdge);
    g.strokePath(p, PathStrokeType(1.0f));
    g.setColour(Colours::white.withAlpha(0.05f));
    g.drawLine(r.getX() + 8, r.getBottom() + 0.5f, r.getRight() - 8, r.getBottom() + 0.5f, 1.0f);
    if (title.isNotEmpty()) {
        auto band = r.withHeight(26.0f).reduced(3.0f, 3.0f);
        g.setColour(Colour(0xff08080a));
        g.fillRoundedRectangle(band, 4.0f);
        g.setFont(font(Face::bold, 17.0f).withExtraKerningFactor(0.04f));
        g.setColour(col::title);
        g.drawText(title, band, Justification::centred, false);
    }
}

/* ------------------------------------------------------------------------------- the knob --- */
static void paintKnob(juce::Graphics &g, juce::Rectangle<float> r)
{
    using namespace juce;
    auto c = r.getCentre();
    float R = r.getWidth() * 0.5f;
    /* its shadow */
    g.setColour(Colours::black.withAlpha(0.55f));
    g.fillEllipse(r.translated(1.5f, 3.0f).reduced(R * 0.02f));
    /* the black knurled skirt */
    g.setGradientFill(ColourGradient(Colour(0xff2b2b2e), c.x, c.y - R, Colour(0xff060607), c.x, c.y + R, false));
    g.fillEllipse(r);
    g.setColour(Colour(0xff3a3a3e));
    for (int i = 0; i < 48; i++) {
        float a = (float)i / 48.0f * MathConstants<float>::twoPi;
        float s = std::sin(a), co = std::cos(a);
        g.drawLine(c.x + co * R * 0.86f, c.y + s * R * 0.86f, c.x + co * R * 0.99f, c.y + s * R * 0.99f, 1.0f);
    }
    /* the aluminium cap: spun (a ring of fine radial streaks), lit from the top left */
    float rc = R * 0.74f;
    auto cap = Rectangle<float>(c.x - rc, c.y - rc, rc * 2, rc * 2);
    g.setColour(Colour(0xff8c8f94));
    g.fillEllipse(cap.expanded(1.2f));
    g.setGradientFill(ColourGradient(Colour(0xffe9ecef), cap.getX(), cap.getY(), Colour(0xff8d9196), cap.getRight(), cap.getBottom(), false));
    g.fillEllipse(cap);
    Random rnd(7);
    for (int i = 0; i < 180; i++) {
        float a = (float)i / 180.0f * MathConstants<float>::twoPi;
        float l = 0.5f + 0.5f * std::cos(2.0f * (a - 0.8f));
        g.setColour((l > 0.5f ? Colours::white : Colours::black).withAlpha(0.06f + 0.1f * rnd.nextFloat() * std::fabs(l - 0.5f) * 2.0f));
        g.drawLine(c.x, c.y, c.x + std::cos(a) * rc, c.y + std::sin(a) * rc, 0.8f);
    }
    /* the spun highlight: two bright wedges, as a turned cap shows them */
    for (int k = 0; k < 2; k++) {
        Path w;
        float a0 = -2.4f + k * MathConstants<float>::pi;
        w.addPieSegment(cap, a0, a0 + 0.55f, 0.08f);
        g.setColour(Colours::white.withAlpha(0.28f));
        g.fillPath(w);
    }
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.0f), c.x, c.y, Colours::white.withAlpha(0.18f), c.x, cap.getY(), true));
    g.fillEllipse(cap);
    g.setColour(Colour(0xff4b4e53));
    g.drawEllipse(cap, 1.0f);
}
void drawKnobBody(juce::Graphics &g, juce::Rectangle<float> r)
{
    float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
    int key = (int)std::lround(r.getWidth() * sc);
    auto &cache = get().knob;
    auto it = cache.find(key);
    if (it == cache.end()) {
        int n = juce::jmax(4, key + 8);
        juce::Image made(juce::Image::ARGB, n, n, true, juce::SoftwareImageType());
        {
            juce::Graphics kg(made);
            paintKnob(kg, juce::Rectangle<float>(2.0f, 2.0f, (float)key, (float)key));
        }
        it = cache.emplace(key, made).first;
    }
    const juce::Image &img = it->second;
    float px = r.getWidth() / (float)key;
    g.setOpacity(1.0f);
    g.drawImage(img, juce::Rectangle<float>(r.getX() - 2.0f * px, r.getY() - 2.0f * px, img.getWidth() * px, img.getHeight() * px),
                juce::RectanglePlacement::stretchToFit);
}
void drawKnobPointer(juce::Graphics &g, juce::Rectangle<float> r, float pos)
{
    using namespace juce;
    auto c = r.getCentre();
    float R = r.getWidth() * 0.5f;
    float a = (-0.75f + 1.5f * juce::jlimit(0.0f, 1.0f, pos)) * MathConstants<float>::pi;
    float s = std::sin(a), co = -std::cos(a);
    g.setColour(Colour(0xff111214));
    g.drawLine(c.x + s * R * 0.32f, c.y + co * R * 0.32f, c.x + s * R * 0.72f, c.y + co * R * 0.72f, juce::jmax(2.0f, R * 0.075f));
    /* the skirt's white index */
    g.setColour(Colours::white.withAlpha(0.85f));
    g.drawLine(c.x + s * R * 0.80f, c.y + co * R * 0.80f, c.x + s * R * 0.97f, c.y + co * R * 0.97f, juce::jmax(1.5f, R * 0.05f));
}
void drawLedRing(juce::Graphics &g, juce::Point<float> c, float radius, float pos, bool bipolar, bool active)
{
    using namespace juce;
    const int N = 15;
    int lit = (int)std::lround(jlimit(0.0f, 1.0f, pos) * (N - 1));
    for (int i = 0; i < N; i++) {
        float a = (-0.75f + 1.5f * (float)i / (float)(N - 1)) * MathConstants<float>::pi;
        Point<float> p(c.x + std::sin(a) * radius, c.y - std::cos(a) * radius);
        bool on = active && (bipolar ? ((i >= 7 && i <= lit) || (i <= 7 && i >= lit)) : i <= lit);
        Path seg;
        seg.addRoundedRectangle(-2.2f, -4.0f, 4.4f, 8.0f, 1.6f);
        seg.applyTransform(AffineTransform::rotation(a).translated(p));
        if (on) {
            g.setColour(col::blueLit.withAlpha(0.25f));
            g.fillEllipse(p.x - 6, p.y - 6, 12, 12);
            g.setColour(col::whiteLit);
        } else
            g.setColour(Colour(0xff202124));
        g.fillPath(seg);
        g.setColour(Colours::black.withAlpha(0.6f));
        g.strokePath(seg, PathStrokeType(0.6f));
    }
}

/* ----------------------------------------------------------------------------- the button --- */
void drawButton(juce::Graphics &g, juce::Rectangle<float> r, const juce::String &text, bool on, bool down, Lit kind,
                float textSize, bool dimLegend)
{
    using namespace juce;
    auto b = down ? r.translated(0, 1.0f) : r;
    g.setColour(Colours::black.withAlpha(0.6f));
    g.fillRoundedRectangle(r.translated(0.5f, 2.0f), 4.0f);
    g.setGradientFill(ColourGradient(Colour(0xff2c2c30), b.getX(), b.getY(), Colour(0xff0b0b0d), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle(b, 4.0f);
    auto face = b.reduced(2.5f);
    g.setGradientFill(ColourGradient(Colour(0xff1b1b1e), face.getX(), face.getY(), Colour(0xff0e0e10), face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle(face, 3.0f);
    Colour lit = kind == Lit::orange ? col::orangeLit : kind == Lit::blue ? col::blueLit : col::whiteLit;
    if (on && kind == Lit::white && text.isEmpty()) {
        /* a blank white-lit cap (the Master Control's) */
        g.setColour(lit.withAlpha(0.25f));
        g.fillRoundedRectangle(face.expanded(1.5f), 4.0f);
        g.setColour(lit);
        g.fillRoundedRectangle(face.reduced(1.5f), 2.0f);
    } else if (on && text.isEmpty()) {
        g.setColour(lit.withAlpha(0.8f));
        g.fillRoundedRectangle(face.reduced(2.0f), 2.0f);
    }
    if (text.isNotEmpty()) {
        Colour tc = on ? lit : (kind == Lit::orange ? col::orangeDim : (dimLegend ? Colour(0xff6e6e72) : col::legend));
        g.setFont(font(Face::heavy, textSize));
        if (on) {
            g.setColour(tc.withAlpha(0.35f));
            for (int k = 0; k < 4; k++)
                g.drawFittedText(text, face.translated((k & 1) ? 0.7f : -0.7f, (k & 2) ? 0.7f : -0.7f).toNearestInt(), Justification::centred, 2, 0.8f);
        }
        g.setColour(tc);
        g.drawFittedText(text, face.toNearestInt(), Justification::centred, 2, 0.8f);
    }
    g.setColour(Colours::white.withAlpha(0.07f));
    g.drawRoundedRectangle(b.reduced(0.5f), 4.0f, 1.0f);
}
void drawLed(juce::Graphics &g, juce::Point<float> c, float r, float level, juce::Colour colour)
{
    using namespace juce;
    level = jlimit(0.0f, 1.0f, level);
    g.setColour(Colour(0xff1d1d20));
    g.fillEllipse(c.x - r, c.y - r, 2 * r, 2 * r);
    if (level > 0.01f) {
        g.setColour(colour.withAlpha(0.3f * level));
        g.fillEllipse(c.x - r * 2.2f, c.y - r * 2.2f, 4.4f * r, 4.4f * r);
        g.setColour(colour.withAlpha(0.35f + 0.65f * level));
        g.fillEllipse(c.x - r * 0.8f, c.y - r * 0.8f, 1.6f * r, 1.6f * r);
    }
}
void drawLegend(juce::Graphics &g, const juce::String &t, juce::Rectangle<float> r, float size, juce::Justification j, juce::Colour c)
{
    g.setFont(font(Face::bold, size));
    g.setColour(c);
    g.drawFittedText(t, r.toNearestInt(), j, 2, 0.85f);
}
void drawTag(juce::Graphics &g, juce::Rectangle<float> r)
{
    const juce::Image &tag = get().tag;
    if (tag.isNull())
        return;
    auto fit = juce::RectanglePlacement(juce::RectanglePlacement::centred).appliedTo(tag.getBounds().toFloat(), r);
    g.setColour(col::legend);
    g.drawImage(tag, fit, juce::RectanglePlacement::stretchToFit, true);
}
} // namespace hyui
