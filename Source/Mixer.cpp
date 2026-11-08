/*
    Mix vrstiev (API v3) - hlasitost, panorama, solo, hlavna hlasitost a meraky stop.
    Pozri ui/API.md.
*/

#include "MainComponent.h"

static constexpr double minDb = -60.0, maxDb = 6.0;   // pod -60 dB = ticho

static float clampDb (double db)
{
    return db <= minDb ? -100.0f : (float) juce::jlimit (minDb, maxDb, db);
}

//==============================================================================
void MainComponent::detachAllTrackMeters()
{
    // Volat, kym stopy (a ich merace) este existuju
    if (edit != nullptr)
    {
        for (auto* t : te::getAudioTracks (*edit))
        {
            auto it = trackMeters.find (t->itemID.toString());

            if (it != trackMeters.end() && it->second.measurer != nullptr && it->second.client != nullptr)
                it->second.measurer->removeClient (*it->second.client);
        }
    }

    trackMeters.clear();
}

void MainComponent::syncTrackMeters()
{
    if (edit == nullptr)
    {
        trackMeters.clear();
        return;
    }

    std::set<juce::String> existing;

    for (auto* t : te::getAudioTracks (*edit))
    {
        auto id = t->itemID.toString();
        existing.insert (id);

        auto* meterPlugin = t->getLevelMeterPlugin();
        auto* measurer = meterPlugin != nullptr ? &meterPlugin->measurer : nullptr;
        auto& tm = trackMeters[id];

        if (tm.measurer != measurer)
        {
            if (tm.measurer != nullptr && tm.client != nullptr)
                tm.measurer->removeClient (*tm.client);

            tm.measurer = measurer;
            tm.client = std::make_unique<te::LevelMeasurer::Client>();

            if (measurer != nullptr)
                measurer->addClient (*tm.client);
        }

        if (tm.client != nullptr && tm.measurer != nullptr)
        {
            auto l = tm.client->getAndClearAudioLevel (0).dB;
            auto r = tm.client->getAndClearAudioLevel (1).dB;
            auto db = juce::jmax (l, r);
            tm.levelDb = db > tm.levelDb ? db : juce::jmax (db, tm.levelDb - 1.5f);   // pomaly dobeh
        }
    }

    // zmazane stopy: ich merac zanikol spolu so stopou, klienta len zahodime
    for (auto it = trackMeters.begin(); it != trackMeters.end();)
        it = existing.count (it->first) == 0 ? trackMeters.erase (it) : std::next (it);
}

//==============================================================================
void MainComponent::setTrackVolumeDb (const juce::String& trackId, double db)
{
    if (auto* t = findTrackById (trackId))
        if (auto* vol = t->getVolumePlugin())
        {
            vol->setVolumeDb (clampDb (db));
            requestSave();
        }
}

void MainComponent::setTrackPan (const juce::String& trackId, double pan)
{
    if (auto* t = findTrackById (trackId))
        if (auto* vol = t->getVolumePlugin())
        {
            vol->setPan ((float) juce::jlimit (-1.0, 1.0, pan));
            requestSave();
        }
}

void MainComponent::setTrackSolo (const juce::String& trackId, bool solo)
{
    if (auto* t = findTrackById (trackId))
    {
        t->setSolo (solo);
        requestSave();
    }
}

void MainComponent::setMasterVolumeDb (double db)
{
    if (edit == nullptr)
        return;

    if (auto vol = edit->getMasterVolumePlugin())
    {
        vol->setVolumeDb (clampDb (db));
        requestSave();
    }
}

//==============================================================================
juce::var MainComponent::buildMixerState()
{
    auto obj = std::make_unique<juce::DynamicObject>();
    juce::Array<juce::var> levels;
    float masterLevel = -100.0f;

    if (edit != nullptr)
    {
        if (auto vol = edit->getMasterVolumePlugin())
            obj->setProperty ("masterDb", juce::roundToInt (vol->getVolumeDb() * 10.0f) / 10.0);

        for (auto* t : te::getAudioTracks (*edit))
        {
            auto id = t->itemID.toString();
            auto it = trackMeters.find (id);
            auto db = it != trackMeters.end() ? it->second.levelDb : -100.0f;

            auto lv = std::make_unique<juce::DynamicObject>();
            lv->setProperty ("id", id);
            lv->setProperty ("levelDb", juce::roundToInt (db * 10.0f) / 10.0);
            levels.add (lv.release());

            masterLevel = juce::jmax (masterLevel, db);
        }
    }

    obj->setProperty ("masterLevelDb", juce::roundToInt (masterLevel * 10.0f) / 10.0);
    // API v4: skutocny vystup zvukovej karty, zvlast L a R (VU metre)
    obj->setProperty ("masterLevelL", juce::roundToInt (outLevelL * 10.0f) / 10.0);
    obj->setProperty ("masterLevelR", juce::roundToInt (outLevelR * 10.0f) / 10.0);
    obj->setProperty ("trackLevels", levels);
    return juce::var (obj.release());
}
