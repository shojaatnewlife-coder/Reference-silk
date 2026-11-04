// reference · Silk — the window: two glowing knobs (RED / BLUE), their ON pills, POWER and six themes.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "../Invite/InvitePanel.h"

/** One look: panel, type and the two knob colours. */
struct SilkTheme
{
    const char* name;
    juce::Colour panelA, panelB, highlight; // panel gradient top-left -> bottom-right, plus a soft wash
    juce::Colour border, text, dim, faint, sub, tick, pointer;
    juce::Colour logoE, powerDot;
    juce::Colour red, redBall[3], blue, blueBall[3];
    bool light;   // light panel (Ivory): darker glow, no additive halos
    bool tinted;  // the original black: a red wash behind RED, a blue wash behind BLUE

    static const SilkTheme& get (int index);
};

/** The embedded Inter typefaces, shared by every open window. */
struct SilkFonts
{
    SilkFonts();
    juce::Typeface::Ptr thinFace, lightFace;
    juce::Font thin (float height, float tracking = 0.0f) const;
    juce::Font light (float height, float tracking = 0.0f) const;
};

/** A small rounded pill with a dot: the ON switch above each knob, and POWER. */
class PillSwitch : public juce::ToggleButton
{
public:
    PillSwitch (juce::String label, bool big);
    void setTheme (const SilkTheme* t, juce::Colour accentColour) { theme = t; accent = accentColour; repaint(); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::SharedResourcePointer<SilkFonts> fonts;
    juce::String label;
    bool big;
    const SilkTheme* theme = nullptr;
    juce::Colour accent;
};

/** The knob, with its name, frequency line, description and value. Drag up / down, double-click resets. */
class SilkKnob : public juce::Slider
{
public:
    static constexpr float kW = 350.0f, kH = 360.0f, kCy = 113.0f; // reference units; kCy = dial centre

    SilkKnob (bool isRed);
    void setTheme (const SilkTheme* t) { theme = t; repaint(); }
    void setActive (bool a);
    void paint (juce::Graphics&) override;

private:
    juce::SharedResourcePointer<SilkFonts> fonts;
    bool isRed, active = true;
    const SilkTheme* theme = nullptr;
};

/** Six small swatches under POWER: click one to change the look. */
class ThemeDots : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (int)> onPick;
    void setSelected (int i) { selected = i; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

private:
    int indexAt (juce::Point<float>) const;
    int selected = 0, hover = -1;
};

class SilkEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr float kRefW = 1000.0f, kRefH = 640.0f;

    explicit SilkEditor (SilkProcessor&);
    ~SilkEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void applyTheme (int index);

private:
    void timerCallback() override;
    void renderBackground();
    void refreshInvite();
    void showInvitePanel (bool show);

    SilkProcessor& proc;
    juce::SharedResourcePointer<SilkFonts> fonts;
    const SilkTheme* theme = nullptr;
    int themeIndex = -1;

    SilkKnob redKnob { true }, blueKnob { false };
    PillSwitch redOn { "ON", false }, blueOn { "ON", false }, power { "POWER", true };
    ThemeDots themes;
    juce::TooltipWindow tooltips { this, 600 };

    refinv::InviteBar inviteBar;
    refinv::LineButton siteLink { REF_FA ("Reference  \u2197"), juce::Colour (0x8cffffff) };
    std::unique_ptr<refinv::InvitePanel> invitePanel;
    int inviteTicks = 0;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    SA redA, blueA;
    BA redOnA, blueOnA, powerA;

    juce::Image background;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SilkEditor)
};
