/*
    Etapa 2D: historia nahravacich pokusov (UNDO / REDO) a odstranenie stopy.

    Zamerne nepouzivame celkove UNDO Tracktionu (vracalo by aj fadery a tempo).
    Kazdy dokonceny pokus a kazde odstranenie stopy je jeden krok, najviac 20.
    Zvukove subory sa NIKDY nemazu - ostavaju v priecinku skladby, preto REDO
    a vratenie odstranenej stopy funguju vzdy.
*/

#include "MainComponent.h"

static constexpr size_t maxHistory = 20;

void MainComponent::clearHistory()
{
    undoStack.clear();
    redoStack.clear();
}

void MainComponent::pushHistory (HistoryEntry e)
{
    undoStack.push_back (std::move (e));

    if (undoStack.size() > maxHistory)
        undoStack.erase (undoStack.begin());

    redoStack.clear();      // novy krok ukoncuje vetvu REDO
}

juce::var MainComponent::buildHistoryState()
{
    auto h = std::make_unique<juce::DynamicObject>();
    h->setProperty ("canUndo", ! undoStack.empty());
    h->setProperty ("canRedo", ! redoStack.empty());
    h->setProperty ("undoLabel", undoStack.empty() ? juce::String() : undoStack.back().label);
    h->setProperty ("redoLabel", redoStack.empty() ? juce::String() : redoStack.back().label);
    return h.release();
}

//==============================================================================
void MainComponent::removeTrackSafely (te::AudioTrack& t)
{
    // merac stopy odpojime skor, nez stopa zanikne
    auto meterIt = trackMeters.find (t.itemID.toString());

    if (meterIt != trackMeters.end())
    {
        if (meterIt->second.measurer != nullptr && meterIt->second.client != nullptr)
            meterIt->second.measurer->removeClient (*meterIt->second.client);

        trackMeters.erase (meterIt);
    }

    edit->deleteTrack (&t);
}

te::AudioTrack* MainComponent::insertTrackFromState (const juce::ValueTree& state, int index)
{
    auto tracks = te::getAudioTracks (*edit);
    index = juce::jlimit (0, tracks.size(), index);

    auto point = index > 0 ? te::TrackInsertPoint (*tracks[index - 1], false)
               : ! tracks.isEmpty() ? te::TrackInsertPoint (*tracks.getFirst(), true)
                                    : te::TrackInsertPoint::getEndOfTracks (*edit);

    // nove ID: Tracktion si stare objekty s rovnakym ID chvilu drzi v pamati
    auto copy = state.createCopy();
    te::EditItemID::remapIDs (copy, nullptr, *edit);
    edit->insertTrack (point, copy, nullptr);
    return dynamic_cast<te::AudioTrack*> (te::findTrackForState (*edit, copy));
}

//==============================================================================
void MainComponent::deleteTrackWithHistory (te::AudioTrack& t)
{
    if (edit == nullptr || edit->getTransport().isRecording())
        return;

    HistoryEntry e;
    e.type = HistoryEntry::deleteTrack;
    e.label = sk ("odstránenie stopy „") + t.getName() + sk ("“");
    e.trackId = t.itemID.toString();
    e.trackState = t.state.createCopy();        // meno, hlasitost, PAN, M/S, klipy
    e.trackIndex = te::getAudioTracks (*edit).indexOf (&t);

    removeTrackSafely (t);
    pushHistory (std::move (e));
    saveEdit();
    trackList.refreshIfChanged();
    setStatus (sk ("Stopa odstránená. Vrátiť: Ctrl+Z."), RafaColours::play);
}

//==============================================================================
bool MainComponent::undoLast()
{
    if (edit == nullptr || undoStack.empty())
        return false;

    if (edit->getTransport().isRecording())
    {
        setStatus (sk ("Počas nahrávania sa nedá vracať – najprv dokonči pokus."), RafaColours::accent);
        return false;
    }

    auto e = undoStack.back();
    undoStack.pop_back();

    if (e.type == HistoryEntry::take)
    {
        auto* track = findTrackById (e.trackId);

        if (track == nullptr)
            return false;

        auto clipID = te::EditItemID::fromVar (e.clipState.getProperty (te::IDs::id));

        for (auto* c : track->getClips())
            if (c->itemID == clipID)
            {
                c->removeFromParent();
                break;
            }

        // stopa, ktora vznikla az pri tomto pokuse a ostala prazdna, zmizne tiez
        if (e.trackCreatedByTake && track->getClips().isEmpty())
        {
            e.trackState = track->state.createCopy();
            e.trackIndex = te::getAudioTracks (*edit).indexOf (track);
            removeTrackSafely (*track);
        }
        else
        {
            e.trackState = {};
        }

        setStatus (sk ("Pokus vrátený (") + e.label + sk ("). Obnoviť: Ctrl+Y."), RafaColours::play);
    }
    else
    {
        if (auto* t = insertTrackFromState (e.trackState, e.trackIndex))
        {
            e.trackId = t->itemID.toString();      // obnovena stopa ma nove ID
            selectTrack (e.trackId);
        }

        setStatus (sk ("Stopa obnovená."), RafaColours::play);
    }

    redoStack.push_back (std::move (e));
    saveEdit();
    trackList.refreshIfChanged();
    return true;
}

bool MainComponent::redoLast()
{
    if (edit == nullptr || redoStack.empty() || edit->getTransport().isRecording())
        return false;

    auto e = redoStack.back();
    redoStack.pop_back();

    if (e.type == HistoryEntry::take)
    {
        te::AudioTrack* track = findTrackById (e.trackId);

        if (track == nullptr && e.trackState.isValid())
            if ((track = insertTrackFromState (e.trackState, e.trackIndex)) != nullptr)
                e.trackId = track->itemID.toString();

        if (track == nullptr)
        {
            redoStack.push_back (e);
            return false;
        }

        auto clipCopy = e.clipState.createCopy();
        te::EditItemID::remapIDs (clipCopy, nullptr, *edit);
        auto* inserted = te::insertClipWithState (*track, clipCopy);

        if (inserted == nullptr)
        {
            redoStack.push_back (e);
            setStatus (sk ("Pokus sa nepodarilo obnoviť."), RafaColours::accent);
            return false;
        }

        e.clipState = inserted->state.createCopy();   // pre dalsie UNDO (nove ID klipu)
        selectTrack (track->itemID.toString());
        setStatus (sk ("Pokus obnovený."), RafaColours::play);
    }
    else
    {
        if (auto* t = findTrackById (e.trackId))
            removeTrackSafely (*t);

        setStatus (sk ("Stopa znova odstránená."), RafaColours::play);
    }

    undoStack.push_back (std::move (e));
    saveEdit();
    trackList.refreshIfChanged();
    return true;
}
