// Development tool: drives the real processor + editor offline, checks the state round trip and renders
// the window in every theme. Not part of the plugin.
#include "../Source/plugin/PluginEditor.h"

using namespace juce;

static void pump (int ms) { MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static void setParam (SilkProcessor& p, const char* id, float plain)
{
    if (auto* prm = p.getState().getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

static void snap (SilkProcessor& p, const File& f, int width)
{
    std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (width, roundToInt (width * SilkEditor::kRefH / SilkEditor::kRefW));
    pump (200);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    f.deleteFile();
    FileOutputStream os (f);
    PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir (argc > 1 ? String (argv[1]) : File::getCurrentWorkingDirectory().getFullPathName());

    SilkProcessor p;
    p.setPlayConfigDetails (2, 2, 48000, 512);
    p.prepareToPlay (48000, 512);
    std::printf ("latency: %d samples\n", p.getLatencySamples());

    // a few blocks of audio
    AudioBuffer<float> buf (2, 512);
    MidiBuffer midi;
    double phase = 0;
    for (int b = 0; b < 200; ++b)
    {
        for (int i = 0; i < 512; ++i, phase += 2 * MathConstants<double>::pi * 110 / 48000)
            buf.setSample (0, i, (float) (0.4 * std::sin (phase))), buf.setSample (1, i, (float) (0.4 * std::sin (phase * 1.01)));
        p.processBlock (buf, midi);
    }
    std::printf ("output sample: %.4f\n", buf.getSample (0, 100));

    setParam (p, ParamID::red, 62);
    setParam (p, ParamID::blue, 38);
    p.setTheme (3);
    MemoryBlock mb;
    p.getStateInformation (mb);
    {
        SilkProcessor p2;
        p2.setStateInformation (mb.getData(), (int) mb.getSize());
        std::printf ("state round trip: theme %d, red %.0f, blue %.0f\n", p2.getTheme(),
                     p2.getState().getRawParameterValue (ParamID::red)->load(), p2.getState().getRawParameterValue (ParamID::blue)->load());
    }

    const char* names[] = { "black", "grey", "walnut", "ivory", "neon", "midnight" };
    for (int t = 0; t < SilkProcessor::kNumThemes; ++t)
    {
        p.setTheme (t);
        snap (p, outDir.getChildFile (String ("silk_") + names[t] + ".png"), 1000);
    }
    p.setTheme (0);
    setParam (p, ParamID::blueOn, 0);
    snap (p, outDir.getChildFile ("silk_blue_off.png"), 1000);
    snap (p, outDir.getChildFile ("silk_small.png"), 600);
    return 0;
}
