// Reference · Invite — the small panel (thin neon lines on black, Persian text) and the status line.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "RefInvite.h"

/** UTF-8 text (Persian) for JUCE strings. */
#define REF_FA(text) juce::String (juce::CharPointer_UTF8 (text))

namespace refinv
{
struct PanelInfo
{
    bool unlocked = false, demo = false;
    int daysLeft = 0;
    juce::String inviteCode, gaveCode;
};

/** What the panel needs from the plugin. */
struct PanelHooks
{
    std::function<PanelInfo()> info;
    std::function<GiveResult (const juce::String&, juce::String&)> give;
    std::function<RedeemResult (const juce::String&)> redeem;
};

/** Persian digits for small numbers. */
juce::String faNumber (int n);

/** A thin outlined button with a neon colour. */
class LineButton : public juce::Button
{
public:
    LineButton (const juce::String& label, juce::Colour colour);
    void setColour2 (juce::Colour c) { colour = c; repaint(); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::Colour colour;
};

/** The line at the top of the plugin: days left + code, or DEMO, or nothing when unlocked. Click opens the panel. */
class InviteBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;
    void set (const PanelInfo& i) { info = i; setVisible (! i.unlocked); repaint(); }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    PanelInfo info;
};

class InvitePanel : public juce::Component
{
public:
    explicit InvitePanel (PanelHooks hooks);
    void refresh();
    std::function<void()> onClose;
    std::function<void()> onChanged;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    PanelHooks hooks;
    PanelInfo info;

    juce::Label title, days, myLabel, myCode, friendLabel, thanksOut, thanksLabel, status, helpText;
    juce::TextEditor friendField, thanksField;
    LineButton copyMine { REF_FA ("کپی"), juce::Colour (0xffc8ff6a) }, make { REF_FA ("ساخت کد تشکر"), juce::Colour (0xffff4fa3) },
        copyThanks { REF_FA ("کپی"), juce::Colour (0xffc8ff6a) }, activate { REF_FA ("فعال‌سازی"), juce::Colour (0xfffff36a) },
        pasteFriend { REF_FA ("چسباندن"), juce::Colour (0x99ffffff) }, pasteThanks { REF_FA ("چسباندن"), juce::Colour (0x99ffffff) },
        close { REF_FA ("×"), juce::Colour (0xccffffff) };
    juce::Rectangle<float> card;
};
} // namespace refinv
