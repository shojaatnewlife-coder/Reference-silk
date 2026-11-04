// reference · Silk — JUCE audio processor: RED and BLUE transformer silk, nothing else.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "SilkCore.h"
#include "../Invite/RefInvite.h"

namespace ParamID
{
inline constexpr const char* red = "red";
inline constexpr const char* blue = "blue";
inline constexpr const char* redOn = "redOn";
inline constexpr const char* blueOn = "blueOn";
inline constexpr const char* power = "power";
} // namespace ParamID

class SilkProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kNumThemes = 6;

    SilkProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Reference Silk"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }

    // ---- invite system (free plugin: 14 days full, then invite 1 person) ----
    struct InviteInfo { bool unlocked = false, demo = false; int daysLeft = 0; juce::String inviteCode, gaveCode; };
    InviteInfo getInviteInfo();
    refinv::GiveResult giveThanks (const juce::String& friendInvite, juce::String& thanksOut);
    refinv::RedeemResult redeemThanks (const juce::String& code);
    bool isDemo() const noexcept { return demoMode.load(); }

    /** The look of the window (not automatable, saved with the session). */
    int getTheme() const;
    void setTheme (int index);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void run (juce::AudioBuffer<float>&, bool bypassed);

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* pRed = nullptr;
    std::atomic<float>* pBlue = nullptr;
    std::atomic<float>* pRedOn = nullptr;
    std::atomic<float>* pBlueOn = nullptr;
    std::atomic<float>* pPower = nullptr;

    ref::SilkEngine engine;

    // invite system
    void inviteLoad();   // call with inviteLock held
    void inviteSave();   // call with inviteLock held
    juce::CriticalSection inviteLock;
    std::unique_ptr<refinv::Invite> invite;
    std::atomic<bool> demoMode { false };
    double currentRate = 48000.0;
    int64_t demoCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SilkProcessor)
};
