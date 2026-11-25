/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
#include "Look.h"
#include "BinaryData.h"
#include <map>
#include <vector>

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
/* the textures, made once per pixel density: the powder coat's grain, and the steel's brushing under it */
static const juce::Image &grain(float density)
{
    int key = juce::jlimit(1, 8, (int)std::lround(density * 2.0f));
    auto &img = get().grain[key];
    if (img.isNull()) {
        int n = 384;
        img = juce::Image(juce::Image::ARGB, n, n, true, juce::SoftwareImageType());
        juce::Random rnd(1234 + key);
        juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
        /* each row a streak of the brushing (a long moving average of noise), the coat's fine grain on top */
        std::vector<float> row((size_t)n), raw((size_t)n);
        for (int y = 0; y < n; y++) {
            float rowTone = (rnd.nextFloat() - 0.5f) * 0.06f;
            for (int x = 0; x < n; x++)
                raw[(size_t)x] = rnd.nextFloat() - 0.5f;
            const int win = 22;
            float acc = 0.0f;
            for (int x = -win; x < n; x++) {
                acc += raw[(size_t)((x + win + n) % n)];
                acc -= raw[(size_t)((x + n) % n)];
                if (x >= 0)
                    row[(size_t)x] = acc / (float)win;
            }
            for (int x = 0; x < n; x++) {
                float v = rowTone + row[(size_t)x] * 0.35f + (rnd.nextFloat() - 0.5f) * 0.16f;
                if (rnd.nextFloat() < 0.012f)
                    v += (rnd.nextFloat() - 0.4f) * 0.5f;       /* a pore of the coat */
                uint8_t a = (uint8_t)juce::jlimit(0, 255, (int)(std::fabs(v) * 255.0f * 0.55f));
                bd.setPixelColour(x, y, v > 0 ? juce::Colour::fromRGBA(255, 255, 255, a) : juce::Colour::fromRGBA(0, 0, 0, a));
            }
        }
    }
    return img;
}
void drawPanel(juce::Graphics &g, juce::Rectangle<float> r, float)
{
    using namespace juce;
    /* anthracite steel, lighter where the light falls (top left) */
    g.setGradientFill(ColourGradient(Colour(0xff45474d), r.getX(), r.getY(), Colour(0xff2c2d31), r.getRight(), r.getBottom(), false));
    g.fillRect(r);
}
void drawSurface(juce::Graphics &g, juce::Rectangle<float> r, float density)
{
    using namespace juce;
    /* the coat's grain and brushing, at one texel a screen pixel */
    const Image &gr = grain(density);
    float tile = (float)gr.getWidth() / juce::jmax(0.5f, density);
    {
        Graphics::ScopedSaveState s(g);
        g.reduceClipRegion(r.toNearestInt());
        g.setOpacity(0.85f);
        for (float y = r.getY(); y < r.getBottom(); y += tile)
            for (float x = r.getX(); x < r.getRight(); x += tile)
                g.drawImage(gr, Rectangle<float>(x, y, tile, tile), RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);
    }
    Random rnd(4711);
    float W = r.getWidth(), H = r.getHeight();
    /* old fingerprints and wiping marks: large, very faint */
    for (int i = 0; i < 14; i++) {
        float cx = r.getX() + rnd.nextFloat() * W, cy = r.getY() + rnd.nextFloat() * H, rad = 30.0f + rnd.nextFloat() * 90.0f;
        g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.03f + rnd.nextFloat() * 0.025f), cx, cy, Colours::transparentWhite, cx + rad, cy, true));
        g.fillEllipse(cx - rad, cy - rad * 0.7f, rad * 2, rad * 1.4f);
    }
    /* fine scratches, mostly along the brushing */
    for (int i = 0; i < 90; i++) {
        float x = r.getX() + rnd.nextFloat() * W, y = r.getY() + rnd.nextFloat() * H;
        float len = 6.0f + rnd.nextFloat() * rnd.nextFloat() * 70.0f, ang = (rnd.nextFloat() - 0.5f) * (rnd.nextFloat() < 0.7f ? 0.25f : 3.0f);
        g.setColour(Colours::white.withAlpha(0.035f + rnd.nextFloat() * 0.06f));
        g.drawLine(x, y, x + std::cos(ang) * len, y + std::sin(ang) * len, 0.35f + rnd.nextFloat() * 0.3f);
    }
    /* dust: specks, more of them low down and in the corners, and a few fibres */
    for (int i = 0; i < 2600; i++) {
        float x = r.getX() + rnd.nextFloat() * W;
        float yy = std::pow(rnd.nextFloat(), 0.7f);
        float y = r.getY() + yy * H;
        float s = 0.25f + rnd.nextFloat() * rnd.nextFloat() * 1.6f;
        bool light = rnd.nextFloat() < 0.78f;
        g.setColour((light ? Colour(0xffdcdad4) : Colour(0xff0a0a0a)).withAlpha(0.08f + rnd.nextFloat() * 0.28f));
        g.fillEllipse(x - s * 0.5f, y - s * 0.5f, s, s * (0.7f + rnd.nextFloat() * 0.6f));
    }
    for (int i = 0; i < 26; i++) {
        float x = r.getX() + rnd.nextFloat() * W, y = r.getY() + rnd.nextFloat() * H;
        Path f;
        f.startNewSubPath(x, y);
        f.cubicTo(x + rnd.nextFloat() * 12 - 6, y + rnd.nextFloat() * 12 - 6, x + rnd.nextFloat() * 20 - 10, y + rnd.nextFloat() * 20 - 10,
                  x + rnd.nextFloat() * 26 - 13, y + rnd.nextFloat() * 26 - 13);
        g.setColour(Colour(0xffd8d6d0).withAlpha(0.10f + rnd.nextFloat() * 0.12f));
        g.strokePath(f, PathStrokeType(0.4f));
    }
    /* the light: a broad soft pool from the top left, the far corners falling off */
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.13f), r.getX() + W * 0.22f, r.getY() - H * 0.15f, Colours::transparentWhite,
                                     r.getX() + W * 0.95f, r.getY() + H * 0.9f, true));
    g.fillRect(r);
    g.setGradientFill(ColourGradient(Colours::transparentBlack, r.getCentreX(), r.getCentreY(), Colours::black.withAlpha(0.32f),
                                     r.getX() - W * 0.05f, r.getY() - H * 0.05f, true));
    g.fillRect(r);
    /* the panel's bevelled edge */
    g.setColour(Colours::white.withAlpha(0.18f));
    g.drawLine(r.getX(), r.getY() + 0.5f, r.getRight(), r.getY() + 0.5f, 1.0f);
    g.drawLine(r.getX() + 0.5f, r.getY(), r.getX() + 0.5f, r.getBottom(), 1.0f);
    g.setColour(Colours::black.withAlpha(0.5f));
    g.drawLine(r.getX(), r.getBottom() - 0.5f, r.getRight(), r.getBottom() - 0.5f, 1.5f);
    g.drawLine(r.getRight() - 0.5f, r.getY(), r.getRight() - 0.5f, r.getBottom(), 1.5f);
}
void drawWell(juce::Graphics &g, juce::Rectangle<float> r, const juce::String &title)
{
    using namespace juce;
    Path p;
    p.addRoundedRectangle(r, 8.0f);
    /* a recessed field, a shade darker than the panel */
    g.setGradientFill(ColourGradient(Colour(0xff2f3034), r.getX(), r.getY(), Colour(0xff26272a), r.getX(), r.getBottom(), false));
    g.fillPath(p);
    /* its walls: in shadow at the top and left (the light comes from there), catching light at the bottom and right */
    {
        Graphics::ScopedSaveState s(g);
        g.reduceClipRegion(p);
        for (int i = 0; i < 6; i++) {
            Path in;
            in.addRoundedRectangle(r.translated(2.0f + i * 0.6f, 2.5f + i * 0.7f).expanded(1.0f), 8.0f);
            Path ring = p;
            ring.setUsingNonZeroWinding(false);
            ring.addPath(in);
            g.setColour(Colours::black.withAlpha(0.07f));
            g.fillPath(ring);
        }
    }
    g.setColour(Colours::black.withAlpha(0.55f));
    g.strokePath(p, PathStrokeType(1.0f));
    g.setColour(Colours::white.withAlpha(0.13f));
    g.drawLine(r.getX() + 9, r.getBottom() + 0.8f, r.getRight() - 9, r.getBottom() + 0.8f, 1.0f);
    g.drawLine(r.getRight() + 0.8f, r.getY() + 9, r.getRight() + 0.8f, r.getBottom() - 9, 1.0f);
    if (title.isNotEmpty()) {
        auto band = r.withHeight(30.0f).reduced(4.0f, 4.0f);
        g.setGradientFill(ColourGradient(Colour(0xff1d1e21), band.getX(), band.getY(), Colour(0xff252629), band.getX(), band.getBottom(), false));
        g.fillRoundedRectangle(band, 4.0f);
        g.setColour(Colours::white.withAlpha(0.07f));
        g.drawLine(band.getX() + 4, band.getBottom() + 0.5f, band.getRight() - 4, band.getBottom() + 0.5f, 1.0f);
        g.setFont(font(Face::bold, 18.0f).withExtraKerningFactor(0.05f));
        g.setColour(Colours::black.withAlpha(0.6f));
        g.drawText(title, band.translated(0.8f, 1.0f), Justification::centred, false);
        g.setColour(col::title);
        g.drawText(title, band, Justification::centred, false);
    }
}

/* ------------------------------------------------------------------------------- the knob --- */
/* a knob lit from the top left: its soft shadow to the bottom right, the black knurled skirt (its ridges catching the
 * light on the lit side), the spun aluminium cap (fine concentric turning, the bow-tie reflection a spun surface
 * shows, a chamfered rim) */
static void paintKnob(juce::Graphics &g, juce::Rectangle<float> r)
{
    using namespace juce;
    auto c = r.getCentre();
    float R = r.getWidth() * 0.5f;
    /* the shadow */
    for (int i = 0; i < 10; i++) {
        float e = R * (0.02f + 0.045f * (float)i);
        g.setColour(Colours::black.withAlpha(0.06f));
        g.fillEllipse(r.translated(R * 0.07f, R * 0.12f).expanded(e));
    }
    /* the skirt */
    g.setGradientFill(ColourGradient(Colour(0xff3c3d41), c.x - R * 0.6f, c.y - R * 0.6f, Colour(0xff050506), c.x + R * 0.7f, c.y + R * 0.8f, false));
    g.fillEllipse(r);
    for (int i = 0; i < 60; i++) {
        float a = (float)i / 60.0f * MathConstants<float>::twoPi;
        float lit = 0.5f + 0.5f * std::cos(a + 2.35f);
        float s = std::sin(a), co = std::cos(a);
        g.setColour(Colours::white.withAlpha(0.05f + 0.22f * lit * lit));
        g.drawLine(c.x + co * R * 0.84f, c.y + s * R * 0.84f, c.x + co * R * 0.985f, c.y + s * R * 0.985f, juce::jmax(0.6f, R * 0.025f));
    }
    g.setColour(Colours::black.withAlpha(0.6f));
    g.drawEllipse(r.reduced(0.5f), 1.0f);
    /* the cap */
    float rc = R * 0.76f;
    auto cap = Rectangle<float>(c.x - rc, c.y - rc, rc * 2, rc * 2);
    g.setColour(Colours::black.withAlpha(0.5f));
    g.fillEllipse(cap.expanded(1.6f).translated(0.6f, 1.0f));
    /* the reflection of a spun surface: bright along one axis, dark across it */
    const int NW = 144;
    for (int i = 0; i < NW; i++) {
        float a0 = (float)i / NW * MathConstants<float>::twoPi, a1 = (float)(i + 1) / NW * MathConstants<float>::twoPi + 0.01f;
        float am = 0.5f * (a0 + a1);
        float refl = 0.5f + 0.5f * std::cos(2.0f * (am - 0.7f));
        refl = refl * refl;
        float side = 0.5f + 0.5f * std::cos(am + 2.3f);          /* (the lit half a little brighter) */
        float v = 0.50f + 0.40f * refl + 0.08f * side;
        Path w;
        w.addPieSegment(cap, a0, a1, 0.0f);
        g.setColour(Colour::fromFloatRGBA(v * 0.98f, v * 0.99f, v, 1.0f));
        g.fillPath(w);
    }
    /* the turning: fine concentric rings */
    Random rnd(11);
    for (float rr = rc * 0.08f; rr < rc; rr += juce::jmax(0.7f, rc / 46.0f)) {
        bool lightRing = rnd.nextBool();
        g.setColour((lightRing ? Colours::white : Colours::black).withAlpha(0.025f + rnd.nextFloat() * 0.05f));
        g.drawEllipse(c.x - rr, c.y - rr, rr * 2, rr * 2, 0.6f);
    }
    /* a soft overall light from the top left, the rim's chamfer */
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.22f), cap.getX(), cap.getY(), Colours::black.withAlpha(0.18f), cap.getRight(), cap.getBottom(), false));
    g.fillEllipse(cap);
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.75f), cap.getX(), cap.getY(), Colour(0xff2a2b2e), cap.getRight(), cap.getBottom(), false));
    g.drawEllipse(cap.reduced(0.6f), juce::jmax(1.0f, rc * 0.05f));
    g.setColour(Colours::black.withAlpha(0.18f));
    g.fillEllipse(c.x - rc * 0.05f, c.y - rc * 0.05f, rc * 0.1f, rc * 0.1f);
}
void drawKnobBody(juce::Graphics &g, juce::Rectangle<float> r)
{
    float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
    int key = (int)std::lround(r.getWidth() * sc);
    auto &cache = get().knob;
    auto it = cache.find(key);
    const int pad = juce::jmax(6, key / 4);
    if (it == cache.end()) {
        int n = key + 2 * pad;
        juce::Image made(juce::Image::ARGB, n, n, true, juce::SoftwareImageType());
        {
            juce::Graphics kg(made);
            paintKnob(kg, juce::Rectangle<float>((float)pad, (float)pad, (float)key, (float)key));
        }
        it = cache.emplace(key, made).first;
    }
    const juce::Image &img = it->second;
    float px = r.getWidth() / (float)key;
    g.setOpacity(1.0f);
    g.drawImage(img, juce::Rectangle<float>(r.getX() - pad * px, r.getY() - pad * px, img.getWidth() * px, img.getHeight() * px),
                juce::RectanglePlacement::stretchToFit);
}
void drawKnobPointer(juce::Graphics &g, juce::Rectangle<float> r, float pos)
{
    using namespace juce;
    auto c = r.getCentre();
    float R = r.getWidth() * 0.5f;
    float a = (-0.75f + 1.5f * juce::jlimit(0.0f, 1.0f, pos)) * MathConstants<float>::pi;
    float s = std::sin(a), co = -std::cos(a);
    float w = juce::jmax(1.8f, R * 0.07f);
    /* an engraved line: dark, with its lower edge catching light */
    g.setColour(Colours::white.withAlpha(0.45f));
    g.drawLine(c.x + s * R * 0.36f + 0.6f, c.y + co * R * 0.36f + 0.9f, c.x + s * R * 0.74f + 0.6f, c.y + co * R * 0.74f + 0.9f, w * 0.6f);
    g.setColour(Colour(0xff18191b));
    g.drawLine(c.x + s * R * 0.36f, c.y + co * R * 0.36f, c.x + s * R * 0.74f, c.y + co * R * 0.74f, w);
    g.setColour(Colours::white.withAlpha(0.8f));
    g.drawLine(c.x + s * R * 0.84f, c.y + co * R * 0.84f, c.x + s * R * 0.98f, c.y + co * R * 0.98f, juce::jmax(1.4f, R * 0.045f));
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
    auto b = down ? r.translated(0.4f, 1.0f) : r;
    /* its shadow on the panel (light from the top left) */
    for (int i = 0; i < 5; i++) {
        g.setColour(Colours::black.withAlpha(down ? 0.05f : 0.08f));
        g.fillRoundedRectangle(r.translated(1.0f + i * 0.5f, 2.0f + i * 0.7f).expanded((float)i * 0.6f), 5.0f);
    }
    /* the bezel, then the soft rubber top */
    g.setGradientFill(ColourGradient(Colour(0xff4a4b50), b.getX(), b.getY(), Colour(0xff121214), b.getRight(), b.getBottom(), false));
    g.fillRoundedRectangle(b, 4.5f);
    auto face = b.reduced(2.2f);
    g.setGradientFill(ColourGradient(Colour(0xff2a2b2f), face.getX(), face.getY(), Colour(0xff17181a), face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle(face, 3.5f);
    g.setColour(Colours::white.withAlpha(0.06f));
    g.fillRoundedRectangle(face.withHeight(face.getHeight() * 0.45f).reduced(1.5f, 0.8f), 3.0f);
    Colour lit = kind == Lit::orange ? col::orangeLit : kind == Lit::blue ? col::blueLit : col::whiteLit;
    if (on && text.isEmpty()) {
        /* a blank cap lit from inside (the Master Control's) */
        g.setColour(lit.withAlpha(0.22f));
        g.fillRoundedRectangle(face.expanded(2.0f), 5.0f);
        g.setGradientFill(ColourGradient(lit, face.getCentreX(), face.getY(), lit.darker(0.25f), face.getCentreX(), face.getBottom(), false));
        g.fillRoundedRectangle(face.reduced(1.5f), 2.5f);
    }
    if (text.isNotEmpty()) {
        Colour tc = on ? lit : (kind == Lit::orange ? col::orangeDim : (dimLegend ? Colour(0xff8a8a8e) : col::legend));
        g.setFont(font(Face::heavy, textSize));
        if (on) {
            g.setColour(tc.withAlpha(0.3f));
            for (int k = 0; k < 4; k++)
                g.drawFittedText(text, face.translated((k & 1) ? 0.8f : -0.8f, (k & 2) ? 0.8f : -0.8f).toNearestInt(), Justification::centred, 2, 0.8f);
        }
        g.setColour(tc);
        g.drawFittedText(text, face.toNearestInt(), Justification::centred, 2, 0.8f);
    }
    g.setColour(Colours::white.withAlpha(0.12f));
    g.drawLine(b.getX() + 4, b.getY() + 0.6f, b.getRight() - 4, b.getY() + 0.6f, 0.8f);
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
