/*
    Tempo, metronom a LOOP (API v2) - pozri ui/API.md.

    Slucka (LOOP) funguje ako hardverova loop station:
    - REC pri zapnutej slucke nahra PRESNE JEDEN prechod slucky (od LOOP IN po LOOP OUT)
      do novej vrstvy (stopy) a potom pokracuje v prehravani slucky.
    - Nahravanie zastavi automaticky po pretoceni slucky (stopRecording - prehravanie bezi dalej).
*/

#include "MainComponent.h"

static constexpr double minTempo = 40.0, maxTempo = 240.0;
static constexpr double minLoopSeconds = 2.0;   // Tracktion nahrava v slucke len od 2 s

//==============================================================================
double MainComponent::getSecondsPerBar() const
{
    if (edit == nullptr)
        return 2.0;

    auto& ts = edit->tempoSequence;
    auto bpm = ts.getTempo (0)->getBpm();
    auto* sig = ts.getTimeSig (0);
    auto beatsPerBar = (double) sig->numerator.get() * 4.0 / (double) juce::jmax (1, sig->denominator.get());
    return beatsPerBar * 60.0 / bpm;
}

double MainComponent::snapToNearestBeat (double seconds) const
{
    if (edit == nullptr)
        return seconds;

    auto& ts = edit->tempoSequence;
    auto beats = ts.toBeats (te::TimePosition::fromSeconds (juce::jmax (0.0, seconds))).inBeats();
    return ts.toTime (te::BeatPosition::fromBeats (std::round (beats))).inSeconds();
}

void MainComponent::initTempoDefaults()
{
    if (edit == nullptr)
        return;

    // Nova skladba: 120 BPM, 4/4, metronom vypnuty, odpocitanie 1 takt, slucka 4 takty (vypnuta)
    if (! edit->getTransport().getLoopRange().isEmpty())
        return;   // existujuca skladba ma uz svoje nastavenia

    edit->setCountInMode (te::Edit::CountIn::oneBar);
    edit->clickTrackEmphasiseBars = true;
    edit->clickTrackRecordingOnly = false;
    edit->setClickTrackVolume (0.6f);
    edit->getTransport().looping = false;
    edit->getTransport().setLoopRange ({ te::TimePosition(), te::TimePosition::fromSeconds (4 * getSecondsPerBar()) });
}

//==============================================================================
void MainComponent::setTempoBpm (double bpm)
{
    if (edit == nullptr || edit->getTransport().isRecording())
        return;

    bpm = juce::jlimit (minTempo, maxTempo, std::round (bpm * 10.0) / 10.0);

    auto& ts = edit->tempoSequence;
    auto& tr = edit->getTransport();

    // slucka si zachova svoju dlzku v dobach (taktoch)
    auto loopBeats = ts.toBeats (tr.getLoopRange());
    ts.getTempo (0)->setBpm (bpm);
    tr.setLoopRange (ts.toTime (loopBeats));

    requestSave();
}

void MainComponent::setTimeSignature (int numerator, int denominator)
{
    if (edit == nullptr || edit->getTransport().isRecording())
        return;

    numerator   = juce::jlimit (2, 7, numerator);
    denominator = (denominator == 8) ? 8 : 4;

    edit->tempoSequence.getTimeSig (0)->setStringTimeSig (juce::String (numerator) + "/" + juce::String (denominator));
    requestSave();
}

void MainComponent::setMetronome (bool on)
{
    if (edit == nullptr)
        return;

    edit->clickTrackEnabled = on;
    requestSave();
}

void MainComponent::setMetronomeVolume (double volume)
{
    if (edit == nullptr)
        return;

    // Tracktion pripusta 0.2 - 1.0; hodnoty z UI 0 - 1 premapujeme
    edit->setClickTrackVolume ((float) (0.2 + 0.8 * juce::jlimit (0.0, 1.0, volume)));
    requestSave();
}

void MainComponent::setCountInBars (int bars)
{
    if (edit == nullptr || edit->getTransport().isRecording())
        return;

    edit->setCountInMode (bars >= 2 ? te::Edit::CountIn::twoBar
                        : bars == 1 ? te::Edit::CountIn::oneBar
                                    : te::Edit::CountIn::none);
    requestSave();
}

//==============================================================================
void MainComponent::setLoopOn (bool on)
{
    if (edit == nullptr || edit->getTransport().isRecording())
        return;

    auto& tr = edit->getTransport();
    tr.looping = on;

    if (on)
    {
        auto range = tr.getLoopRange();
        auto pos = tr.getPosition();

        if (pos < range.getStart() || pos >= range.getEnd())
            tr.setPosition (range.getStart());
    }

    requestSave();
}

void MainComponent::setLoopRangeSeconds (double start, double end)
{
    if (edit == nullptr || edit->getTransport().isRecording())
        return;

    start = juce::jmax (0.0, start);

    if (end <= start + 0.1)
    {
        setStatus (sk ("LOOP: koniec slučky musí byť za začiatkom."), RafaColours::accent);
        return;
    }

    edit->getTransport().setLoopRange ({ te::TimePosition::fromSeconds (start), te::TimePosition::fromSeconds (end) });
    requestSave();
}

void MainComponent::setLoopBars (int bars)
{
    if (edit == nullptr)
        return;

    bars = juce::jlimit (1, 32, bars);

    // zaciatok slucky ostava (zarovnany na takt), meni sa dlzka
    auto barLen = getSecondsPerBar();
    auto start = std::round (edit->getTransport().getLoopRange().getStart().inSeconds() / barLen) * barLen;
    setLoopRangeSeconds (start, start + bars * barLen);
}

void MainComponent::setLoopPointAtPlayhead (bool isStart)
{
    if (edit == nullptr || edit->getTransport().isRecording())
        return;

    auto& tr = edit->getTransport();
    auto range = tr.getLoopRange();
    auto pos = snapToNearestBeat (tr.getPosition().inSeconds());

    if (isStart)
    {
        // ak by zaciatok presiel za koniec, slucka si zachova dlzku
        auto end = pos < range.getEnd().inSeconds() - 0.1 ? range.getEnd().inSeconds()
                                                           : pos + juce::jmax (range.getLength().inSeconds(), getSecondsPerBar());
        setLoopRangeSeconds (pos, end);
        setStatus (sk ("LOOP IN nastavený na ") + juce::String (pos, 2) + " s");
    }
    else
    {
        if (pos <= range.getStart().inSeconds() + 0.1)
        {
            setStatus (sk ("LOOP OUT musí byť za LOOP IN – posuň prehrávaciu hlavu ďalej."), RafaColours::accent);
            return;
        }

        setLoopRangeSeconds (range.getStart().inSeconds(), pos);
        setStatus (sk ("LOOP OUT nastavený na ") + juce::String (pos, 2) + " s");
    }
}

//==============================================================================
void MainComponent::finishLoopRecordingPass()
{
    if (edit == nullptr || ! loopRecordPass)
        return;

    loopRecordPass = false;
    edit->getTransport().stopRecording (false);   // prehravanie slucky bezi dalej

    pendingRecordingCheck = true;
    recordingCheckDeadline = juce::Time::getMillisecondCounter() + 4000;
    trackList.setRecordingTrack (nullptr);
    currentRecordingTrack = nullptr;
    setStatus (sk ("Vrstva nahraná – slučka hrá ďalej. REC = ďalšia vrstva, STOP = zastaviť."), RafaColours::play);
    updateTransportButtons();
}

void MainComponent::checkLoopRecordingPass()
{
    if (edit == nullptr || ! loopRecordPass)
        return;

    auto& tr = edit->getTransport();

    if (! tr.isRecording())
    {
        loopRecordPass = false;
        return;
    }

    auto pos = tr.getPosition().inSeconds();
    auto loopStart = tr.getLoopRange().getStart().inSeconds();

    // slucka sa pretocila (LOOP OUT -> LOOP IN) = jeden prechod je nahraty.
    // Zastavime az ~60 ms po pretoceni, aby koniec vrstvy nechybal (kompenzacia latencie);
    // Tracktion klip oreze presne na LOOP OUT a zvysok kratsi ako 0,1 s zahodi.
    if (pos < lastLoopPosition - 0.25)
        loopWrapped = true;

    if (loopWrapped && pos >= loopStart + 0.06)
    {
        loopWrapped = false;
        finishLoopRecordingPass();
    }

    lastLoopPosition = pos;
}

//==============================================================================
juce::var MainComponent::buildTempoLoopState()
{
    auto obj = std::make_unique<juce::DynamicObject>();

    if (edit == nullptr)
        return juce::var (obj.release());

    auto& ts = edit->tempoSequence;
    auto& tr = edit->getTransport();
    auto* sig = ts.getTimeSig (0);
    auto numerator = sig->numerator.get();

    auto tempo = std::make_unique<juce::DynamicObject>();
    tempo->setProperty ("bpm", ts.getTempo (0)->getBpm());
    tempo->setProperty ("numerator", numerator);
    tempo->setProperty ("denominator", sig->denominator.get());
    obj->setProperty ("tempo", tempo.release());

    auto countIn = edit->getCountInMode();
    auto metro = std::make_unique<juce::DynamicObject>();
    metro->setProperty ("on", (bool) edit->clickTrackEnabled.get());
    metro->setProperty ("volume", juce::jlimit (0.0, 1.0, ((double) edit->getClickTrackVolume() - 0.2) / 0.8));
    metro->setProperty ("countInBars", countIn == te::Edit::CountIn::twoBar ? 2
                                     : countIn == te::Edit::CountIn::oneBar ? 1 : 0);
    obj->setProperty ("metronome", metro.release());

    auto range = tr.getLoopRange();
    auto barLen = getSecondsPerBar();
    auto loop = std::make_unique<juce::DynamicObject>();
    loop->setProperty ("on", (bool) tr.looping.get());
    loop->setProperty ("start", range.getStart().inSeconds());
    loop->setProperty ("end", range.getEnd().inSeconds());
    loop->setProperty ("lengthBars", barLen > 0 ? std::round (range.getLength().inSeconds() / barLen * 100.0) / 100.0 : 0.0);
    loop->setProperty ("canRecord", range.getLength().inSeconds() >= minLoopSeconds);
    loop->setProperty ("recordingPass", loopRecordPass && tr.isRecording());
    obj->setProperty ("loop", loop.release());

    // takt a doba prehravacej hlavy (1 = prva), napr. pre blikanie LED metronomu
    auto beats = ts.toBeats (tr.getPosition()).inBeats();
    auto beatsPerBar = (double) numerator;
    auto position = std::make_unique<juce::DynamicObject>();
    position->setProperty ("bar", beats < 0 ? 0 : (int) std::floor (beats / beatsPerBar) + 1);
    position->setProperty ("beat", beats < 0 ? 0 : (int) std::floor (std::fmod (beats, beatsPerBar)) + 1);
    obj->setProperty ("musicalPosition", position.release());

    return juce::var (obj.release());
}
