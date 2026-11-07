/*
    Most medzi obrazovkou v HTML (priecinok ui/) a motorom.
    Popis prikazov a dat: ui/API.md  (verzia API 1)

    Ak v ui/ este nie je index.html, alebo sa WebView2 neda spustit,
    pouzije sa klasicka obrazovka v C++ (MainComponent). Klasicku obrazovku
    si mozno vynutit aj podrzanim klavesu Shift pri spusteni programu.
*/

#include "MainComponent.h"
#include "Diagnostics.h"
#include "RafaUIData.h"

static constexpr int rafaApiVersion = 3;

//==============================================================================
static const char* findUIResource (const juce::String& fileName, int& size)
{
    for (int i = 0; i < RafaUI::namedResourceListSize; ++i)
        if (fileName.equalsIgnoreCase (RafaUI::originalFilenames[i]))
            return RafaUI::getNamedResource (RafaUI::namedResourceList[i], size);

    size = 0;
    return nullptr;
}

static juce::String mimeTypeFor (const juce::String& fileName)
{
    auto ext = fileName.fromLastOccurrenceOf (".", false, false).toLowerCase();

    // texty su v UTF-8 (slovencina) - kodovanie musi byt uvedene, inak sa diakritika pokazi
    if (ext == "html" || ext == "htm") return "text/html; charset=utf-8";
    if (ext == "css")                  return "text/css; charset=utf-8";
    if (ext == "js" || ext == "mjs")   return "text/javascript; charset=utf-8";
    if (ext == "json")                 return "application/json; charset=utf-8";
    if (ext == "svg")                  return "image/svg+xml; charset=utf-8";
    if (ext == "png")                  return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif")                  return "image/gif";
    if (ext == "webp")                 return "image/webp";
    if (ext == "ico")                  return "image/x-icon";
    if (ext == "woff")                 return "font/woff";
    if (ext == "woff2")                return "font/woff2";
    if (ext == "ttf")                  return "font/ttf";
    if (ext == "otf")                  return "font/otf";
    if (ext == "wav")                  return "audio/wav";
    return "application/octet-stream";
}

std::optional<juce::WebBrowserComponent::Resource> MainComponent::getWebResource (const juce::String& url)
{
    // url je napr. "/", "/index.html", "/assets/logo.svg?v=2"
    auto path = url.upToFirstOccurrenceOf ("?", false, false)
                   .upToFirstOccurrenceOf ("#", false, false)
                   .trimCharactersAtStart ("/");

    if (path.isEmpty())
        path = "index.html";

    // subory sa balia bez priecinkov - hladame podla mena suboru
    auto fileName = path.fromLastOccurrenceOf ("/", false, false);

    int size = 0;

    if (auto* data = findUIResource (fileName, size))
    {
        juce::WebBrowserComponent::Resource r;
        r.data.resize ((size_t) size);
        std::memcpy (r.data.data(), data, (size_t) size);
        r.mimeType = mimeTypeFor (fileName);
        return r;
    }

    return std::nullopt;
}

//==============================================================================
void MainComponent::setupWebUI()
{
    int size = 0;

    if (findUIResource ("index.html", size) == nullptr)
        return;   // obrazovka v HTML este nie je dodana

    if (juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown())
        return;   // vynutena klasicka obrazovka

    auto userData = RafaPaths::getRootFolder().getChildFile ("WebView");
    userData.createDirectory();

    auto options = juce::WebBrowserComponent::Options{}
       #if JUCE_WINDOWS
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
       #endif
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                    .withUserDataFolder (userData)
                                    .withStatusBarDisabled()
                                    .withBackgroundColour (RafaColours::background))
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withEventListener ("rafaCommand", [this] (const juce::var& v) { handleWebCommand (v); })
        .withResourceProvider ([this] (const juce::String& url) { return getWebResource (url); });

    juce::Logger::writeToLog ("   WebView2: kontrola podpory");

    if (! juce::WebBrowserComponent::areOptionsSupported (options))
    {
        juce::Logger::writeToLog ("   WebView2: NIE JE podporovane");
        setStatus (sk ("WebView2 nie je dostupné – používa sa klasická obrazovka."), RafaColours::accent);
        return;
    }

    juce::Logger::writeToLog ("   WebView2: vytvaram okno prehliadaca");
    webView = std::make_unique<juce::WebBrowserComponent> (options);
    juce::Logger::writeToLog ("   WebView2: okno vytvorene");

    // klasicku obrazovku skryjeme (jej logika ostava a pouziva ju aj HTML)
    for (auto* child : getChildren())
        child->setVisible (false);

    addAndMakeVisible (*webView);
    webUIActive = true;
    resized();

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
}

//==============================================================================
te::AudioTrack* MainComponent::findTrackById (const juce::String& id)
{
    if (edit != nullptr)
        for (auto* t : te::getAudioTracks (*edit))
            if (t->itemID.toString() == id)
                return t;

    return nullptr;
}

void MainComponent::handleWebCommand (const juce::var& message)
{
    auto command = message.getProperty ("command", {}).toString();
    auto args    = message.getProperty ("args", {});

    if (command == "uiReady")
    {
        juce::Logger::writeToLog ("   HTML obrazovka sa nacitala (uiReady)");
        sendStateToWeb();
        sendTracksToWebIfChanged (true);
        return;
    }

    if (edit == nullptr && command != "newSong" && command != "openSong" && command != "openAudioSettings")
        return;

    if      (command == "record")            startRecording();
    else if (command == "play")
    {
        if (! edit->getTransport().isPlaying())
            togglePlay();
    }
    else if (command == "stop")              stopTransport();
    else if (command == "togglePlay")        togglePlay();
    else if (command == "rewind")            rewind();
    else if (command == "seek")
    {
        if (! edit->getTransport().isRecording())
            edit->getTransport().setPosition (te::TimePosition::fromSeconds (juce::jmax (0.0, (double) args.getProperty ("seconds", 0.0))));
    }
    else if (command == "selectInput")
    {
        auto index = inputDeviceIDs.indexOf (args.getProperty ("id", {}).toString());

        if (index >= 0 && ! edit->getTransport().isRecording())
            inputCombo.setSelectedId (index + 1, juce::sendNotificationSync);
    }
    else if (command == "setMonitoring")
    {
        monitorToggle.setToggleState ((bool) args.getProperty ("on", false), juce::dontSendNotification);
        settings->setValue ("monitoring", monitorToggle.getToggleState());
        applyMonitoring();
    }
    else if (command == "trackMute")
    {
        if (auto* t = findTrackById (args.getProperty ("trackId", {}).toString()))
            t->setMute ((bool) args.getProperty ("muted", false));
    }
    else if (command == "trackRename")
    {
        auto name = args.getProperty ("name", {}).toString().trim().substring (0, 60);

        if (auto* t = findTrackById (args.getProperty ("trackId", {}).toString()); t != nullptr && name.isNotEmpty())
        {
            t->setName (name);
            saveEdit();
        }
    }
    else if (command == "trackDelete")
    {
        if (auto* t = findTrackById (args.getProperty ("trackId", {}).toString()))
            deleteTrack (*t);
    }
    else if (command == "newSong")           { if (edit == nullptr || ! edit->getTransport().isRecording()) createNewSong(); }
    else if (command == "openSong")          { if (edit == nullptr || ! edit->getTransport().isRecording()) openSong(); }
    else if (command == "openSongFolder")    { if (editFile != juce::File()) editFile.getParentDirectory().startAsProcess(); }
    else if (command == "exportMix")         { if (! edit->getTransport().isRecording()) exportMix(); }
    else if (command == "openAudioSettings") { if (edit == nullptr || ! edit->getTransport().isRecording()) showAudioSettings(); }
    else if (command == "runDiagnostics")    runDiagnostics();
    // --- API v2: tempo, metronom, LOOP ---
    else if (command == "setTempo")           setTempoBpm ((double) args.getProperty ("bpm", 120.0));
    else if (command == "setTimeSignature")   setTimeSignature ((int) args.getProperty ("numerator", 4), (int) args.getProperty ("denominator", 4));
    else if (command == "setMetronome")       setMetronome ((bool) args.getProperty ("on", false));
    else if (command == "setMetronomeVolume") setMetronomeVolume ((double) args.getProperty ("volume", 0.6));
    else if (command == "setCountIn")         setCountInBars ((int) args.getProperty ("bars", 1));
    else if (command == "setLoop")            setLoopOn ((bool) args.getProperty ("on", false));
    else if (command == "toggleLoop")         setLoopOn (! edit->getTransport().looping.get());
    else if (command == "setLoopBars")        setLoopBars ((int) args.getProperty ("bars", 4));
    else if (command == "setLoopRange")       setLoopRangeSeconds ((double) args.getProperty ("start", 0.0), (double) args.getProperty ("end", 0.0));
    else if (command == "loopIn")             setLoopPointAtPlayhead (true);
    else if (command == "loopOut")            setLoopPointAtPlayhead (false);
    // --- API v3: mix ---
    else if (command == "trackVolume")        setTrackVolumeDb (args.getProperty ("trackId", {}).toString(), (double) args.getProperty ("db", 0.0));
    else if (command == "trackPan")           setTrackPan (args.getProperty ("trackId", {}).toString(), (double) args.getProperty ("pan", 0.0));
    else if (command == "trackSolo")          setTrackSolo (args.getProperty ("trackId", {}).toString(), (bool) args.getProperty ("solo", false));
    else if (command == "masterVolume")       setMasterVolumeDb ((double) args.getProperty ("db", 0.0));
    else
    {
        DBG ("Neznamy prikaz z UI: " << command);
        return;
    }

    // okamzita odozva pre obrazovku
    sendStateToWeb();
    sendTracksToWebIfChanged (false);
}

//==============================================================================
juce::var MainComponent::buildStateObject()
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("apiVersion", rafaApiVersion);

    bool playing = false, recording = false;
    double position = 0.0, length = 0.0;

    if (edit != nullptr)
    {
        auto& tr = edit->getTransport();
        recording = tr.isRecording();
        playing   = tr.isPlaying() || recording;
        position  = tr.getPosition().inSeconds();
        length    = edit->getLength().inSeconds();
    }

    auto transport = std::make_unique<juce::DynamicObject>();
    transport->setProperty ("playing", playing);
    transport->setProperty ("recording", recording);
    transport->setProperty ("position", position);
    // pocas odpocitania (count-in) je pozicia pred zaciatkom nahravania
    transport->setProperty ("countingIn", recording && edit != nullptr
                                            && position < (edit->getTransport().looping ? edit->getTransport().getLoopRange().getStart().inSeconds() : 0.0));
    obj->setProperty ("transport", transport.release());

    auto song = std::make_unique<juce::DynamicObject>();
    song->setProperty ("name", editFile.getFileNameWithoutExtension());
    song->setProperty ("length", length);
    obj->setProperty ("song", song.release());

    auto meter = std::make_unique<juce::DynamicObject>();
    meter->setProperty ("inputDb", inputMeterDb);
    obj->setProperty ("meter", meter.release());

    juce::Array<juce::var> inputs;

    for (int i = 0; i < inputDeviceIDs.size(); ++i)
    {
        auto in = std::make_unique<juce::DynamicObject>();
        in->setProperty ("id", inputDeviceIDs[i]);
        in->setProperty ("name", inputCombo.getItemText (i));
        inputs.add (in.release());
    }

    obj->setProperty ("inputs", inputs);
    obj->setProperty ("selectedInput", getSelectedInputID());
    obj->setProperty ("monitoring", monitorToggle.getToggleState());

    auto device = std::make_unique<juce::DynamicObject>();
    auto& dm  = engine.getDeviceManager();
    auto& adm = dm.deviceManager;

    if (auto* dev = adm.getCurrentAudioDevice())
    {
        auto sr = dev->getCurrentSampleRate();
        auto setup = adm.getAudioDeviceSetup();
        auto totalSamples = dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples()
                          + dev->getCurrentBufferSizeSamples();

        device->setProperty ("ok", true);
        device->setProperty ("driver", adm.getCurrentAudioDeviceType());
        device->setProperty ("input",  setup.inputDeviceName.isNotEmpty()  ? setup.inputDeviceName  : dev->getName());
        device->setProperty ("output", setup.outputDeviceName.isNotEmpty() ? setup.outputDeviceName : dev->getName());
        device->setProperty ("sampleRate", sr);
        device->setProperty ("buffer", dev->getCurrentBufferSizeSamples());
        device->setProperty ("latencyMs", sr > 0 ? juce::roundToInt ((double) totalSamples * 10000.0 / sr) / 10.0 : 0.0);
    }
    else
    {
        device->setProperty ("ok", false);
        device->setProperty ("driver", adm.getCurrentAudioDeviceType());
        device->setProperty ("input", "");
        device->setProperty ("output", "");
        device->setProperty ("sampleRate", 0);
        device->setProperty ("buffer", 0);
        device->setProperty ("latencyMs", 0);
    }

    device->setProperty ("cpu", juce::roundToInt (dm.getCpuUsage() * 100.0f));
    obj->setProperty ("device", device.release());

    auto status = std::make_unique<juce::DynamicObject>();
    status->setProperty ("text", statusLabel.getText());
    status->setProperty ("level", statusLevel);
    obj->setProperty ("status", status.release());

    // API v2: tempo, metronome, loop, musicalPosition
    auto tempoLoop = buildTempoLoopState();   // var musi zit, kym kopirujeme vlastnosti

    if (auto* tl = tempoLoop.getDynamicObject())
        for (auto& p : tl->getProperties())
            obj->setProperty (p.name, p.value);

    obj->setProperty ("mixer", buildMixerState());

    auto busy = std::make_unique<juce::DynamicObject>();
    busy->setProperty ("diagnosticsAllowed", ! playing && ! recording);
    busy->setProperty ("editingAllowed", ! recording);
    obj->setProperty ("busy", busy.release());

    return juce::var (obj.release());
}

void MainComponent::sendStateToWeb()
{
    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible ("rafaState", buildStateObject());
}

//==============================================================================
juce::Array<juce::var> MainComponent::getPeaksFor (te::WaveAudioClip& clip)
{
    constexpr int numPeaks = 200;

    auto file   = clip.getOriginalFile();
    auto pos    = clip.getPosition();
    auto offset = pos.getOffset().inSeconds();
    auto length = pos.getLength().inSeconds();

    auto key = file.getFullPathName() + "|" + juce::String (offset, 3) + "|" + juce::String (length, 3)
             + "|" + juce::String (file.getSize());

    if (auto it = peaksCache.find (key); it != peaksCache.end())
        return it->second;

    juce::Array<juce::var> peaks;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    if (std::unique_ptr<juce::AudioFormatReader> reader { fm.createReaderFor (file) }; reader != nullptr && length > 0.0)
    {
        auto sr = reader->sampleRate;
        auto startSample = (juce::int64) (offset * sr);
        auto totalSamples = juce::jmin ((juce::int64) (length * sr), reader->lengthInSamples - startSample);
        auto block = juce::jmax ((juce::int64) 1, totalSamples / numPeaks);
        float overallMax = 0.0f;
        juce::Array<float> raw;

        for (int i = 0; i < numPeaks; ++i)
        {
            juce::Range<float> ranges[2];
            auto numCh = (int) juce::jmin (2u, reader->numChannels);
            reader->readMaxLevels (startSample + block * i, (int) block, ranges, numCh);

            float m = 0.0f;
            for (int c = 0; c < numCh; ++c)
                m = juce::jmax (m, std::abs (ranges[c].getStart()), std::abs (ranges[c].getEnd()));

            raw.add (m);
            overallMax = juce::jmax (overallMax, m);
        }

        // normalizujeme len jemne, aby tiche nahravky boli viditelne, ale nie prehnane
        auto scale = overallMax > 0.0f ? 1.0f / juce::jmax (overallMax, 0.25f) : 1.0f;

        for (auto v : raw)
            peaks.add (juce::jlimit (0.0, 1.0, (double) (v * scale)));
    }

    peaksCache[key] = peaks;
    return peaks;
}

juce::var MainComponent::buildTracksObject()
{
    juce::Array<juce::var> tracks;
    double viewLength = 30.0;

    if (edit != nullptr)
    {
        auto& tr = edit->getTransport();

        for (auto* t : te::getAudioTracks (*edit))
        {
            auto obj = std::make_unique<juce::DynamicObject>();
            obj->setProperty ("id", t->itemID.toString());
            obj->setProperty ("name", t->getName());
            obj->setProperty ("muted", t->isMuted (false));
            obj->setProperty ("solo", t->isSolo (false));

            if (auto* vol = t->getVolumePlugin())
            {
                obj->setProperty ("volumeDb", juce::roundToInt (vol->getVolumeDb() * 10.0f) / 10.0);
                obj->setProperty ("pan", juce::roundToInt (vol->getPan() * 100.0f) / 100.0);
            }
            obj->setProperty ("recording", tr.isRecording() && t == currentRecordingTrack);

            juce::Array<juce::var> clips;

            for (auto* c : t->getClips())
            {
                if (auto* wave = dynamic_cast<te::WaveAudioClip*> (c))
                {
                    auto pos = wave->getPosition();
                    auto clipObj = std::make_unique<juce::DynamicObject>();
                    clipObj->setProperty ("start", pos.getStart().inSeconds());
                    clipObj->setProperty ("length", pos.getLength().inSeconds());
                    clipObj->setProperty ("peaks", getPeaksFor (*wave));
                    clips.add (clipObj.release());
                }
            }

            obj->setProperty ("clips", clips);
            tracks.add (obj.release());
        }

        viewLength = juce::jmax (viewLength, edit->getLength().inSeconds() + 5.0,
                                 tr.getPosition().inSeconds() + 5.0);
    }

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("tracks", tracks);
    root->setProperty ("viewLength", viewLength);
    return juce::var (root.release());
}

void MainComponent::sendTracksToWebIfChanged (bool force)
{
    if (webView == nullptr)
        return;

    juce::String sig;

    if (edit != nullptr)
    {
        auto& tr = edit->getTransport();
        sig << (tr.isRecording() ? "R" : "-") << juce::String ((juce::pointer_sized_int) currentRecordingTrack);

        for (auto* t : te::getAudioTracks (*edit))
        {
            sig << "|" << t->itemID.toString() << ":" << t->getName() << ":" << (int) t->isMuted (false)
                << ":" << (int) t->isSolo (false);

            if (auto* vol = t->getVolumePlugin())
                sig << ":" << juce::String (vol->getVolumeDb(), 1) << ":" << juce::String (vol->getPan(), 2);

            for (auto* c : t->getClips())
            {
                auto p = c->getPosition();
                sig << "," << c->itemID.toString() << "@" << juce::String (p.getStart().inSeconds(), 2)
                    << "+" << juce::String (p.getLength().inSeconds(), 2);
            }
        }

        // pocas nahravania sa posuva odporucana dlzka osi
        if (tr.isRecording() || tr.getPosition().inSeconds() + 5.0 > 30.0)
            sig << "#" << (int) ((tr.getPosition().inSeconds() + 5.0) / 5.0);
    }

    if (! force && sig == lastTracksSignature)
        return;

    lastTracksSignature = sig;
    webView->emitEventIfBrowserIsVisible ("rafaTracks", buildTracksObject());
}
