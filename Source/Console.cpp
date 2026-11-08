/*
    Konzola Model 5 (API v4): VU metre zo skutocneho vystupu, vyber vrstvy,
    nova prazdna vrstva, citlivost vstupu (INPUT), ulozenie skladby.
    Pozri ui/API.md.
*/

#include "MainComponent.h"

//==============================================================================
// VU metre: merame skutocny vystup zvukovej karty (to, co pocuje pouzivatel)
//==============================================================================
void MainComponent::detachOutputMeter()
{
    // zariadenie mohlo zaniknut - odhlasime sa len z existujuceho
    if (meteredOut != nullptr)
    {
        auto& dm = engine.getDeviceManager();

        for (int i = 0; i < dm.getNumWaveOutDevices(); ++i)
            if (dm.getWaveOutDevice (i) == meteredOut)
                meteredOut->levelMeasurer.removeClient (outMeterClient);
    }

    meteredOut = nullptr;
    outMeterClient.reset();
}

void MainComponent::syncOutputMeter()
{
    auto* out = engine.getDeviceManager().getDefaultWaveOutDevice();

    if (out != meteredOut)
    {
        detachOutputMeter();
        meteredOut = out;

        if (meteredOut != nullptr)
            meteredOut->levelMeasurer.addClient (outMeterClient);
    }

    auto decay = [] (float& v, float db) { v = db > v ? db : juce::jmax (db, v - 1.5f); };

    if (meteredOut != nullptr)
    {
        decay (outLevelL, outMeterClient.getAndClearAudioLevel (0).dB);
        decay (outLevelR, outMeterClient.getAndClearAudioLevel (1).dB);
    }
    else
    {
        decay (outLevelL, -100.0f);
        decay (outLevelR, -100.0f);
    }
}

//==============================================================================
// Odlozene ukladanie: pri tahani faderom nepiseme subor 20x za sekundu
//==============================================================================
void MainComponent::requestSave()
{
    saveDueMs = juce::Time::getMillisecondCounter() + 800;
}

//==============================================================================
// INPUT: citlivost vstupu pred nahravanim (-24 az +24 dB), plati pre vsetky vstupy
//==============================================================================
void MainComponent::applyInputGain()
{
    auto& dm = engine.getDeviceManager();

    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
        if (auto* w = dm.getWaveInDevice (i))
            if (std::abs (w->getInputGainDb() - inputGainDb) > 0.01f)
                w->setInputGainDb (inputGainDb);
}

//==============================================================================
// Vyber vrstvy a nova prazdna vrstva
//==============================================================================
juce::String MainComponent::getSelectedTrackId()
{
    if (edit == nullptr)
        return {};

    auto tracks = te::getAudioTracks (*edit);

    for (auto* t : tracks)
        if (t->itemID.toString() == selectedTrackId)
            return selectedTrackId;

    // vybrana vrstva zanikla - vyberieme prvu
    return tracks.isEmpty() ? juce::String() : tracks.getFirst()->itemID.toString();
}

void MainComponent::selectTrack (const juce::String& trackId)
{
    if (findTrackById (trackId) == nullptr)
        return;

    selectedTrackId = trackId;

    if (edit != nullptr)
    {
        edit->state.setProperty ("rafaSelectedTrack", trackId, nullptr);
        requestSave();
    }
}

void MainComponent::addEmptyTrack()
{
    if (edit == nullptr || edit->getTransport().isRecording())
        return;

    auto count = te::getAudioTracks (*edit).size();
    edit->ensureNumberOfAudioTracks (count + 1);

    if (auto* t = te::getAudioTracks (*edit).getLast())
    {
        t->setName (sk ("Stopa ") + juce::String (count + 1));
        selectTrack (t->itemID.toString());
        saveEdit();
        setStatus (sk ("Pridaná prázdna vrstva ") + juce::String (count + 1) + sk (" – stlač REC."), RafaColours::play);
    }
}
