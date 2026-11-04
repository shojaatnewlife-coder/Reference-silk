// Reference · Invite — panel and status line
#include "InvitePanel.h"

using namespace juce;

namespace refinv
{
static const Colour kInk (0xfff2f2f2), kDim (0x99ffffff), kFaint (0x33ffffff), kGreen (0xffc8ff6a), kPink (0xffff4fa3),
    kRose (0xffff7a9a), kYellow (0xfffff36a);

static Font uiFont (float h) { return Font (FontOptions().withHeight (h)); }

String faNumber (int n)
{
    const String s (n);
    String out;
    for (auto ch : s)
        out += (ch >= '0' && ch <= '9') ? (juce_wchar) (0x06F0 + (ch - '0')) : ch;
    return out;
}

// ------------------------------------------------------------------ LineButton
LineButton::LineButton (const String& label, Colour c) : Button (label), colour (c)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void LineButton::paintButton (Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    if (highlighted || down)
    {
        g.setColour (colour.withAlpha (down ? 0.22f : 0.12f));
        g.fillRoundedRectangle (r, rad);
    }
    g.setColour (colour.withAlpha (isEnabled() ? 0.9f : 0.3f));
    g.drawRoundedRectangle (r, rad, 0.8f);
    g.setFont (uiFont (r.getHeight() * 0.5f));
    g.drawText (getButtonText(), r, Justification::centred, false);
}

// ------------------------------------------------------------------ InviteBar
void InviteBar::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float h = r.getHeight();
    String text;
    Colour col;
    if (info.demo)
    {
        text = "DEMO  " + String::charToString (0x00B7) + "  invite 1 person to unlock";
        col = kPink;
        g.setColour (kPink.withAlpha (0.08f));
        g.fillRoundedRectangle (r.reduced (0.5f), h * 0.5f);
    }
    else
    {
        text = faNumber (info.daysLeft) + REF_FA (" روز مانده  ") + String::charToString (0x00B7) + "  " + info.inviteCode + "  " + String::charToString (0x00B7) + REF_FA ("  دعوت");
        col = kGreen;
    }
    g.setColour (col.withAlpha (0.75f));
    g.drawRoundedRectangle (r.reduced (0.5f), h * 0.5f, 0.6f);
    g.setColour (col);
    g.setFont (uiFont (h * 0.55f));
    g.drawText (text, r, Justification::centred, false);
}

// ------------------------------------------------------------------ InvitePanel
static void styleLabel (Label& l, float, Colour c, Justification j)
{
    l.setColour (Label::textColourId, c);
    l.setJustificationType (j);
    l.setInterceptsMouseClicks (false, false);
}

static void styleField (TextEditor& e)
{
    e.setColour (TextEditor::backgroundColourId, Colour (0xff050505));
    e.setColour (TextEditor::outlineColourId, kFaint);
    e.setColour (TextEditor::focusedOutlineColourId, kPink);
    e.setColour (TextEditor::textColourId, kInk);
    e.setColour (TextEditor::highlightColourId, kPink.withAlpha (0.3f));
    e.setColour (CaretComponent::caretColourId, kPink);
    e.setJustification (Justification::centred);
    e.setSelectAllWhenFocused (true);
    e.setInputRestrictions (24);
}

InvitePanel::InvitePanel (PanelHooks h) : hooks (std::move (h))
{
    for (auto* l : { &title, &days, &myLabel, &myCode, &friendLabel, &thanksOut, &thanksLabel, &status, &helpText })
        addAndMakeVisible (l);
    for (auto* b : std::initializer_list<Button*> { &copyMine, &make, &copyThanks, &activate, &pasteFriend, &pasteThanks, &close })
        addAndMakeVisible (b);
    addAndMakeVisible (friendField);
    addAndMakeVisible (thanksField);
    styleField (friendField);
    styleField (thanksField);
    friendField.setTextToShowWhenEmpty ("SLK-XXXX-XX", kFaint);
    thanksField.setTextToShowWhenEmpty ("XXXXX-XXXXX", kFaint);

    styleLabel (title, 0, kInk, Justification::centredRight);
    styleLabel (days, 0, kGreen, Justification::centredRight);
    styleLabel (helpText, 0, kDim, Justification::topRight);
    styleLabel (myLabel, 0, kDim, Justification::centredRight);
    styleLabel (myCode, 0, kGreen, Justification::centred);
    styleLabel (friendLabel, 0, kDim, Justification::centredRight);
    styleLabel (thanksOut, 0, kPink, Justification::centred);
    styleLabel (thanksLabel, 0, kDim, Justification::centredRight);
    styleLabel (status, 0, kYellow, Justification::centred);

    title.setText (REF_FA ("دعوت دوستان"), dontSendNotification);
    helpText.setText (REF_FA ("این پلاگین رایگان است و ۱۴ روز کامل کار می‌کند. بعد از آن، با دعوت یک نفر برای همیشه کامل می‌ماند.\n"
                              "مراحل:  ۱. کد دعوتت را برای یک دوست بفرست.  ۲. دوستت کد تو را در Silk خودش می‌زند.  ۳. کد تشکری که به تو می‌دهد را پایین بنویس."),
                      dontSendNotification);
    myLabel.setText (REF_FA ("کد دعوت تو"), dontSendNotification);
    friendLabel.setText (REF_FA ("کد دعوت دوستت (اگر کسی تو را دعوت کرده)"), dontSendNotification);
    thanksLabel.setText (REF_FA ("کد تشکر (از دوستی که دعوتش کردی)"), dontSendNotification);

    copyMine.onClick = [this] { SystemClipboard::copyTextToClipboard (info.inviteCode); status.setText (REF_FA ("کد دعوتت کپی شد. برای دوستت بفرست."), dontSendNotification); };
    copyThanks.onClick = [this] { SystemClipboard::copyTextToClipboard (thanksOut.getText()); status.setText (REF_FA ("کد تشکر کپی شد. برای دوستت بفرست."), dontSendNotification); };
    pasteFriend.onClick = [this] { friendField.setText (SystemClipboard::getTextFromClipboard().trim().substring (0, 24)); };
    pasteThanks.onClick = [this] { thanksField.setText (SystemClipboard::getTextFromClipboard().trim().substring (0, 24)); };
    close.onClick = [this] { if (onClose) onClose(); };

    make.onClick = [this] {
        String out;
        const auto r = hooks.give (friendField.getText(), out);
        switch (r)
        {
            case GiveResult::ok:               status.setText (REF_FA ("کد تشکر ساخته شد. آن را برای دوستت بفرست."), dontSendNotification); break;
            case GiveResult::empty:            status.setText (REF_FA ("اول کد دعوت دوستت را بنویس."), dontSendNotification); break;
            case GiveResult::badFormat:        status.setText (REF_FA ("این کد درست نیست. باید شبیه SLK-XXXX-XX باشد."), dontSendNotification); break;
            case GiveResult::ownCode:          status.setText (REF_FA ("این کد دعوت خودت است. کد دوستت را بنویس."), dontSendNotification); break;
            case GiveResult::alreadyGaveOther: status.setText (REF_FA ("هر نصب فقط یک کد تشکر می‌دهد. این همان کد قبلی توست."), dontSendNotification); break;
        }
        refresh();
    };
    activate.onClick = [this] {
        const auto r = hooks.redeem (thanksField.getText());
        switch (r)
        {
            case RedeemResult::ok:          status.setText (REF_FA ("ممنون! نسخهٔ کامل برای همیشه فعال شد."), dontSendNotification); break;
            case RedeemResult::empty:       status.setText (REF_FA ("اول کد تشکر را بنویس."), dontSendNotification); break;
            case RedeemResult::badFormat:   status.setText (REF_FA ("این کد درست نیست. باید شبیه XXXXX-XXXXX باشد."), dontSendNotification); break;
            case RedeemResult::ownCode:     status.setText (REF_FA ("این کد روی همین کامپیوتر ساخته شده. باید از کامپیوتر دوستت باشد."), dontSendNotification); break;
            case RedeemResult::wrongCode:   status.setText (REF_FA ("این کد برای کد دعوت تو ساخته نشده. دوباره چک کن."), dontSendNotification); break;
            case RedeemResult::alreadyUsed: status.setText (REF_FA ("این کد قبلاً استفاده شده."), dontSendNotification); break;
        }
        refresh();
        if (onChanged) onChanged();
    };
    refresh();
}

void InvitePanel::refresh()
{
    info = hooks.info();
    if (info.unlocked)
        days.setText (REF_FA ("نسخهٔ کامل برای همیشه فعال است. ممنون!"), dontSendNotification);
    else if (info.demo)
        days.setText (REF_FA ("۱۴ روز تمام شد. یک نفر را دعوت کن تا برای همیشه کامل بماند."), dontSendNotification);
    else
        days.setText (faNumber (info.daysLeft) + REF_FA (" روز نسخهٔ کامل مانده."), dontSendNotification);
    days.setColour (Label::textColourId, info.demo ? kRose : kGreen);
    myCode.setText (info.inviteCode, dontSendNotification);
    thanksOut.setText (info.gaveCode, dontSendNotification);
    copyThanks.setVisible (info.gaveCode.isNotEmpty());
    resized();
    repaint();
}

void InvitePanel::mouseUp (const MouseEvent& e)
{
    if (! card.contains (e.position) && onClose)
        onClose();
}

void InvitePanel::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.78f));
    const float s = getWidth() / 1000.0f;
    g.setColour (Colour (0xff030303));
    g.fillRoundedRectangle (card, 14.0f * s);
    g.setColour (kPink.withAlpha (0.55f));
    g.drawRoundedRectangle (card, 14.0f * s, 0.8f);
    g.setColour (kFaint);
    for (float y : { 190.0f, 306.0f })
        g.fillRect (card.getX() + 24 * s, card.getY() + y * s, card.getWidth() - 48 * s, 0.6f);
}

void InvitePanel::resized()
{
    const float s = getWidth() / 1000.0f;
    card = Rectangle<float> (620 * s, 448 * s).withCentre (getLocalBounds().toFloat().getCentre());
    auto at = [this, s] (float x, float y, float w, float h) {
        return Rectangle<float> (card.getX() + x * s, card.getY() + y * s, w * s, h * s).toNearestInt();
    };
    auto font = [s] (Label& l, float h) { l.setFont (uiFont (h * s)); };

    close.setBounds (at (16, 16, 28, 28));
    title.setBounds (at (60, 18, 536, 30));          font (title, 20);
    days.setBounds (at (24, 52, 572, 24));           font (days, 15);
    helpText.setBounds (at (24, 80, 572, 52));       font (helpText, 12);

    myLabel.setBounds (at (430, 144, 166, 30));      font (myLabel, 13);
    myCode.setBounds (at (120, 140, 300, 38));       font (myCode, 24);
    copyMine.setBounds (at (24, 146, 84, 26));

    friendLabel.setBounds (at (24, 198, 572, 22));   font (friendLabel, 12);
    friendField.setBounds (at (220, 226, 376, 32));
    friendField.applyFontToAllText (uiFont (17 * s));
    pasteFriend.setBounds (at (124, 229, 86, 26));
    make.setBounds (at (24, 264, 186, 28));
    thanksOut.setBounds (at (220, 262, 280, 32));    font (thanksOut, 20);
    copyThanks.setBounds (at (510, 266, 86, 26));

    thanksLabel.setBounds (at (24, 314, 572, 22));   font (thanksLabel, 12);
    thanksField.setBounds (at (220, 342, 376, 32));
    thanksField.applyFontToAllText (uiFont (17 * s));
    pasteThanks.setBounds (at (124, 345, 86, 26));
    activate.setBounds (at (24, 345, 92, 26));

    status.setBounds (at (24, 396, 572, 36));        font (status, 13);
}
} // namespace refinv
