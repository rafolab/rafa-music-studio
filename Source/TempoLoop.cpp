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
    if (edit == nullptr || recNow())
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
    if (edit == nullptr || recNow())
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
    if (edit == nullptr || recNow())
        return;

    edit->setCountInMode (bars >= 2 ? te::Edit::CountIn::twoBar
                        : bars == 1 ? te::Edit::CountIn::oneBar
                                    : te::Edit::CountIn::none);
    requestSave();
}

//==============================================================================
void MainComponent::setLoopOn (bool on)
{
    if (edit == nullptr || recNow())
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
    if (edit == nullptr || recNow())
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
    if (edit == nullptr || recNow())
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
    // plynule prve opakovanie: kratko pred koncom prechodu pripravit zaciatok slucky v pamati
    {
        const auto loopEndSec = tr.getLoopRange().getEnd().inSeconds();

        if (! loopWrapped && ! bridgeArmed && bridgePlugin != nullptr
             && pos >= loopEndSec - 0.40 && pos < loopEndSec - 0.04 && pos > loopStart)
            armLoopBridge (loopStart, loopEndSec);
    }

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

    // kontrola (len pri meraniach): sedi premostenie zo suboru s finalnym prechodom slucky?
    if (bridgeDebugCopy.getNumSamples() > 0)
    {
        int bestLag = 0; double best = 1.0e9;
        for (int lag = -4; lag <= 4; ++lag)
        {
            double d = 0.0;
            for (int i = 8; i < juce::jmin (bridgeDebugCopy.getNumSamples(), length) - 8; ++i)
                d += std::abs (bridgeDebugCopy.getSample (0, i) - pass.getSample (0, i + lag));
            if (d < best) { best = d; bestLag = lag; }
        }
        RAFA_T ("bridgeCheck lag=" + juce::String (bestLag) + " diff=" + juce::String (best, 6));
        bridgeDebugCopy.setSize (0, 0);
    }

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
    loop->setProperty ("recordingPass", (loopRecordPass && tr.isRecording()) || fly.active);
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

//==============================================================================
// Plynule prve opakovanie slucky - pozri LoopBridge.h
bool MainComponent::smoothLoopEnabled() const   { return settings->getBoolValue ("smoothFirstRepeat", true); }

void MainComponent::setSmoothLoop (bool on)
{
    settings->setValue ("smoothFirstRepeat", on);
    settings->saveIfNeeded();
    setStatus (on ? sk ("Plynulé prvé opakovanie slučky: ZAP") : sk ("Plynulé prvé opakovanie slučky: VYP"), RafaColours::play);
}

void MainComponent::installLoopBridge (te::AudioTrack& track)
{
    removeLoopBridge();

    if (! smoothLoopEnabled())
        return;

    // pocet kanalov nahravky = pocet kanalov vstupu nahravania (mono gitara / stereo par)
    int channels = 1;

    for (auto* instance : edit->getAllInputDevices())
        if (auto* wi = dynamic_cast<te::WaveInputDevice*> (&instance->getInputDevice()))
            if (instance->getTargets().contains (track.itemID))
                channels = wi->isStereoPair() ? 2 : 1;

    auto plugin = edit->getPluginCache().createNewPlugin (LoopBridgePlugin::create());

    if (auto* bp = dynamic_cast<LoopBridgePlugin*> (plugin.get()))
    {
        bp->setChannels (channels);   // pred vlozenim - graf sa postavi uz so spravnym poctom kanalov
        track.pluginList.insertPlugin (plugin, 0, nullptr);
        bridgePlugin = plugin;
    }

    RAFA_T ("bridge installed " + juce::String (bridgePlugin != nullptr ? "ok" : "FAILED"));
}

void MainComponent::armLoopBridge (double loopStart, double loopEnd, juce::int64 knownStartFrame)
{
    bridgeArmed = true;   // len jeden pokus za prechod
    auto* bp = dynamic_cast<LoopBridgePlugin*> (bridgePlugin.get());

    if (bp == nullptr || edit == nullptr)
        return;

    juce::File file;

    for (auto* instance : edit->getAllInputDevices())
    {
        auto f = instance->getRecordingFile (recordingTrackID);

        if (f != juce::File())
        {
            file = f;
            break;
        }
    }

    // zaciatok slucky (najviac 0,5 s) - nahraty pred viac ako sekundou, je uz na disku
    const double bridgeSec = juce::jmin (0.5, (loopEnd - loopStart) - 0.6);
    juce::AudioBuffer<float> audio;
    double rate = 0.0;
    juce::int64 timeRef = -1;
    juce::String error;

    if (file == juce::File() || bridgeSec < 0.1)
        error = "nahravka nie je dostupna";
    else
    {
        // prva citacia: zistime frekvenciu a TimeReference (zaciatok nahravky v case skladby)
        juce::AudioBuffer<float> probe;

        if (GrowingWavReader::read (file, 0, 1, probe, rate, timeRef, error))
        {
            // Zaciatok suboru = zaciatok nahravania v Tracktione: pri odpocitani je to LOOP IN mínus
            // (pocet dob odpocitania + 0,5 doby), inak LOOP IN (TransportControl::performRecord).
            // Ak subor uz nesie casovu znacku BWAV, pouzije sa ta.
            juce::int64 fileStart = timeRef;
            const bool countInUsed = edit->getNumCountInBeats() > 0;

            if (fileStart < 0)
            {
                auto& ts = edit->tempoSequence;
                const double countIn = edit->getNumCountInBeats();
                auto startTime = te::TimePosition::fromSeconds (loopStart);

                if (countIn > 0)
                    startTime = ts.toTime (ts.toBeats (startTime) - te::BeatDuration::fromBeats (countIn + 0.5));

                fileStart = (juce::int64) std::llround (startTime.inSeconds() * rate);
            }

            // Tracktion zaokruhli posun klipu o 1 vzorku nadol (overene porovnanim s finalnym prechodom)
            const auto startFrame = knownStartFrame >= 0 ? knownStartFrame
                                                         : (juce::int64) std::llround (loopStart * rate) - fileStart - (countInUsed ? 1 : 0);
            GrowingWavReader::read (file, startFrame, (int) std::llround (bridgeSec * rate), audio, rate, timeRef, error);
            timeRef = fileStart;
            bridgeDebugCopy.makeCopyOf (audio);
        }
    }

    if (error.isNotEmpty() || audio.getNumSamples() == 0)
    {
        lastBridgeReport = "premostenie nepripravene: " + error;
        RAFA_T (lastBridgeReport);
        return;   // bez premostenia: spravanie ako doteraz
    }

    bp->arm (std::move (audio), rate, te::TimePosition::fromSeconds (loopStart));
    bridgeArmedMs = juce::Time::getMillisecondCounter();
    RAFA_T ("bridge armed len=" + juce::String (bridgeSec, 3) + " timeRef=" + juce::String (timeRef));
}

void MainComponent::maintainLoopBridge()
{
    if (bridgePlugin == nullptr || edit == nullptr)
        return;

    auto& tr = edit->getTransport();
    auto* bp = dynamic_cast<LoopBridgePlugin*> (bridgePlugin.get());

    if (loopRecordPass || tr.isRecording() || pendingRecordingCheck || fly.active)
        return;   // nahravanie / vkladanie klipu este bezi

    const auto res = bp != nullptr ? bp->getResult() : LoopBridgePlugin::Result::idle;
    const bool finished = res == LoopBridgePlugin::Result::handedOver || res == LoopBridgePlugin::Result::timedOut;
    const bool stale = ! tr.isPlaying() || ! bridgeArmed || res == LoopBridgePlugin::Result::idle
                       || juce::Time::getMillisecondCounter() - bridgeArmedMs > 4000;

    if (finished || stale)
    {
        removeLoopBridge();
    }
}

void MainComponent::removeLoopBridge()
{
    if (bridgePlugin != nullptr)
    {
        if (auto* bp = dynamic_cast<LoopBridgePlugin*> (bridgePlugin.get()))
        {
            if (bridgeArmed)
            {
                static const char* names[] = { "nic", "caka", "hra", "odovzdane klipu", "okno uplynulo" };
                lastBridgeReport = "premostenie: " + juce::String (names[(int) bp->getResult()]) + ", zahrane "
                                   + juce::String (bp->getBridgedSamples()) + " vzoriek";
                RAFA_T (lastBridgeReport);
            }

            bp->disarm();
        }

        bridgePlugin->deleteFromParent();
        bridgePlugin = nullptr;
    }

    bridgeArmed = false;
}

//==============================================================================
// Dalsia vrstva bez prerusenia: slucka hra dalej, nahravanie zacne presne na najblizsom LOOP IN
// a skonci presne po jednom prechode. Pouziva priame API vstupu Tracktionu s necyklickym casom
// (TransportControl::record by pri hrajucej slucke zastavil prehravanie a odpocitaval).
bool MainComponent::startFlyingLayer()
{
    auto& tr = edit->getTransport();
    auto* ctx = tr.getCurrentPlaybackContext();

    if (ctx == nullptr)
        return false;

    auto* track = prepareTrackForRecording();

    if (track == nullptr)
        return true;   // chyba uz bola ohlasena

    te::InputDeviceInstance* inst = nullptr;

    for (auto* i : edit->getAllInputDevices())
        if (i->getInputDevice().getDeviceType() == te::InputDevice::waveDevice
             && i->getTargets().contains (track->itemID) && i->isRecordingEnabled (track->itemID))
            inst = i;

    if (inst == nullptr)
    {
        setStatus (sk ("Nahrávanie sa nespustilo: vstup nie je pripravený."), RafaColours::record);
        return true;
    }

    currentRecordingTrack = track;
    recordingTrackID = track->itemID;
    clipsBeforeRecording = track->getClips().size();
    pendingRecordingCheck = false;
    recordingStartedMs = juce::Time::getMillisecondCounter();

    fly = {};
    fly.active = true;
    fly.trackID = track->itemID;
    fly.instance = inst;
    fly.preparedMs = juce::Time::getMillisecondCounter();

    installLoopBridge (*track);
    trackList.refreshIfChanged();
    trackList.setRecordingTrack (track);
    setStatus (sk ("● Vrstva do stopy „") + track->getName() + sk ("“ sa nahrá od najbližšieho začiatku slučky – hraj ďalej."),
               RafaColours::record);
    updateTransportButtons();
    return true;
}

// Druhy krok (o ~80 ms): Tracktion medzitym asynchronne spracoval zapnutie vstupu na stope
// (InputDeviceInstance::updateRecordingStatus) - keby sme zaznam spustili skor, hned by ho ukoncil.
void MainComponent::beginFlyingRecording()
{
    auto& tr = edit->getTransport();
    auto* ctx = tr.getCurrentPlaybackContext();
    auto* inst = fly.instance;

    if (ctx == nullptr || inst == nullptr || ! tr.isPlaying() || ! tr.looping)
    {
        fly.active = false;
        trackList.setRecordingTrack (nullptr);
        currentRecordingTrack = nullptr;
        removeLoopBridge();
        setStatus (sk ("Vrstva sa nenahrala – slučka už nehrá."), RafaColours::accent);
        updateTransportButtons();
        return;
    }

    const auto loop = tr.getLoopRange();
    const double L = loop.getLength().inSeconds();

    // necyklicky cas najblizsieho LOOP IN v RovNAKOM prepocte, akym Tracktion casuje vstupne bloky
    // (cas zvukoveho prudu -> cas skladby); (U - P) je pocas prechodu konstantne
    const double streamTime = engine.getDeviceManager().getCurrentStreamTime();
    const double P = ctx->globalStreamTimeToEditTime (streamTime).inSeconds();
    const double U = ctx->globalStreamTimeToEditTimeUnlooped (streamTime).inSeconds();
    double toEnd = loop.getEnd().inSeconds() - P;

    if (toEnd < 0.15)   // prilis tesne pred koncom - zacneme az o jeden prechod neskor
        toEnd += L;

    // Merania (spatna slucka vystup -> vstup, bloky 256/512/1024): bez korekcie leti zaznam o presne
    // jeden zvukovy blok skor nez fyzicky spravna poloha a nez prva vrstva. Zaciatok preto posunieme
    // o jeden blok dopredu - vrstvy potom sedia navzajom aj s metronomom.
    double blockSec = 0.0;
    if (auto* dev = engine.getDeviceManager().deviceManager.getCurrentAudioDevice())
        if (dev->getCurrentSampleRate() > 0)
            blockSec = dev->getCurrentBufferSizeSamples() / dev->getCurrentSampleRate();

    const auto punch = te::TimePosition::fromSeconds (U + toEnd - blockSec);

    te::InputDeviceInstance::RecordingParameters rp;
    rp.punchRange = { punch, te::Edit::getMaximumEditEnd() };
    rp.targets = { fly.trackID };

    std::vector<std::unique_ptr<te::InputDeviceInstance::RecordingContext>> contexts;
    juce::String error;

    for (auto& prepared : inst->prepareToRecord (rp))
    {
        if (prepared.has_value())
            contexts.push_back (std::move (*prepared));
        else
            error = prepared.error();
    }

    if (contexts.empty())
    {
        fly.active = false;
        trackList.setRecordingTrack (nullptr);
        currentRecordingTrack = nullptr;
        removeLoopBridge();
        setStatus (sk ("Nahrávanie sa nespustilo: ") + error, RafaColours::record);
        updateTransportButtons();
        return;
    }

    fly.started = true;
    fly.punchStart = punch;
    fly.loopStart = loop.getStart().inSeconds();
    fly.loopEnd = loop.getEnd().inSeconds();

    inst->startRecording (std::move (contexts));

    // koniec presne po jednom prechode (+50 ms; motor dopise aj vzorky kompenzacie latencie)
    te::InputDeviceInstance::StopRecordingParameters sp;
    sp.targetsToStop = { fly.trackID };
    sp.unloopedTimeToEndRecording = punch + te::TimeDuration::fromSeconds (L + 0.05);
    sp.isLooping = false;
    sp.markedRange = loop;

    juce::Component::SafePointer<MainComponent> safe (this);
    inst->stopRecording (sp, [safe] (tl::expected<te::Clip::Array, juce::String> res)
    {
        if (safe != nullptr)
            safe->flyingLayerStopped (std::move (res), false);
    });

    RAFA_T ("fly start toEnd=" + juce::String (toEnd, 3) + " punch=" + juce::String (punch.inSeconds(), 3)
            + " adjust=" + juce::String (engine.getDeviceManager().getRecordAdjustmentMs() * 48.0, 1)
            + " devIn=" + juce::String (engine.getDeviceManager().deviceManager.getCurrentAudioDevice()->getInputLatencyInSamples())
            + " devOut=" + juce::String (engine.getDeviceManager().deviceManager.getCurrentAudioDevice()->getOutputLatencyInSamples())
            + " block=" + juce::String (engine.getDeviceManager().deviceManager.getCurrentAudioDevice()->getCurrentBufferSizeSamples()));
}

void MainComponent::cancelFlyingLayer()
{
    if (! fly.active || fly.stopping || fly.instance == nullptr || edit == nullptr)
        return;

    if (! fly.started)   // zaznam este nezacal - len zrusit pripravu
    {
        fly.active = false;
        trackList.setRecordingTrack (nullptr);
        currentRecordingTrack = nullptr;
        removeLoopBridge();
        setStatus (sk ("Vrstva zrušená."), RafaColours::accent);
        updateTransportButtons();
        return;
    }

    fly.stopping = true;
    auto* ctx = edit->getTransport().getCurrentPlaybackContext();

    te::InputDeviceInstance::StopRecordingParameters sp;
    sp.targetsToStop = { fly.trackID };
    sp.unloopedTimeToEndRecording = ctx != nullptr ? ctx->getUnloopedPosition() : fly.punchStart;
    sp.isLooping = false;
    sp.markedRange = edit->getTransport().getLoopRange();

    fly.instance->prepareToStopRecording (sp.targetsToStop);
    auto res = fly.instance->stopRecording (sp);   // vyvola aj ulozeny callback (flyingLayerStopped)

    if (fly.active)   // callback sa nevolal (napr. nahravanie este nezacalo)
        flyingLayerStopped (std::move (res), true);
}

void MainComponent::flyingLayerStopped (tl::expected<te::Clip::Array, juce::String> res, bool cancelled)
{
    if (! fly.active)
        return;

    cancelled = cancelled || fly.stopping;
    const double L = fly.loopEnd - fly.loopStart;
    fly.active = false;
    trackList.setRecordingTrack (nullptr);
    currentRecordingTrack = nullptr;
    RAFA_T ("fly stopped cancelled=" + juce::String ((int) cancelled) + " ok=" + juce::String ((int) res.has_value())
            + " clips=" + juce::String (res.has_value() ? (int) res->size() : -1));

    if (! res.has_value())
    {
        setStatus (sk ("Vrstva sa nenahrala: ") + res.error(), RafaColours::record);
        updateTransportButtons();
        return;
    }

    for (auto& c : *res)
    {
        auto* clip = dynamic_cast<te::WaveAudioClip*> (c);

        if (clip == nullptr)
            continue;

        const double recorded = te::AudioFile (engine, clip->getOriginalFile()).getLength();

        if (recorded < L - 0.005)
        {
            // neuplny prechod (STOP / REC pocas nahravania): klip odstranime, subor ostava v priecinku
            clip->removeFromParent();
            setStatus (cancelled ? sk ("Vrstva zrušená pred dokončením prechodu (nahrávka ostala v priečinku skladby).")
                                 : sk ("Vrstva je kratšia ako slučka – nepoužitá (nahrávka ostala v priečinku skladby)."),
                       RafaColours::accent);
            continue;
        }

        // subor zacina presne na LOOP IN prechodu - klip na slucku, posun 0
        clip->setPosition ({ { te::TimePosition::fromSeconds (fly.loopStart), te::TimePosition::fromSeconds (fly.loopEnd) },
                             te::TimeDuration() });
        pendingRecordingCheck = true;   // dalej ako bezna vrstva (historia, ulozenie, hlasenie)
        recordingCheckDeadline = juce::Time::getMillisecondCounter() + 4000;
    }

    updateTransportButtons();
}

// casovac: pripravit premostenie ku koncu prechodu letmej vrstvy
void MainComponent::checkFlyingLayer()
{
    if (! fly.active || fly.stopping || edit == nullptr)
        return;

    if (! fly.started)
    {
        if (juce::Time::getMillisecondCounter() - fly.preparedMs >= 80)
            beginFlyingRecording();
        return;
    }

    if (bridgeArmed || bridgePlugin == nullptr)
        return;

    auto* ctx = edit->getTransport().getCurrentPlaybackContext();

    if (ctx == nullptr)
        return;

    const double U = ctx->globalStreamTimeToEditTimeUnlooped (engine.getDeviceManager().getCurrentStreamTime()).inSeconds();
    const double passEnd = fly.punchStart.inSeconds() + (fly.loopEnd - fly.loopStart);

    if (U >= passEnd - 0.40 && U < passEnd - 0.04 && U > fly.punchStart.inSeconds() + 0.6)
        armLoopBridge (fly.loopStart, fly.loopEnd, 0);   // subor zacina presne na LOOP IN
}
