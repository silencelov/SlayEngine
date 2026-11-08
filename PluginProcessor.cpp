#include <JuceHeader.h>

class SlayEngineAudioProcessor  : public juce::AudioProcessor
{
public:
    SlayEngineAudioProcessor()
         : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
           apvts (*this, nullptr, "Parameters", createParameterLayout()) {}

    ~SlayEngineAudioProcessor() override {}

    void prepareToPlay (double sampleRate, int samplesPerBlock) override {
        sr = sampleRate;
        p1 = 0.0f; p2 = 0.0f; env = 0.0f;
        lpStateL = 0.0f; lpStateR = 0.0f;
    }

    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        juce::ScopedNoDenormals noDenormals;
        buffer.clear();

        for (const auto metadata : midiMessages) {
            auto msg = metadata.getMessage();
            if (msg.isNoteOn())  { active = true; noteHz = (float) juce::MidiMessage::getMidiNoteInHertz (msg.getNoteNumber()); env = 1.0f; }
            else if (msg.isNoteOff()) { active = false; }
        }

        if (!active && env <= 0.001f) return;

        float warp   = apvts.getRawParameterValue ("WARP")->load();
        float bite   = apvts.getRawParameterValue ("BITE")->load();
        float cutoff = apvts.getRawParameterValue ("CUTOFF")->load();
        float rel    = apvts.getRawParameterValue ("REL")->load();

        float relCoeff = std::exp (-1.0f / (rel * (float) sr));
        float inc1 = noteHz / (float) sr;
        float inc2 = (noteHz * 1.006f) / (float) sr;

        float rc = 1.0f / (2.0f * juce::MathConstants<float>::pi * cutoff);
        float alpha = (1.0f / (float) sr) / (rc + (1.0f / (float) sr));

        auto* outL = buffer.getWritePointer (0);
        auto* outR = buffer.getWritePointer (1);

        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            float modP1 = std::pow (p1, 1.0f + warp * 2.0f);
            float modP2 = std::pow (p2, 1.0f + warp * 2.0f);
            
            float w1 = std::sin (modP1 * juce::MathConstants<float>::twoPi);
            float w2 = (modP2 < 0.5f ? 1.0f : -1.0f);

            float sigL = (w1 * 0.6f + w2 * 0.4f);
            float sigR = (w1 * 0.4f + w2 * 0.6f);

            p1 += inc1; if (p1 >= 1.0f) p1 -= 1.0f;
            p2 += inc2; if (p2 >= 1.0f) p2 -= 1.0f;

            if (!active) env *= relCoeff;

            sigL *= env;
            sigR *= env;

            sigL = std::tanh (sigL * bite + 0.1f * sigL * sigL);
            sigR = std::tanh (sigR * bite + 0.1f * sigR * sigR);

            lpStateL += alpha * (sigL - lpStateL);
            lpStateR += alpha * (sigR - lpStateR);

            outL[i] += lpStateL * 0.4f;
            outR[i] += lpStateR * 0.4f;
        }
    }

    juce::AudioProcessorEditor* createEditor() override {
        return new juce::GenericAudioProcessorEditor (*this);
    }

    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SlayEngine"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override {
        auto state = apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
    void setStateInformation (const void* data, int sizeInBytes) override {
        std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
        if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
        }

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        params.push_back (std::make_unique<juce::AudioParameterFloat> ("WARP", "Wave Warp", 0.0f, 1.0f, 0.5f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> ("BITE", "Bite Drive", 1.0f, 10.0f, 3.5f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> ("CUTOFF", "Toxic Cutoff", 200.0f, 16000.0f, 4500.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> ("REL", "Release", 0.05f, 3.0f, 0.35f));
        return { params.begin(), params.end() };
    }

    double sr = 44100.0;
    float p1 = 0.0f, p2 = 0.0f;
    bool active = false;
    float noteHz = 440.0f;
    float env = 0.0f;
    float lpStateL = 0.0f, lpStateR = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlayEngineAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SlayEngineAudioProcessor();
}
