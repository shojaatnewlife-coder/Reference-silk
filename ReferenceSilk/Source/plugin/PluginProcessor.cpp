// reference · Silk — JUCE audio processor
#include "PluginProcessor.h"
#include "PluginEditor.h"

SilkProcessor::SilkProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "REFSILK", createLayout())
{
    pRed = apvts.getRawParameterValue (ParamID::red);
    pBlue = apvts.getRawParameterValue (ParamID::blue);
    pRedOn = apvts.getRawParameterValue (ParamID::redOn);
    pBlueOn = apvts.getRawParameterValue (ParamID::blueOn);
    pPower = apvts.getRawParameterValue (ParamID::power);
    apvts.state.setProperty ("theme", 0, nullptr);
    setLatencySamples (ref::SilkEngine::latency());

    juce::String device;
    JUCE_BEGIN_IGNORE_DEPRECATION_WARNINGS
    device = juce::SystemStats::getUniqueDeviceID();
    JUCE_END_IGNORE_DEPRECATION_WARNINGS
    if (device.isEmpty())
        device = juce::SystemStats::getComputerName() + "|" + juce::SystemStats::getLogonName();
    const juce::ScopedLock sl (inviteLock);
    invite = std::make_unique<refinv::Invite> (device.toStdString(), "reference-silk", "SLK");
    inviteLoad();
    inviteSave();
}

// ------------------------------------------------------------------ invite files
static juce::Array<juce::File> inviteFiles()
{
    using F = juce::File;
   #if JUCE_MAC
    const auto main = F::getSpecialLocation (F::userApplicationDataDirectory).getChildFile ("Application Support/Reference/Silk.ref");
   #else
    const auto main = F::getSpecialLocation (F::userApplicationDataDirectory).getChildFile ("Reference/Silk.ref");
   #endif
    const auto hidden = F::getSpecialLocation (F::userHomeDirectory).getChildFile (".reference/.silk.ref");
    return { main, hidden };
}

void SilkProcessor::inviteLoad()
{
    std::vector<std::string> copies;
    for (auto& f : inviteFiles())
        copies.push_back (f.existsAsFile() ? f.loadFileAsString().toStdString() : std::string());
    invite->load (copies, (int64_t) (juce::Time::currentTimeMillis() / 1000));
    demoMode = invite->demo();
}

void SilkProcessor::inviteSave()
{
    const juce::String text (invite->save());
    for (auto& f : inviteFiles())
    {
        f.getParentDirectory().createDirectory();
        f.replaceWithText (text);
    }
}

SilkProcessor::InviteInfo SilkProcessor::getInviteInfo()
{
    const juce::ScopedLock sl (inviteLock);
    inviteLoad(); // another window or session may have changed it
    InviteInfo i;
    i.unlocked = invite->unlocked();
    i.demo = invite->demo();
    i.daysLeft = invite->daysLeft();
    i.inviteCode = refinv::pretty (invite->inviteCode());
    if (! invite->raw().gaveFor.empty())
    {
        std::string t;
        invite->giveThanks (invite->raw().gaveFor, t);
        i.gaveCode = t;
    }
    return i;
}

refinv::GiveResult SilkProcessor::giveThanks (const juce::String& friendInvite, juce::String& thanksOut)
{
    const juce::ScopedLock sl (inviteLock);
    inviteLoad();
    std::string t;
    const auto r = invite->giveThanks (friendInvite.toStdString(), t);
    thanksOut = t;
    inviteSave();
    return r;
}

refinv::RedeemResult SilkProcessor::redeemThanks (const juce::String& code)
{
    const juce::ScopedLock sl (inviteLock);
    inviteLoad();
    const auto r = invite->redeem (code.toStdString());
    inviteSave();
    demoMode = invite->demo();
    return r;
}

juce::AudioProcessorValueTreeState::ParameterLayout SilkProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v)) + " %"; });
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::red, 1 }, "Red", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::blue, 1 }, "Blue", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, pct));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::redOn, 1 }, "Red On", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::blueOn, 1 }, "Blue On", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::power, 1 }, "Power", true));
    return l;
}

bool SilkProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void SilkProcessor::prepareToPlay (double sampleRate, int)
{
    currentRate = sampleRate > 0 ? sampleRate : 48000.0;
    demoCounter = 0;
    engine.prepare (sampleRate > 0 ? sampleRate : 48000.0);
    ref::SilkEngine::Settings s;
    s.red = pRed->load();
    s.blue = pBlue->load();
    s.redOn = pRedOn->load() > 0.5f;
    s.blueOn = pBlueOn->load() > 0.5f;
    s.power = pPower->load() > 0.5f;
    engine.set (s);
    engine.reset();
    setLatencySamples (ref::SilkEngine::latency());
}

void SilkProcessor::run (juce::AudioBuffer<float>& buffer, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    ref::SilkEngine::Settings s;
    s.red = pRed->load();
    s.blue = pBlue->load();
    s.redOn = pRedOn->load() > 0.5f;
    s.blueOn = pBlueOn->load() > 0.5f;
    s.power = ! bypassed && pPower->load() > 0.5f; // host bypass still keeps the same latency
    if (demoMode.load())
    {
        // demo: the sound passes through clean for 2 s in every 45 s
        const auto period = (int64_t) (45.0 * currentRate);
        if (demoCounter % period < (int64_t) (2.0 * currentRate))
            s.power = false;
        demoCounter = (demoCounter + buffer.getNumSamples()) % period;
    }
    engine.set (s);
    const int nc = juce::jmin (2, buffer.getNumChannels(), getTotalNumInputChannels());
    if (nc > 0)
        engine.process (buffer.getArrayOfWritePointers(), nc, buffer.getNumSamples());
}

void SilkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { run (buffer, false); }
void SilkProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { run (buffer, true); }

int SilkProcessor::getTheme() const
{
    return juce::jlimit (0, kNumThemes - 1, (int) apvts.state.getProperty ("theme", 0));
}

void SilkProcessor::setTheme (int index)
{
    apvts.state.setProperty ("theme", juce::jlimit (0, kNumThemes - 1, index), nullptr);
}

void SilkProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SilkProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.hasType (apvts.state.getType()))
        {
            const int theme = juce::jlimit (0, kNumThemes - 1, (int) state.getProperty ("theme", 0));
            apvts.replaceState (state);
            apvts.state.setProperty ("theme", theme, nullptr);
        }
    }
}

juce::AudioProcessorEditor* SilkProcessor::createEditor() { return new SilkEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SilkProcessor(); }
