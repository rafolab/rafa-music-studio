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
    RAFA_T ("finishPass pos=" + juce::String (edit->getTransport().getPosition().inSeconds(), 3));
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
    // Zastavime ~50 ms po pretoceni (motor sam dopise aj vzorky kompenzacie latencie), aby koniec vrstvy nechybal (kompenzacia latencie);
    // Tracktion klip oreze presne na LOOP OUT a zvysok kratsi ako 0,1 s zahodi.
    if (pos < lastLoopPosition - 0.25)
    {
        loopWrapped = true;
        RAFA_T ("wrap pos=" + juce::String (pos, 3));
    }

    if (loopWrapped && pos >= loopStart + 0.05)
    {
        loopWrapped = false;
        finishLoopRecordingPass();
    }

    lastLoopPosition = pos;
}

//==============================================================================
// Tracktion po nahravani v slucke rozdeli zaznam na "takes" po dlzke slucky, ale pocita ich od
// ZACIATKU SUBORU. S odpocitanim (count-in) subor zacina pred LOOP IN, takze take 1 obsahuje
// odpocitanie a klip (offset za koncom take 1) hra ticho alebo bez casti slucky (chybajuca doba).
// Oprava: takes spojime (v poradi 1, 2, 3 …) spat do povodneho zaznamu a vyrezeme z neho presne
// jeden prechod slucky do noveho suboru …_slucka.wav. Klip sa prepne na novy subor AZ PO overeni,
// ze subor je zapisany, citatelny a ma presnu dlzku. Povodne subory sa NEMAZU (zostavaju v
// priecinku skladby pre pripad obnovy). Pri akejkolvek chybe klip ostava nezmeneny.
MainComponent::LoopFlattenResult MainComponent::flattenLoopTakes (te::WaveAudioClip& clip)
{
    LoopFlattenResult res;

    if (edit == nullptr || ! clip.hasAnyTakes())
        return res;   // notNeeded

    res.status = LoopFlattenResult::failed;
    auto fail = [&res] (const juce::String& why) { res.error = why; juce::Logger::writeToLog ("LOOP oprava zlyhala: " + why); return res; };

    // 1) zoznam suborov takes bez duplicit, zoradeny podla cisla (zakladny subor = 1, "_take_N" = N)
    auto takesTree = clip.state.getChildWithName (te::IDs::TAKES);
    juce::Array<juce::File> files;

    for (auto t : takesTree)
    {
        auto f = te::SourceFileReference::findFileFromString (*edit, t[te::IDs::source].toString());

        if (! f.existsAsFile())
            return fail ("chyba subor take: " + t[te::IDs::source].toString());

        if (! files.contains (f))
            files.add (f);
    }

    auto takeNumber = [] (const juce::File& f)
    {
        auto n = f.getFileNameWithoutExtension();
        auto i = n.lastIndexOfIgnoreCase ("_take_");
        return i < 0 ? 1 : n.substring (i + 6).getIntValue();
    };

    std::stable_sort (files.begin(), files.end(), [&] (const juce::File& x, const juce::File& y) { return takeNumber (x) < takeNumber (y); });

    for (int i = 0; i < files.size(); ++i)
        if (takeNumber (files[i]) != i + 1)
            return fail ("neuplna postupnost takes (" + files[i].getFileName() + ")");

    // 2) nacitanie a spojenie - vsetky takes musia mat rovnaky format, vsetky okrem posledneho dlzku slucky
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    const auto loopRange = edit->getTransport().getLoopRange();
    double sampleRate = 0.0;
    int channels = 0, bits = 24;
    juce::int64 loopSamples = 0;
    juce::AudioBuffer<float> all;

    for (int i = 0; i < files.size(); ++i)
    {
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (files[i]));

        if (r == nullptr)
            return fail ("subor sa neda citat: " + files[i].getFileName());

        if (i == 0)
        {
            sampleRate = r->sampleRate;
            channels = (int) r->numChannels;
            bits = (int) r->bitsPerSample;
            loopSamples = (juce::int64) std::llround (loopRange.getLength().inSeconds() * sampleRate);
            all.setSize (channels, 0);
        }
        else if (! juce::approximatelyEqual (r->sampleRate, sampleRate) || (int) r->numChannels != channels)
            return fail ("rozny format takes");

        if (sampleRate <= 0.0 || channels <= 0 || loopSamples <= 0)
            return fail ("neplatny format");

        if (i < files.size() - 1 && std::abs (r->lengthInSamples - loopSamples) > 2)
            return fail ("take " + juce::String (i + 1) + " ma dlzku " + juce::String (r->lengthInSamples)
                           + " namiesto " + juce::String (loopSamples));

        const int n = (int) r->lengthInSamples, old = all.getNumSamples();
        all.setSize (channels, old + n, true, true);

        if (! r->read (&all, old, n, 0, true, true))
            return fail ("chyba pri citani " + files[i].getFileName());
    }

    // 3) vyrez presne jedneho prechodu slucky; chybat moze najviac 5 ms (zaokruhlenie), inak je to chyba
    const int start = (int) std::llround (clip.getPosition().getOffset().inSeconds() * sampleRate);
    const int length = (int) loopSamples;
    const int available = juce::jlimit (0, length, all.getNumSamples() - start);
    const int tolerance = (int) (0.005 * sampleRate);

    if (start < 0 || available < length - tolerance)
        return fail ("v nahravke chyba " + juce::String ((length - available) * 1000.0 / sampleRate, 0)
                       + " ms konca slucky (nahrane " + juce::String (all.getNumSamples()) + " vzoriek, zaciatok " + juce::String (start) + ")");

    juce::AudioBuffer<float> pass (channels, length);
    pass.clear();

    for (int c = 0; c < channels; ++c)
        pass.copyFrom (c, 0, all, c, start, available);

    // 4) zapis noveho suboru a jeho overenie (zapis, dlzka, citatelnost)
    auto outFile = files[0].getSiblingFile (files[0].getFileNameWithoutExtension() + "_slucka")
                           .withFileExtension ("wav").getNonexistentSibling (false);
    {
        juce::WavAudioFormat wav;
        auto stream = std::make_unique<juce::FileOutputStream> (outFile);

        if (! stream->openedOk())
            return fail ("neda sa vytvorit " + outFile.getFileName());

        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), sampleRate, (unsigned int) channels,
                                                                              juce::jlimit (16, 24, bits), {}, 0));
        if (writer == nullptr)
        {
            stream.reset();
            outFile.deleteFile();
            return fail ("neda sa zapisat WAV");
        }

        stream.release();   // vlastni ho writer

        if (! writer->writeFromAudioSampleBuffer (pass, 0, length))
        {
            writer.reset();
            outFile.deleteFile();
            return fail ("zapis WAV zlyhal");
        }
    }   // writer sa zatvori a dopise hlavicku

    {
        std::unique_ptr<juce::AudioFormatReader> check (fm.createReaderFor (outFile));

        if (check == nullptr || check->lengthInSamples != length || (int) check->numChannels != channels
             || ! juce::approximatelyEqual (check->sampleRate, sampleRate))
        {
            check.reset();
            outFile.deleteFile();
            return fail ("kontrola noveho WAV zlyhala");
        }
    }

    // 5) az teraz prepnut klip na novy subor (povodne takes zostavaju na disku)
    // informacie o novom subore nacitat skor, nez ho klip pouzije (inak motor moze klip chvilu nehrat)
    engine.getAudioFileManager().forceFileUpdate (te::AudioFile (engine, outFile));
    clip.clearTakes();
    clip.getSourceFileReference().setToFile (outFile, te::SourceFileReference::PathStyle::alwaysRelative, false);
    clip.setPosition ({ { loopRange.getStart(), loopRange.getEnd() }, te::TimeDuration() });

    res.status = LoopFlattenResult::ok;
    res.file = outFile;
    res.missingSamples = length - available;
    return res;
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
