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
    float density = 1.0f;
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
/* the textures, made once per pixel density. Real surfaces are relief, not tint: a height field of noise, lit from the
 * top left (each texel brighter where it faces the light, darker where it turns away), reads as material.
 *   grain  the powder coat's orange peel (soft bumps a few pixels wide) over the steel's brushing (long streaks)
 *   mottle large, faint tone differences of the coat (the panel never quite one colour)
 *   rubber the buttons' fine, matte stipple */
namespace
{
void boxBlur(std::vector<float> &h, int n, int rad, bool horizontalOnly = false)
{
    std::vector<float> t((size_t)(n * n));
    for (int pass = 0; pass < 2; pass++) {
        for (int y = 0; y < n; y++) {
            float acc = 0.0f;
            for (int k = -rad; k <= rad; k++)
                acc += h[(size_t)(y * n + ((k + n) % n))];
            for (int x = 0; x < n; x++) {
                t[(size_t)(y * n + x)] = acc / (float)(2 * rad + 1);
                acc += h[(size_t)(y * n + ((x + rad + 1) % n))] - h[(size_t)(y * n + ((x - rad + n) % n))];
            }
        }
        if (horizontalOnly) {
            h = t;
            continue;
        }
        for (int x = 0; x < n; x++) {
            float acc = 0.0f;
            for (int k = -rad; k <= rad; k++)
                acc += t[(size_t)(((k + n) % n) * n + x)];
            for (int y = 0; y < n; y++) {
                h[(size_t)(y * n + x)] = acc / (float)(2 * rad + 1);
                acc += t[(size_t)(((y + rad + 1) % n) * n + x)] - t[(size_t)(((y - rad + n) % n) * n + x)];
            }
        }
    }
}
/* a height field lit from the top left, into white / black texels of the given strength */
juce::Image relief(const std::vector<float> &h, int n, float strength, float flatNoise, juce::Random &rnd)
{
    juce::Image img(juce::Image::ARGB, n, n, true, juce::SoftwareImageType());
    juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            float a = h[(size_t)(((y - 1 + n) % n) * n + (x - 1 + n) % n)], b = h[(size_t)(((y + 1) % n) * n + (x + 1) % n)];
            float v = (a - b) * strength + (rnd.nextFloat() - 0.5f) * flatNoise;
            int al = juce::jlimit(0, 255, (int)(std::fabs(v) * 255.0f));
            bd.setPixelColour(x, y, v > 0 ? juce::Colour::fromRGBA(255, 255, 255, (uint8_t)al) : juce::Colour::fromRGBA(0, 0, 0, (uint8_t)al));
        }
    return img;
}
} // namespace
static const juce::Image &grain(float density)
{
    int key = juce::jlimit(1, 8, (int)std::lround(density * 2.0f));
    auto &img = get().grain[key];
    if (img.isNull()) {
        const int n = 512;
        juce::Random rnd(1234 + key);
        /* the orange peel: bumps two to four texels wide */
        std::vector<float> peel((size_t)(n * n)), brush((size_t)(n * n));
        for (auto &v : peel) v = rnd.nextFloat();
        boxBlur(peel, n, juce::jmax(1, (int)std::lround(1.2f * density)));
        /* the brushing under it: long horizontal streaks */
        for (auto &v : brush) v = rnd.nextFloat();
        boxBlur(brush, n, 28, true);
        std::vector<float> h((size_t)(n * n));
        for (size_t i = 0; i < h.size(); i++)
            h[i] = peel[i] * 1.0f + brush[i] * 0.9f;
        img = relief(h, n, 0.42f, 0.035f, rnd);
        /* pores and pinholes of the coat */
        juce::Graphics g(img);
        for (int i = 0; i < 380; i++) {
            float x = rnd.nextFloat() * n, y = rnd.nextFloat() * n, r = 0.5f + rnd.nextFloat() * 0.9f;
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            g.fillEllipse(x - r, y - r, 2 * r, 2 * r);
            g.setColour(juce::Colours::white.withAlpha(0.14f));
            g.fillEllipse(x - r + 0.6f, y - r + 0.8f, 2 * r, 2 * r);
        }
    }
    return img;
}
static const juce::Image &mottle()
{
    auto &img = get().grain[100];
    if (img.isNull()) {
        const int n = 128;
        juce::Random rnd(99);
        img = juce::Image(juce::Image::ARGB, n, n, true, juce::SoftwareImageType());
        std::vector<float> a((size_t)(n * n)), b((size_t)(n * n));
        for (auto &v : a) v = rnd.nextFloat();
        for (auto &v : b) v = rnd.nextFloat();
        boxBlur(a, n, 9);
        boxBlur(b, n, 3);
        juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                float v = (a[(size_t)(y * n + x)] - 0.5f) * 3.2f + (b[(size_t)(y * n + x)] - 0.5f) * 1.2f;
                int al = juce::jlimit(0, 255, (int)(std::fabs(v) * 255.0f * 0.12f));
                bd.setPixelColour(x, y, v > 0 ? juce::Colour::fromRGBA(255, 250, 240, (uint8_t)al) : juce::Colour::fromRGBA(0, 0, 8, (uint8_t)al));
            }
    }
    return img;
}
static const juce::Image &sanded(float density)
{
    int key = 200 + juce::jlimit(1, 8, (int)std::lround(density * 2.0f));
    auto &img = get().grain[key];
    if (img.isNull()) {
        /* sanded plastic: fine, even, no direction; tiny lit bumps and a few deeper sanding marks */
        const int n = 384;
        juce::Random rnd(77 + key);
        std::vector<float> h((size_t)(n * n));
        for (auto &v : h) v = rnd.nextFloat();
        boxBlur(h, n, 1);
        img = relief(h, n, 0.32f, 0.05f, rnd);
        juce::Graphics g(img);
        for (int i = 0; i < 260; i++) {
            float x = rnd.nextFloat() * n, y = rnd.nextFloat() * n, a = rnd.nextFloat() * 6.3f, len = 2.0f + rnd.nextFloat() * 7.0f;
            g.setColour(juce::Colours::white.withAlpha(0.05f + rnd.nextFloat() * 0.05f));
            g.drawLine(x, y, x + std::cos(a) * len, y + std::sin(a) * len, 0.5f);
        }
    }
    return img;
}
static const juce::Image &rubber()
{
    auto &img = get().grain[101];
    if (img.isNull()) {
        const int n = 128;
        juce::Random rnd(5);
        std::vector<float> h((size_t)(n * n));
        for (auto &v : h) v = rnd.nextFloat();
        boxBlur(h, n, 1);
        img = relief(h, n, 0.45f, 0.04f, rnd);
    }
    return img;
}
void drawPanel(juce::Graphics &g, juce::Rectangle<float> r, float)
{
    using namespace juce;
    /* anthracite steel, lighter where the light falls (top left) */
    g.setGradientFill(ColourGradient(Colour(0xff3a3c41), r.getX(), r.getY(), Colour(0xff222326), r.getRight(), r.getBottom(), false));
    g.fillRect(r);
}
void drawMetal(juce::Graphics &g, juce::Rectangle<float> r, float density)
{
    using namespace juce;
    get().density = density;
    /* the coat's large tone differences, stretched soft over the whole panel */
    g.setOpacity(1.0f);
    g.setImageResamplingQuality(Graphics::highResamplingQuality);
    g.drawImage(mottle(), r, RectanglePlacement::stretchToFit);
    /* its relief: orange peel over the brushing, one texel a screen pixel */
    const Image &gr = grain(density);
    float tile = (float)gr.getWidth() / juce::jmax(0.5f, density);
    {
        Graphics::ScopedSaveState s(g);
        g.reduceClipRegion(r.toNearestInt());
        g.setOpacity(0.9f);
        for (float y = r.getY(); y < r.getBottom(); y += tile)
            for (float x = r.getX(); x < r.getRight(); x += tile)
                g.drawImage(gr, Rectangle<float>(x, y, tile, tile), RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);
    }
}
void drawSurface(juce::Graphics &g, juce::Rectangle<float> r, float)
{
    using namespace juce;
    float W = r.getWidth(), H = r.getHeight();
    Random rnd(4711);
    /* old fingerprints and wiping marks: large, very faint, a little greasy (brighter in the light) */
    for (int i = 0; i < 16; i++) {
        float cx = r.getX() + rnd.nextFloat() * W, cy = r.getY() + rnd.nextFloat() * H, rad = 22.0f + rnd.nextFloat() * 70.0f;
        g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.025f + rnd.nextFloat() * 0.02f), cx, cy, Colours::transparentWhite, cx + rad, cy, true));
        g.fillEllipse(cx - rad, cy - rad * 0.65f, rad * 2, rad * 1.3f);
        /* a print's ridges */
        if (i % 3 == 0)
            for (int k = 1; k < 9; k++) {
                g.setColour(Colours::white.withAlpha(0.018f));
                g.drawEllipse(cx - k * 1.8f, cy - k * 2.2f, k * 3.6f, k * 4.4f, 0.5f);
            }
    }
    /* fine scratches, mostly along the brushing, each with its dark lower edge */
    for (int i = 0; i < 110; i++) {
        float x = r.getX() + rnd.nextFloat() * W, y = r.getY() + rnd.nextFloat() * H;
        float len = 5.0f + rnd.nextFloat() * rnd.nextFloat() * 70.0f, ang = (rnd.nextFloat() - 0.5f) * (rnd.nextFloat() < 0.7f ? 0.2f : 3.0f);
        float x2 = x + std::cos(ang) * len, y2 = y + std::sin(ang) * len, a = 0.03f + rnd.nextFloat() * 0.07f;
        g.setColour(Colours::black.withAlpha(a * 0.8f));
        g.drawLine(x + 0.4f, y + 0.6f, x2 + 0.4f, y2 + 0.6f, 0.45f);
        g.setColour(Colours::white.withAlpha(a));
        g.drawLine(x, y, x2, y2, 0.35f + rnd.nextFloat() * 0.25f);
    }
    /* dust: specks sitting on the surface (a soft shadow to the bottom right, a lit top), more low down and along
     * the edges; a few fibres */
    for (int i = 0; i < 3200; i++) {
        float x = r.getX() + rnd.nextFloat() * W;
        float y = r.getY() + std::pow(rnd.nextFloat(), 0.65f) * H;
        if (rnd.nextFloat() < 0.2f)
            x = rnd.nextBool() ? r.getX() + rnd.nextFloat() * 30.0f : r.getRight() - rnd.nextFloat() * 30.0f;
        float sz = 0.3f + rnd.nextFloat() * rnd.nextFloat() * rnd.nextFloat() * 2.6f;
        float a = 0.10f + rnd.nextFloat() * 0.35f;
        if (sz > 0.9f) {
            g.setColour(Colours::black.withAlpha(a * 0.45f));
            g.fillEllipse(x - sz * 0.5f + sz * 0.35f, y - sz * 0.5f + sz * 0.5f, sz * 1.1f, sz * 1.1f);
        }
        bool light = rnd.nextFloat() < 0.82f;
        g.setColour((light ? Colour(0xffd9d6ce) : Colour(0xff151412)).withAlpha(a));
        g.fillEllipse(x - sz * 0.5f, y - sz * 0.5f, sz, sz * (0.7f + rnd.nextFloat() * 0.6f));
    }
    for (int i = 0; i < 34; i++) {
        float x = r.getX() + rnd.nextFloat() * W, y = r.getY() + std::pow(rnd.nextFloat(), 0.6f) * H;
        Path f;
        f.startNewSubPath(x, y);
        f.cubicTo(x + rnd.nextFloat() * 14 - 7, y + rnd.nextFloat() * 14 - 7, x + rnd.nextFloat() * 24 - 12, y + rnd.nextFloat() * 24 - 12,
                  x + rnd.nextFloat() * 30 - 15, y + rnd.nextFloat() * 30 - 15);
        g.setColour(Colours::black.withAlpha(0.12f));
        g.strokePath(f, PathStrokeType(0.5f), AffineTransform::translation(0.5f, 0.7f));
        g.setColour(Colour(0xffd8d6d0).withAlpha(0.14f + rnd.nextFloat() * 0.14f));
        g.strokePath(f, PathStrokeType(0.45f));
    }
    /* the light: a broad soft pool from the top left, the satin sheen of a softbox across the panel, the far corners
     * falling off */
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.08f), r.getX() + W * 0.2f, r.getY() - H * 0.12f, Colours::transparentWhite,
                                     r.getX() + W * 0.95f, r.getY() + H * 0.95f, true));
    g.fillRect(r);
    {
        ColourGradient sheen(Colours::transparentWhite, r.getX(), r.getY() + H * 0.05f, Colours::transparentWhite, r.getX() + W * 0.55f, r.getY() + H * 0.95f, false);
        sheen.addColour(0.42, Colours::white.withAlpha(0.0f));
        sheen.addColour(0.52, Colours::white.withAlpha(0.03f));
        sheen.addColour(0.60, Colours::white.withAlpha(0.0f));
        g.setGradientFill(sheen);
        g.fillRect(r);
    }
    g.setGradientFill(ColourGradient(Colours::transparentBlack, r.getCentreX(), r.getCentreY(), Colours::black.withAlpha(0.38f),
                                     r.getX() - W * 0.05f, r.getY() - H * 0.05f, true));
    g.fillRect(r);
    /* the panel's bevelled edge */
    g.setColour(Colours::white.withAlpha(0.2f));
    g.drawLine(r.getX(), r.getY() + 0.5f, r.getRight(), r.getY() + 0.5f, 1.0f);
    g.drawLine(r.getX() + 0.5f, r.getY(), r.getX() + 0.5f, r.getBottom(), 1.0f);
    g.setColour(Colours::black.withAlpha(0.55f));
    g.drawLine(r.getX(), r.getBottom() - 0.5f, r.getRight(), r.getBottom() - 0.5f, 1.5f);
    g.drawLine(r.getRight() - 0.5f, r.getY(), r.getRight() - 0.5f, r.getBottom(), 1.5f);
}
void drawWell(juce::Graphics &g, juce::Rectangle<float> r, const juce::String &title)
{
    using namespace juce;
    /* an inlay of dark sanded plastic, set into the steel: the steel's cut edge around it (shadowed on the top and
     * left, lit on the bottom and right), the plastic standing a little proud with a soft moulded rim */
    Path cut;
    cut.addRoundedRectangle(r.expanded(2.5f), 9.5f);
    g.setColour(Colours::black.withAlpha(0.7f));
    g.fillPath(cut);
    g.setColour(Colours::white.withAlpha(0.16f));
    g.drawLine(r.getX() + 10, r.getBottom() + 2.6f, r.getRight() - 6, r.getBottom() + 2.6f, 1.0f);
    g.drawLine(r.getRight() + 2.6f, r.getY() + 10, r.getRight() + 2.6f, r.getBottom() - 6, 1.0f);
    Path p;
    p.addRoundedRectangle(r, 8.0f);
    g.setGradientFill(ColourGradient(Colour(0xff1f2023), r.getX(), r.getY(), Colour(0xff141517), r.getRight(), r.getBottom(), false));
    g.fillPath(p);
    {
        Graphics::ScopedSaveState s(g);
        g.reduceClipRegion(p);
        float d = get().density;
        const Image &sd = sanded(d);
        float tile = (float)sd.getWidth() / juce::jmax(0.5f, d);
        g.setOpacity(0.9f);
        for (float y = r.getY(); y < r.getBottom(); y += tile)
            for (float x = r.getX(); x < r.getRight(); x += tile)
                g.drawImage(sd, Rectangle<float>(x, y, tile, tile), RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);
        /* matte plastic scatters the light: a broad soft glow, no sharp sheen */
        g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.05f), r.getX(), r.getY(), Colours::transparentWhite,
                                         r.getX() + r.getWidth() * 0.8f, r.getBottom(), false));
        g.fillPath(p);
    }
    /* the moulded rim: light on the top and left, its underside dark */
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.22f), r.getX(), r.getY(), Colours::black.withAlpha(0.5f), r.getRight(), r.getBottom(), false));
    g.strokePath(p, PathStrokeType(1.4f));
    if (title.isNotEmpty()) {
        auto band = r.withHeight(30.0f).reduced(5.0f, 5.0f);
        g.setColour(Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(band, 4.0f);
        g.setColour(Colours::white.withAlpha(0.06f));
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
    /* the shadow: tight and dark where it meets the panel, wide and faint further out */
    g.setColour(Colours::black.withAlpha(0.45f));
    g.fillEllipse(r.translated(R * 0.03f, R * 0.06f).expanded(R * 0.01f));
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
    /* a life of use: fine circular scuffs and a few straight scratches on the aluminium */
    for (int i = 0; i < 46; i++) {
        float rr = rc * (0.15f + 0.8f * rnd.nextFloat()), a0 = rnd.nextFloat() * MathConstants<float>::twoPi, span = 0.1f + rnd.nextFloat() * 0.6f;
        Path arc;
        arc.addCentredArc(c.x, c.y, rr, rr, 0.0f, a0, a0 + span, true);
        g.setColour((rnd.nextFloat() < 0.6f ? Colours::white : Colours::black).withAlpha(0.05f + rnd.nextFloat() * 0.08f));
        g.strokePath(arc, PathStrokeType(juce::jmax(0.4f, rc * 0.008f)));
    }
    for (int i = 0; i < 6; i++) {
        float a = rnd.nextFloat() * MathConstants<float>::twoPi, d = rnd.nextFloat() * rc * 0.6f, len = rc * (0.15f + 0.3f * rnd.nextFloat());
        float x = c.x + std::cos(a) * d, y = c.y + std::sin(a) * d, b = rnd.nextFloat() * 3.0f;
        g.setColour(Colours::white.withAlpha(0.12f));
        g.drawLine(x, y, x + std::cos(b) * len, y + std::sin(b) * len, 0.5f);
    }
    /* the metal's own fine grain */
    for (int i = 0; i < (int)(rc * rc * 0.35f); i++) {
        float a = rnd.nextFloat() * MathConstants<float>::twoPi, d = std::sqrt(rnd.nextFloat()) * rc * 0.97f;
        g.setColour((rnd.nextBool() ? Colours::white : Colours::black).withAlpha(0.05f + rnd.nextFloat() * 0.06f));
        g.fillRect(c.x + std::cos(a) * d, c.y + std::sin(a) * d, 0.7f, 0.7f);
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
    {
        /* the rubber's matte stipple */
        Graphics::ScopedSaveState ss(g);
        Path fp;
        fp.addRoundedRectangle(face, 3.5f);
        g.reduceClipRegion(fp);
        g.setOpacity(0.4f);
        const Image &rb = rubber();
        float t = 64.0f;
        for (float y = face.getY(); y < face.getBottom(); y += t)
            for (float x = face.getX(); x < face.getRight(); x += t)
                g.drawImage(rb, Rectangle<float>(x, y, t, t), RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);
    }
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
