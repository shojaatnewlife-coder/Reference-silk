// reference · Silk — the window
#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

// ------------------------------------------------------------------ themes
static Colour c (uint32 argb) { return Colour (argb); }

const SilkTheme& SilkTheme::get (int index)
{
    static const SilkTheme themes[SilkProcessor::kNumThemes] = {
        { "BLACK", c (0xff070707), c (0xff040404), c (0x00000000),
          c (0x29ffffff), c (0xfff2f2f2), c (0xbfffffff), c (0x6bffffff), c (0x8cffffff), c (0xffffffff), c (0xffffffff),
          c (0xffff2a4a), c (0xffc8ff6a),
          c (0xffff2a4a), { c (0xffff8a80), c (0xffe3141f), c (0xff5a0008) },
          c (0xff2f8bff), { c (0xff8cc4ff), c (0xff1c64e6), c (0xff04123a) }, false, true },
        { "GREY", c (0xff3a4955), c (0xff26323c), c (0xff4a5c6b),
          c (0x2effffff), c (0xffe9ecef), c (0xffaebccb), c (0xff8395a6), c (0xffaebccb), c (0xffffffff), c (0xffffffff),
          c (0xffff3b3b), c (0xff9fe3bd),
          c (0xffff3b3b), { c (0xffff8a80), c (0xffe3141f), c (0xff5a0008) },
          c (0xff5aa9ff), { c (0xffa8d2ff), c (0xff2a74e8), c (0xff0a1d4a) }, false, false },
        { "WALNUT", c (0xff2a1c14), c (0xff160f0b), c (0xff3a271b),
          c (0x40f0d9a8), c (0xfff3e6cc), c (0xffc9b48f), c (0xff8f7b5e), c (0xffb89c70), c (0xfff0d9a8), c (0xffffffff),
          c (0xffff4a3d), c (0xfff0d9a8),
          c (0xffff4a3d), { c (0xffffa08a), c (0xffd8261a), c (0xff4a0a05) },
          c (0xff4f8fe8), { c (0xffa5c8ff), c (0xff2a63c4), c (0xff0a1838) }, false, false },
        { "IVORY", c (0xfff4f1ea), c (0xffe6e2d9), c (0xffffffff),
          c (0x2e000000), c (0xff1d1f22), c (0xff55595f), c (0xff8a8d92), c (0xff6a6e74), c (0xff1d1f22), c (0xff1d1f22),
          c (0xffe3141f), c (0xff2fb36b),
          c (0xffe3141f), { c (0xffff8a80), c (0xffe3141f), c (0xff5a0008) },
          c (0xff1f6fe0), { c (0xff9cc6ff), c (0xff1f6fe0), c (0xff06173d) }, true, false },
        { "NEON", c (0xff030303), c (0xff030303), c (0x00000000),
          c (0x59a6ff78), c (0xffeaffdf), c (0xffb8d9a8), c (0xff6f8a62), c (0xff8fcf6a), c (0xffd6ffc4), c (0xffffffff),
          c (0xffff4fa3), c (0xffa6ff78),
          c (0xffff4fa3), { c (0xffffb3d6), c (0xffff2f8f), c (0xff4a0026) },
          c (0xff3fe6ff), { c (0xffb8f6ff), c (0xff14b8e0), c (0xff03303d) }, false, false },
        { "MIDNIGHT", c (0xff0b0f24), c (0xff05060f), c (0xff141a3a),
          c (0x38bec8ff), c (0xffe8ebff), c (0xff9aa3d0), c (0xff6a7299), c (0xff8a93c0), c (0xffe8ebff), c (0xffffffff),
          c (0xffff3b5c), c (0xffc8ff6a),
          c (0xffff3b5c), { c (0xffff9aa9), c (0xffe01a3c), c (0xff4a0512) },
          c (0xff6b8cff), { c (0xffc3d0ff), c (0xff3d5cf0), c (0xff0b1240) }, false, false },
    };
    return themes[jlimit (0, SilkProcessor::kNumThemes - 1, index)];
}

// ------------------------------------------------------------------ fonts and drawing helpers
SilkFonts::SilkFonts()
    : thinFace (Typeface::createSystemTypefaceFor (BinaryData::InterExtraLight_otf, (size_t) BinaryData::InterExtraLight_otfSize)),
      lightFace (Typeface::createSystemTypefaceFor (BinaryData::InterLight_otf, (size_t) BinaryData::InterLight_otfSize))
{
}

static Font makeFont (const Typeface::Ptr& tf, float height, float tracking)
{
    const auto base = tf != nullptr ? FontOptions (tf) : FontOptions();
    return Font (base.withHeight (height).withKerningFactor (tracking));
}
Font SilkFonts::thin (float h, float t) const { return makeFont (thinFace, h, t); }
Font SilkFonts::light (float h, float t) const { return makeFont (lightFace, h, t); }

static void glowStroke (Graphics& g, const Path& p, Colour col, float width, float glow)
{
    const float layers[][2] = { { 6.0f, 0.05f }, { 3.5f, 0.09f }, { 2.0f, 0.16f } };
    for (auto& l : layers)
    {
        g.setColour (col.withMultipliedAlpha (jmin (1.0f, l[1] * glow)));
        g.strokePath (p, PathStrokeType (width * l[0], PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (col);
    g.strokePath (p, PathStrokeType (width, PathStrokeType::curved, PathStrokeType::rounded));
}

static void glowDot (Graphics& g, Point<float> centre, float radius, Colour col, Colour halo, float glow)
{
    for (int i = 3; i >= 1; --i)
    {
        const float r = radius * (1.0f + (float) i * 0.7f);
        g.setColour (halo.withMultipliedAlpha (jmin (1.0f, 0.12f * glow)));
        g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (centre));
    }
    g.setColour (col);
    g.fillEllipse (Rectangle<float> (radius * 2, radius * 2).withCentre (centre));
}

/** Text with spacing after the last letter removed, so centred text really sits in the middle. */
static void spacedText (Graphics& g, const Font& f, const String& s, Rectangle<float> r, Justification j, Colour col, float glow = 0.0f)
{
    GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    const auto box = ga.getBoundingBox (0, -1, true);
    const float ink = box.getWidth() - f.getHeight() * f.getExtraKerningFactor();
    float x = r.getX();
    if (j.testFlags (Justification::horizontallyCentred)) x = r.getCentreX() - ink * 0.5f;
    else if (j.testFlags (Justification::right)) x = r.getRight() - ink;
    const float y = r.getCentreY() + (f.getAscent() - f.getDescent()) * 0.5f;
    Path p;
    ga.createPath (p);
    p.applyTransform (AffineTransform::translation (x - box.getX(), y));
    if (glow > 0.0f)
    {
        g.setColour (col.withMultipliedAlpha (0.18f * glow));
        g.strokePath (p, PathStrokeType (f.getHeight() * 0.12f));
    }
    g.setColour (col);
    g.fillPath (p);
}

static const String kDot = String::fromUTF8 ("\xc2\xb7");
static const String kDash = String::fromUTF8 ("\xe2\x80\x93");

// ------------------------------------------------------------------ PillSwitch
PillSwitch::PillSwitch (String l, bool b) : label (std::move (l)), big (b)
{
    setClickingTogglesState (true);
}

void PillSwitch::paintButton (Graphics& g, bool highlighted, bool)
{
    if (theme == nullptr)
        return;
    const auto& t = *theme;
    const float refW = big ? 150.0f : 62.0f, refH = big ? 34.0f : 21.0f;
    g.addTransform (AffineTransform::scale (getWidth() / refW));
    const bool on = getToggleState();
    const auto r = Rectangle<float> (refW, refH).reduced (1.5f);
    const float glow = t.light ? 0.45f : 1.0f;

    Colour edge = big ? t.text.withMultipliedAlpha (on ? 0.6f : 0.25f) : (on ? accent : t.faint.withMultipliedAlpha (0.6f));
    if (highlighted) edge = edge.withMultipliedAlpha (1.25f);
    Path pill;
    pill.addRoundedRectangle (r, r.getHeight() * 0.5f);
    if (on)
        for (int i = 2; i >= 1; --i)
        {
            g.setColour ((big ? t.text : accent).withMultipliedAlpha (0.05f * glow));
            g.strokePath (pill, PathStrokeType (2.0f + 2.5f * (float) i));
        }
    g.setColour (edge);
    g.strokePath (pill, PathStrokeType (0.65f));

    const String shown = big ? label : (on ? "ON" : "OFF");
    const auto font = big ? fonts->light (11.0f, 0.45f) : fonts->light (8.5f, 0.47f);
    const float textW = GlyphArrangement::getStringWidth (font, shown) - font.getHeight() * font.getExtraKerningFactor();
    const float dotR = big ? 3.0f : 2.4f, gap = big ? 10.0f : 7.0f;
    const float x0 = r.getCentreX() - (dotR * 2 + gap + textW) * 0.5f;
    const Point<float> dc (x0 + dotR, r.getCentreY());
    const Colour dotCol = big ? t.powerDot : accent;
    if (on)
        glowDot (g, dc, dotR, dotCol, dotCol, glow * 1.5f);
    else
    {
        g.setColour (t.faint.withMultipliedAlpha (0.8f));
        g.drawEllipse (Rectangle<float> (dotR * 2, dotR * 2).withCentre (dc), 0.6f);
    }
    spacedText (g, font, shown, { x0 + dotR * 2 + gap, r.getY(), textW + 4, r.getHeight() }, Justification::centredLeft,
                on ? t.text : t.faint);
}

// ------------------------------------------------------------------ SilkKnob
SilkKnob::SilkKnob (bool red) : isRed (red)
{
    setSliderStyle (RotaryHorizontalVerticalDrag);
    setTextBoxStyle (NoTextBox, true, 0, 0);
    setMouseDragSensitivity (260);
    setPopupDisplayEnabled (false, false, nullptr);
    setVelocityBasedMode (false);
    setMouseCursor (MouseCursor::UpDownResizeCursor);
}

void SilkKnob::setActive (bool a)
{
    if (a != active)
    {
        active = a;
        repaint();
    }
}

void SilkKnob::paint (Graphics& g)
{
    if (theme == nullptr)
        return;
    const auto& t = *theme;
    g.addTransform (AffineTransform::scale (getWidth() / kW));

    const float cx = kW * 0.5f, cy = kCy, R = 85.0f;
    const float a0 = MathConstants<float>::pi * 0.75f, a1 = MathConstants<float>::pi * 2.25f;
    const float frac = (float) jlimit (0.0, 1.0, (getValue() - getMinimum()) / jmax (1e-9, getMaximum() - getMinimum()));
    const float a = a0 + (a1 - a0) * frac;
    auto pt = [&] (float r, float ang) { return Point<float> (cx + r * std::cos (ang), cy + r * std::sin (ang)); };
    const float m = active ? 1.0f : 0.3f;
    const float glow = (t.light ? 0.5f : 1.0f) * m;
    Colour col = isRed ? t.red : t.blue;
    const Colour* ball = isRed ? t.redBall : t.blueBall;
    if (! active)
        col = col.withMultipliedSaturation (0.35f);

    // scale: 21 ticks, every fifth longer
    for (int i = 0; i <= 20; ++i)
    {
        const float ang = a0 + (a1 - a0) * (float) i / 20.0f;
        const bool major = i % 5 == 0;
        g.setColour (t.tick.withAlpha ((major ? 0.5f : 0.22f) * (active ? 1.0f : 0.6f)));
        g.drawLine (Line<float> (pt (R + 13.0f, ang), pt (R + (major ? 21.0f : 17.0f), ang)), 0.7f);
    }
    auto arc = [&] (float from, float to) {
        Path p;
        const int steps = jmax (2, (int) ((to - from) * 40.0f));
        for (int i = 0; i <= steps; ++i)
        {
            const auto q = pt (R, from + (to - from) * (float) i / (float) steps);
            if (i == 0) p.startNewSubPath (q);
            else p.lineTo (q);
        }
        return p;
    };
    g.setColour (t.tick.withAlpha (0.12f));
    g.strokePath (arc (a0, a1), PathStrokeType (0.75f));
    if (frac > 0.002f)
        glowStroke (g, arc (a0, a), col.withMultipliedAlpha (active ? 1.0f : 0.5f), 1.5f, glow * 1.6f);

    // the ball and its halo
    if (! t.light || active)
        for (int i = 4; i >= 1; --i)
        {
            const float r = 56.0f + 3.2f * (float) i;
            g.setColour (col.withAlpha ((t.light ? 0.035f : 0.06f) * m));
            g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre ({ cx, cy }));
        }
    {
        const float br = 56.0f;
        auto shade = [&] (Colour b) { return active ? b : b.withMultipliedSaturation (0.25f).withMultipliedBrightness (0.5f); };
        ColourGradient gr (shade (ball[0]), cx - 0.24f * br, cy - 0.36f * br, shade (ball[2]), cx - 0.24f * br + 1.5f * br, cy - 0.36f * br, true);
        gr.addColour (0.5, shade (ball[1]));
        g.setGradientFill (gr);
        g.fillEllipse (Rectangle<float> (br * 2, br * 2).withCentre ({ cx, cy }));
        g.setColour (Colours::white.withAlpha (active ? 0.28f : 0.14f));
        g.fillEllipse (Rectangle<float> (44.0f, 24.0f).withCentre ({ cx - 15.0f, cy - 29.0f }));
        g.setColour (t.text.withAlpha (0.10f));
        g.drawEllipse (Rectangle<float> (124.0f, 124.0f).withCentre ({ cx, cy }), 0.6f);
    }
    glowDot (g, pt (R, a), 3.5f, t.pointer.withMultipliedAlpha (active ? 1.0f : 0.5f), col, glow * 1.5f);

    spacedText (g, fonts->light (9.0f, 0.33f), "0", Rectangle<float> (40, 20).withCentre (pt (R + 35.0f, a0)), Justification::centred, t.faint);
    spacedText (g, fonts->light (9.0f, 0.33f), "100", Rectangle<float> (40, 20).withCentre (pt (R + 35.0f, a1)), Justification::centred, t.faint);

    // name, frequency, character, value
    const String name = isRed ? "RED" : "BLUE";
    const String freq = isRed ? "HIGHS " + kDot + " HIGH MIDS  " + kDot + "  2 " + kDash + " 16 kHz"
                              : "LOWS " + kDot + " LOW MIDS  " + kDot + "  40 " + kDash + " 400 Hz";
    const String desc = isRed ? "SHEEN  " + kDot + "  AIR  " + kDot + "  CLARITY" : "WARMTH  " + kDot + "  WEIGHT  " + kDot + "  THICKNESS";
    spacedText (g, fonts->light (23.0f, 0.48f), name, { 0, 238, kW, 28 }, Justification::centred, t.text.withMultipliedAlpha (active ? 1.0f : 0.55f));
    spacedText (g, fonts->light (11.0f, 0.41f), freq, { 0, 274, kW, 16 }, Justification::centred, t.dim);
    spacedText (g, fonts->light (8.5f, 0.41f), desc, { 0, 294, kW, 14 }, Justification::centred, t.faint);
    const String value = active ? String (roundToInt (getValue())) + " %" : "OFF";
    spacedText (g, fonts->thin (32.0f, 0.125f), value, { 0, 318, kW, 38 }, Justification::centred,
                active ? col : t.faint, active ? (t.light ? 0.5f : 1.0f) : 0.0f);
}

// ------------------------------------------------------------------ ThemeDots
static constexpr float kDotsW = 120.0f;

int ThemeDots::indexAt (Point<float> p) const
{
    const float s = getWidth() / kDotsW;
    const int i = (int) std::floor (p.x / s / 20.0f);
    return i >= 0 && i < SilkProcessor::kNumThemes ? i : -1;
}

void ThemeDots::paint (Graphics& g)
{
    g.addTransform (AffineTransform::scale (getWidth() / kDotsW));
    const auto& cur = SilkTheme::get (selected);
    for (int i = 0; i < SilkProcessor::kNumThemes; ++i)
    {
        const auto& t = SilkTheme::get (i);
        const Point<float> ctr (10.0f + 20.0f * (float) i, 10.0f);
        if (i == selected || i == hover)
        {
            g.setColour (cur.text.withAlpha (i == selected ? 0.7f : 0.3f));
            g.drawEllipse (Rectangle<float> (15.0f, 15.0f).withCentre (ctr), 0.6f);
        }
        const auto disc = Rectangle<float> (10.0f, 10.0f).withCentre (ctr);
        Path left, right;
        left.addPieSegment (disc, MathConstants<float>::pi, MathConstants<float>::twoPi, 0.0f);
        right.addPieSegment (disc, 0.0f, MathConstants<float>::pi, 0.0f);
        g.setColour (t.panelA.interpolatedWith (t.panelB, 0.5f));
        g.fillEllipse (disc);
        g.setColour (t.red.withAlpha (0.85f));
        g.fillPath (left, AffineTransform::scale (0.5f, 0.5f, ctr.x, ctr.y));
        g.setColour (t.blue.withAlpha (0.85f));
        g.fillPath (right, AffineTransform::scale (0.5f, 0.5f, ctr.x, ctr.y));
        g.setColour (t.border.withAlpha (jmax (0.35f, t.border.getFloatAlpha())));
        g.drawEllipse (disc, 0.6f);
    }
}

void ThemeDots::mouseMove (const MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i != hover)
    {
        hover = i;
        setTooltip (i >= 0 ? String ("THEME  ") + SilkTheme::get (i).name : String());
        repaint();
    }
}

void ThemeDots::mouseUp (const MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i >= 0 && onPick)
        onPick (i);
}

// ------------------------------------------------------------------ SilkEditor
static Rectangle<int> scaled (Rectangle<float> r, float s) { return (r * s).toNearestInt(); }

SilkEditor::SilkEditor (SilkProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      redA (p.getState(), ParamID::red, redKnob), blueA (p.getState(), ParamID::blue, blueKnob),
      redOnA (p.getState(), ParamID::redOn, redOn), blueOnA (p.getState(), ParamID::blueOn, blueOn),
      powerA (p.getState(), ParamID::power, power)
{
    redKnob.setDoubleClickReturnValue (true, 40.0);
    blueKnob.setDoubleClickReturnValue (true, 30.0);
    redKnob.setTooltip ("RED: silk on the highs and high mids. Drag up / down, double-click to reset.");
    blueKnob.setTooltip ("BLUE: silk on the lows and low mids. Drag up / down, double-click to reset.");
    redOn.setTooltip ("RED on / off");
    blueOn.setTooltip ("BLUE on / off");
    power.setTooltip ("POWER: off passes the sound through untouched");
    themes.onPick = [this] (int i) {
        proc.setTheme (i);
        applyTheme (i);
    };
    for (auto* c : std::initializer_list<Component*> { &redKnob, &blueKnob, &redOn, &blueOn, &power, &themes })
        addAndMakeVisible (c);

    addChildComponent (inviteBar);
    inviteBar.setTooltip (REF_FA ("Invite / دعوت"));
    inviteBar.setMouseCursor (MouseCursor::PointingHandCursor);
    inviteBar.onClick = [this] { showInvitePanel (true); };
    addAndMakeVisible (siteLink);
    siteLink.setTooltip ("shojaatnewlife-coder.github.io/reference");
    siteLink.onClick = [] { URL ("https://shojaatnewlife-coder.github.io/reference/").launchInDefaultBrowser(); };
    refreshInvite();

    applyTheme (proc.getTheme());
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (kRefW / kRefH);
    setResizeLimits (600, 384, 2000, 1280);
    setSize (900, 576);
    startTimerHz (20);
}

SilkEditor::~SilkEditor() { stopTimer(); }

void SilkEditor::refreshInvite()
{
    const auto i = proc.getInviteInfo();
    refinv::PanelInfo p { i.unlocked, i.demo, i.daysLeft, i.inviteCode, i.gaveCode };
    inviteBar.set (p);
}

void SilkEditor::showInvitePanel (bool show)
{
    if (! show)
    {
        invitePanel.reset();
        refreshInvite();
        return;
    }
    refinv::PanelHooks hooks;
    hooks.info = [this] {
        const auto i = proc.getInviteInfo();
        return refinv::PanelInfo { i.unlocked, i.demo, i.daysLeft, i.inviteCode, i.gaveCode };
    };
    hooks.give = [this] (const String& code, String& out) { return proc.giveThanks (code, out); };
    hooks.redeem = [this] (const String& code) { return proc.redeemThanks (code); };
    invitePanel = std::make_unique<refinv::InvitePanel> (std::move (hooks));
    const Component::SafePointer<SilkEditor> safe (this);
    invitePanel->onClose = [safe] { MessageManager::callAsync ([safe] { if (safe != nullptr) safe->showInvitePanel (false); }); };
    invitePanel->onChanged = [this] { refreshInvite(); };
    addAndMakeVisible (*invitePanel);
    invitePanel->setBounds (getLocalBounds());
}

void SilkEditor::applyTheme (int index)
{
    index = jlimit (0, SilkProcessor::kNumThemes - 1, index);
    if (index == themeIndex)
        return;
    themeIndex = index;
    theme = &SilkTheme::get (index);
    redKnob.setTheme (theme);
    blueKnob.setTheme (theme);
    redOn.setTheme (theme, theme->red);
    blueOn.setTheme (theme, theme->blue);
    power.setTheme (theme, theme->powerDot);
    themes.setSelected (index);
    renderBackground();
    repaint();
}

void SilkEditor::resized()
{
    const float s = getWidth() / kRefW;
    redKnob.setBounds (scaled ({ 250.0f - SilkKnob::kW * 0.5f, 298.0f - SilkKnob::kCy, SilkKnob::kW, SilkKnob::kH }, s));
    blueKnob.setBounds (scaled ({ 750.0f - SilkKnob::kW * 0.5f, 298.0f - SilkKnob::kCy, SilkKnob::kW, SilkKnob::kH }, s));
    redOn.setBounds (scaled ({ 219.0f, 150.0f, 62.0f, 21.0f }, s));
    blueOn.setBounds (scaled ({ 719.0f, 150.0f, 62.0f, 21.0f }, s));
    power.setBounds (scaled ({ 795.0f, 48.0f, 150.0f, 34.0f }, s));
    themes.setBounds (scaled ({ 810.0f, 92.0f, kDotsW, 20.0f }, s));
    inviteBar.setBounds (scaled ({ 330.0f, 14.0f, 340.0f, 22.0f }, s));
    siteLink.setBounds (scaled ({ 30.0f, 604.0f, 104.0f, 20.0f }, s));
    if (invitePanel != nullptr)
        invitePanel->setBounds (getLocalBounds());
    renderBackground();
}

void SilkEditor::renderBackground()
{
    if (getWidth() <= 0 || getHeight() <= 0 || theme == nullptr)
        return;
    const auto& t = *theme;
    const float scale = getWidth() / kRefW;
    float ds = 1.0f;
    if (auto* d = Desktop::getInstance().getDisplays().getPrimaryDisplay())
        ds = (float) d->scale;
    if (! std::isfinite (ds)) ds = 1.0f;
    const float pix = scale * jlimit (2.0f, 4.0f, ds);
    background = Image (Image::ARGB, jlimit (1, 8192, roundToInt (kRefW * pix)), jlimit (1, 8192, roundToInt (kRefH * pix)), true,
                        SoftwareImageType());
    Graphics g (background);
    g.addTransform (AffineTransform::scale (pix));
    const auto all = Rectangle<float> (0, 0, kRefW, kRefH);

    g.setColour (t.light ? c (0xffd9d6cf) : t.panelB.darker (0.5f));
    g.fillRect (all);
    Path panel;
    panel.addRoundedRectangle (all, 18.0f);
    g.saveState();
    g.reduceClipRegion (panel);
    g.setGradientFill (ColourGradient (t.panelA, 0, 0, t.panelB, kRefW * 0.4f, kRefH, false));
    g.fillRect (all);
    if (! t.highlight.isTransparent())
    {
        g.setGradientFill (ColourGradient (t.highlight.withAlpha (t.light ? 0.9f : 0.75f), 180, 0, t.highlight.withAlpha (0.0f), 760, 260, true));
        g.fillRect (all);
    }
    if (t.tinted)
    {
        g.setGradientFill (ColourGradient (c (0x12ff2846), 250, 350, c (0x00ff2846), 700, 350, true));
        g.fillRect (all);
        g.setGradientFill (ColourGradient (c (0x152878ff), 750, 350, c (0x002878ff), 1200, 350, true));
        g.fillRect (all);
    }
    g.restoreState();
    g.setColour (t.border);
    g.drawRoundedRectangle (all.reduced (0.6f), 17.5f, 0.6f);

    // the logo: "referenc" + a glowing "e"
    {
        GlyphArrangement ga;
        ga.addLineOfText (fonts->thin (46.0f, 0.326f), "reference", 55.0f, 85.0f);
        Path ink, accent;
        for (int i = 0; i < ga.getNumGlyphs(); ++i)
            ga.getGlyph (i).createPath (i == ga.getNumGlyphs() - 1 ? accent : ink);
        g.setColour (t.text);
        g.fillPath (ink);
        if (! t.light)
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (t.logoE.withAlpha (0.08f));
                g.strokePath (accent, PathStrokeType (2.2f * (float) i));
            }
        g.setColour (t.logoE);
        g.fillPath (accent);
    }
    spacedText (g, fonts->light (11.0f, 0.73f), "SILK  " + kDot + "  RED / BLUE", { 58, 102, 400, 15 }, Justification::centredLeft, t.sub);

    g.setColour (t.text.withAlpha (0.08f));
    g.fillRect (Rectangle<float> (500.0f, 180.0f, 0.6f, 310.0f));
    spacedText (g, fonts->light (8.0f, 0.56f), "TRANSFORMER SILK  " + kDot + "  OVERSAMPLED  " + kDot + "  LEVEL MATCHED  " + kDot + "  LIGHT CPU",
                { 0, 600, kRefW, 14 }, Justification::centred, t.faint.withMultipliedAlpha (0.75f));
}

void SilkEditor::paint (Graphics& g)
{
    if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat());
    else
        g.fillAll (Colours::black);
}

void SilkEditor::timerCallback()
{
    if (proc.getTheme() != themeIndex) // a session or preset was loaded with another look
        applyTheme (proc.getTheme());
    if (++inviteTicks >= 100) // every 5 s: the trial may end while the window is open
    {
        inviteTicks = 0;
        if (invitePanel == nullptr)
            refreshInvite();
    }
    const bool on = power.getToggleState();
    redKnob.setActive (on && redOn.getToggleState());
    blueKnob.setActive (on && blueOn.getToggleState());
}
