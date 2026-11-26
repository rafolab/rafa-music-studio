#pragma once

#include "Texts.h"

//==============================================================================
/** Premostenie prveho opakovania slucky ("plynule prve opakovanie").

    Problem: po nahrati prechodu slucky Tracktion vlozi novy klip az ~0,1 s po pretoceni
    (koniec slucky existuje az po LOOP OUT + oneskorenie vstupu). Zaciatok prveho opakovania
    by bol ticho.

    Riesenie: kratko pred koncom prechodu sa zaciatok slucky (uz davno nahraty na disku)
    nacita do pamate. Tento plugin sedi na zaciatku retazca nahravanej stopy - teda pred jej
    hlasitostou, PAN, MUTE/SOLO a VOLUME - a pri pretoceni zahra zaciatok z pamate presne
    podla casu prehravania. Akonahle sa vo vstupe pluginu objavi skutocny klip (rovnaky zvuk),
    premostenie okamzite skonci - nic sa nezdvoji.

    Do suborov sa nic nezapisuje: premostenie ide len do reproduktorov. Na zvukovom vlakne sa
    nic nealokuje ani necita z disku. Plugin sa po skonceni zo stopy odstrani a do projektu
    sa neuklada. */
class LoopBridgePlugin : public te::Plugin
{
public:
    LoopBridgePlugin (te::PluginCreationInfo info) : te::Plugin (info) {}
    ~LoopBridgePlugin() override { notifyListenersOfDeletion(); }

    static const char* getPluginName()  { return "RAFA Loop Bridge"; }
    static inline const char* xmlTypeName = "rafaLoopBridge";
    static juce::ValueTree create()     { return te::createValueTree (te::IDs::PLUGIN, te::IDs::type, xmlTypeName); }

    juce::String getName() const override                   { return getPluginName(); }
    juce::String getPluginType() override                   { return xmlTypeName; }
    juce::String getShortName (int) override                { return "Bridge"; }
    juce::String getSelectableDescription() override        { return getPluginName(); }
    bool canBeDisabled() override                           { return false; }
    bool producesAudioWhenNoAudioInput() override           { return true; }   // stopa este nema klip
    // rovnaky pocet kanalov ako nahravka (mono/stereo), aby cesta cez hlasitost a PAN bola totozna s klipom
    int getNumOutputChannelsGivenInputs (int n) override    { return juce::jmax (n, outChannels); }
    void setChannels (int c)                                { outChannels = juce::jlimit (1, 2, c); }
    BusLayout getBusses() const override                    { return BusLayout::singlePassThrough(); }
    bool needsConstantBufferSize() override                 { return false; }
    void initialise (const te::PluginInitialisationInfo&) override {}
    void deinitialise() override {}

    void applyToBuffer (const te::PluginRenderContext&) override;

    //==============================================================================
    /** Pripravi (na hlavnom vlakne) zaciatok slucky v pamati a zapne premostenie.
        audio: kanaly zaciatku slucky (od LOOP IN), sampleRate jeho frekvencia. */
    void arm (juce::AudioBuffer<float>&& audio, double sampleRate, te::TimePosition loopStart);
    void disarm()                       { armed.store (false); waitForAudioThread(); }

    enum class Result { idle, waiting, playing, handedOver, timedOut };
    Result getResult() const            { return (Result) result.load(); }
    int getBridgedSamples() const       { return bridgedSamples.load(); }

private:
    // data vlastni hlavne vlakno, kym armed == false; zvukove vlakno ich len cita, kym armed == true
    juce::AudioBuffer<float> data;
    double dataRate = 0.0;
    int outChannels = 1;
    double startSec = 0.0;
    std::atomic<bool> armed { false };
    std::atomic<int> inCallback { 0 };   // zvukove vlakno prave cita data
    void waitForAudioThread();
    std::atomic<int> result { (int) Result::idle };
    std::atomic<int> bridgedSamples { 0 };
};

//==============================================================================
/** Cita zaciatok WAV suboru, ktory sa este zapisuje (hlavicka este nema konecnu dlzku).
    Vrati aj BWAV TimeReference (cas zaciatku nahravky v Edit vzorkach) - Tracktion ho
    zapisuje hned pri zaciatku nahravania. */
struct GrowingWavReader
{
    static bool read (const juce::File& f, juce::int64 startFrame, int numFrames,
                      juce::AudioBuffer<float>& out, double& sampleRate, juce::int64& timeReference,
                      juce::String& error);
};
