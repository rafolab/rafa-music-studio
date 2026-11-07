#include "MainComponent.h"
#include "Diagnostics.h"

//==============================================================================
// InputMeter
//==============================================================================
void InputMeter::setLevel (float linearLevel)
{
    setLevelDb (juce::Decibels::gainToDecibels (linearLevel, -100.0f));
}

void InputMeter::setLevelDb (float db)
{

    // rychly nabeh, pomalsi dobeh
    levelDb = db > levelDb ? db : juce::jmax (db, levelDb - 1.5f);

    if (db >= peakDb)
    {
        peakDb = db;
        peakHoldFrames = 45;
    }
    else if (--peakHoldFrames <= 0)
    {
        peakDb = juce::jmax (db, peakDb - 1.0f);
    }

    repaint();
}

void InputMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (RafaColours::panelLight);
    g.fillRoundedRectangle (r, 4.0f);

    auto toX = [&r] (float db)
    {
        return r.getX() + r.getWidth() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    };

    auto inner = r.reduced (2.0f);
    auto x = toX (levelDb);

    // farebne pasma: zelena do -12 dB, zlta do -3 dB, cervena nad
    auto greenEnd  = toX (-12.0f);
    auto yellowEnd = toX (-3.0f);

    g.setColour (RafaColours::meterLow);
    g.fillRect (juce::Rectangle<float> (inner.getX(), inner.getY(), juce::jmin (x, greenEnd) - inner.getX(), inner.getHeight()));

    if (x > greenEnd)
    {
        g.setColour (RafaColours::meterMid);
        g.fillRect (juce::Rectangle<float> (greenEnd, inner.getY(), juce::jmin (x, yellowEnd) - greenEnd, inner.getHeight()));
    }

    if (x > yellowEnd)
    {
        g.setColour (RafaColours::meterHigh);
        g.fillRect (juce::Rectangle<float> (yellowEnd, inner.getY(), x - yellowEnd, inner.getHeight()));
    }

    if (peakDb > -60.0f)
    {
        g.setColour (RafaColours::text);
        g.fillRect (toX (peakDb) - 1.0f, inner.getY(), 2.0f, inner.getHeight());
    }

    g.setColour (RafaColours::text.withAlpha (0.5f));
    g.setFont (juce::FontOptions (11.0f));

    for (auto mark : { -48.0f, -24.0f, -12.0f, -6.0f, 0.0f })
    {
        auto mx = toX (mark);
        g.drawVerticalLine ((int) mx, r.getBottom() - 5.0f, r.getBottom());
    }
}

//==============================================================================
// MainComponent
//==============================================================================
MainComponent::MainComponent (te::Engine& e)
    : engine (e)
{
    setLookAndFeel (&lookAndFeel);

    juce::PropertiesFile::Options opts;
    opts.applicationName     = "RAFA MUSIC STUDIO";
    opts.filenameSuffix      = "settings";
    opts.folderName          = "RAFA MUSIC STUDIO";
    opts.osxLibrarySubFolder = "Application Support";
    settings = std::make_unique<juce::PropertiesFile> (opts);

    // ---- hlavicka
    titleLabel.setText ("RAFA MUSIC STUDIO", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, RafaColours::accent);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText (sk ("Etapa 1 · skúška zvuku"), juce::dontSendNotification);
    subtitleLabel.setColour (juce::Label::textColourId, RafaColours::textDim);
    subtitleLabel.setFont (juce::FontOptions (14.0f));
    addAndMakeVisible (subtitleLabel);

    songLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    songLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (songLabel);

    newSongButton.setButtonText (sk ("Nová skladba"));
    newSongButton.onClick = [this] { createNewSong(); };
    openSongButton.setButtonText (sk ("Otvoriť skladbu"));
    openSongButton.onClick = [this] { openSong(); };
    folderButton.setButtonText (sk ("Priečinok skladby"));
    folderButton.onClick = [this]
    {
        if (editFile != juce::File())
            editFile.getParentDirectory().startAsProcess();
    };
    exportButton.setButtonText (sk ("Exportovať mix (WAV)"));
    exportButton.onClick = [this] { exportMix(); };
    audioSettingsButton.setButtonText (sk ("Nastavenia zvuku"));
    audioSettingsButton.onClick = [this] { showAudioSettings(); };
    diagnosticsButton.setButtonText (sk ("Diagnostika"));
    diagnosticsButton.onClick = [this] { runDiagnostics(); };

    for (auto* b : { &newSongButton, &openSongButton, &folderButton, &exportButton, &audioSettingsButton, &diagnosticsButton })
        addAndMakeVisible (b);

    // ---- zariadenie
    deviceTitleLabel.setText (sk ("Zvukové zariadenie"), juce::dontSendNotification);
    deviceTitleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    addAndMakeVisible (deviceTitleLabel);

    deviceInfoLabel.setFont (juce::FontOptions (14.0f));
    deviceInfoLabel.setJustificationType (juce::Justification::topLeft);
    deviceInfoLabel.setColour (juce::Label::textColourId, RafaColours::textDim);
    addAndMakeVisible (deviceInfoLabel);

    inputTitleLabel.setText (sk ("Vstup pre gitaru:"), juce::dontSendNotification);
    inputTitleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    addAndMakeVisible (inputTitleLabel);

    inputCombo.setTextWhenNoChoicesAvailable (sk ("Žiadny vstup – otvor Nastavenia zvuku"));
    inputCombo.onChange = [this]
    {
        settings->setValue ("inputDeviceID", getSelectedInputID());
        settings->saveIfNeeded();
        attachMeterToSelectedInput();
    };
    addAndMakeVisible (inputCombo);

    monitorToggle.setButtonText (sk ("Počúvať vstup cez počítač"));
    monitorToggle.setTooltip (sk ("Vypni, ak počuješ gitaru priamo z komba – inak bude zvuk zdvojený."));
    monitorToggle.setToggleState (settings->getBoolValue ("monitoring", false), juce::dontSendNotification);
    monitorToggle.onClick = [this]
    {
        settings->setValue ("monitoring", monitorToggle.getToggleState());
        applyMonitoring();
    };
    addAndMakeVisible (monitorToggle);
    addAndMakeVisible (inputMeter);

    // ---- transport
    recordButton.setButtonText (sk ("NAHRÁVAŤ  (R)"));
    recordButton.setColour (juce::TextButton::buttonColourId, RafaColours::record.darker (0.3f));
    recordButton.onClick = [this] { startRecording(); };

    playButton.setButtonText (sk ("PREHRAŤ  (medzerník)"));
    playButton.setColour (juce::TextButton::buttonColourId, RafaColours::play.darker (0.4f));
    playButton.onClick = [this] { togglePlay(); };

    stopButton.setButtonText ("STOP");
    stopButton.onClick = [this] { stopTransport(); };

    rewindButton.setButtonText (sk ("Na začiatok"));
    rewindButton.onClick = [this] { rewind(); };

    for (auto* b : { &recordButton, &playButton, &stopButton, &rewindButton })
        addAndMakeVisible (b);

    timeLabel.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    timeLabel.setJustificationType (juce::Justification::centred);
    timeLabel.setColour (juce::Label::textColourId, RafaColours::text);
    addAndMakeVisible (timeLabel);

    statusLabel.setFont (juce::FontOptions (14.0f));
    addAndMakeVisible (statusLabel);

    // ---- stopy
    trackList.onDeleteTrack = [this] (te::AudioTrack& t) { deleteTrack (t); };
    trackViewport.setViewedComponent (&trackList, false);
    trackViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (trackViewport);

    // ---- motor
    juce::Logger::writeToLog ("2a. obrazovka vytvorena, pripajam zariadenia");
    auto& dm = engine.getDeviceManager();
    dm.addChangeListener (this);
    ensureMonoInputs();
    refreshInputList();
    attachMeterToSelectedInput();
    updateDeviceInfo();

    juce::Logger::writeToLog ("2b. zariadenia: " + dm.deviceManager.getCurrentAudioDeviceType()
                              + " / " + (dm.deviceManager.getCurrentAudioDevice() != nullptr
                                            ? dm.deviceManager.getCurrentAudioDevice()->getName() : juce::String ("ziadne")));

    auto last = juce::File (settings->getValue ("lastSong"));
    juce::Logger::writeToLog ("2c. otvaram skladbu: " + last.getFullPathName());

    if (last.existsAsFile())
        loadEdit (last);
    else
        createNewSong();

    setWantsKeyboardFocus (true);
    setSize (1180, 790);
    startTimerHz (30);

    setStatus (sk ("Pripravené. Vyber vstup pre gitaru a stlač NAHRÁVAŤ."));

    juce::Logger::writeToLog ("2d. skladba otvorena, spustam HTML obrazovku");
    setupWebUI();
    juce::Logger::writeToLog ("2e. HTML obrazovka: " + juce::String (webUIActive ? "aktivna" : "klasicka obrazovka"));
}

MainComponent::~MainComponent()
{
    stopTimer();
    webView = nullptr;
    engine.getDeviceManager().removeChangeListener (this);
    detachMeter();
    closeEdit();
    setLookAndFeel (nullptr);
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (RafaColours::background);

    auto r = getLocalBounds();
    auto header = r.removeFromTop (136);

    g.setColour (RafaColours::panel);
    g.fillRect (header);

    auto devicePanel = r.removeFromTop (150).reduced (16, 10);
    g.setColour (RafaColours::panel);
    g.fillRoundedRectangle (devicePanel.toFloat(), 10.0f);

    auto transportPanel = r.removeFromTop (90).reduced (16, 6);
    g.setColour (RafaColours::panel);
    g.fillRoundedRectangle (transportPanel.toFloat(), 10.0f);
}

void MainComponent::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());

    auto r = getLocalBounds();

    // ---- hlavicka
    auto header = r.removeFromTop (136).reduced (20, 10);
    auto top = header.removeFromTop (44);
    titleLabel.setBounds (top.removeFromLeft (360));
    songLabel.setBounds (top);
    subtitleLabel.setBounds (header.removeFromTop (18).withWidth (400));
    header.removeFromTop (10);

    auto buttons = header.removeFromTop (36);
    auto bw = 160;
    for (auto* b : { &newSongButton, &openSongButton, &folderButton, &exportButton })
    {
        b->setBounds (buttons.removeFromLeft (bw).reduced (3, 0));
    }
    diagnosticsButton.setBounds (buttons.removeFromRight (130).reduced (3, 0));
    audioSettingsButton.setBounds (buttons.removeFromRight (170).reduced (3, 0));

    // ---- zariadenie
    auto device = r.removeFromTop (150).reduced (16, 10).reduced (16, 10);
    auto left = device.removeFromLeft (device.getWidth() / 2);
    deviceTitleLabel.setBounds (left.removeFromTop (22));
    deviceInfoLabel.setBounds (left);

    auto right = device;
    inputTitleLabel.setBounds (right.removeFromTop (22));
    right.removeFromTop (4);
    inputCombo.setBounds (right.removeFromTop (30));
    right.removeFromTop (8);
    inputMeter.setBounds (right.removeFromTop (18));
    right.removeFromTop (6);
    monitorToggle.setBounds (right.removeFromTop (26));

    // ---- transport
    auto transport = r.removeFromTop (90).reduced (16, 6).reduced (12, 12);
    recordButton.setBounds (transport.removeFromLeft (190).reduced (4, 0));
    playButton.setBounds (transport.removeFromLeft (220).reduced (4, 0));
    stopButton.setBounds (transport.removeFromLeft (110).reduced (4, 0));
    rewindButton.setBounds (transport.removeFromLeft (130).reduced (4, 0));
    timeLabel.setBounds (transport.removeFromLeft (170));
    statusLabel.setBounds (transport.reduced (8, 0));

    // ---- stopy
    trackViewport.setBounds (r.reduced (16, 10));
    trackList.setSize (trackViewport.getMaximumVisibleWidth(), trackList.getIdealHeight());
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        togglePlay();
        return true;
    }

    if (key.getTextCharacter() == 'r' || key.getTextCharacter() == 'R')
    {
        startRecording();
        return true;
    }

    if (key == juce::KeyPress::homeKey)
    {
        rewind();
        return true;
    }

    return false;
}

void MainComponent::setStatus (const juce::String& text, juce::Colour colour)
{
    if (colour == RafaColours::play)        statusLevel = "ok";
    else if (colour == RafaColours::accent) statusLevel = "warn";
    else if (colour == RafaColours::record) statusLevel = text.startsWith (sk ("●")) ? "recording" : "error";
    else                                    statusLevel = "info";

    statusLabel.setText (text, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, colour);
}

//==============================================================================
// Skladby
//==============================================================================
void MainComponent::createNewSong()
{
    auto name = "Skladba " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M");
    auto folder = RafaPaths::getSongsFolder().getNonexistentChildFile (name, {}, false);
    folder.createDirectory();

    loadEdit (folder.getChildFile (folder.getFileName() + ".tracktionedit"));
    setStatus (sk ("Nová skladba vytvorená."));
}

void MainComponent::openSong()
{
    auto chooser = std::make_shared<juce::FileChooser> (sk ("Otvoriť skladbu"),
                                                        RafaPaths::getSongsFolder(), "*.tracktionedit");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, chooser] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();

                              if (f.existsAsFile())
                              {
                                  loadEdit (f);
                                  setStatus (sk ("Skladba otvorená."));
                              }
                          });
}

void MainComponent::closeEdit()
{
    if (edit == nullptr)
        return;

    edit->getTransport().stop (false, false);
    saveEdit();
    detachAllTrackMeters();
    edit->getTransport().removeChangeListener (this);
    trackList.setEdit (nullptr);
    currentRecordingTrack = nullptr;
    edit = nullptr;
}

void MainComponent::loadEdit (const juce::File& file)
{
    closeEdit();

    editFile = file;
    editFile.getParentDirectory().createDirectory();

    if (editFile.existsAsFile())
        edit = te::loadEditFromFile (engine, editFile);
    else
        edit = te::createEmptyEdit (engine, editFile);

    if (edit == nullptr)
    {
        setStatus (sk ("Skladbu sa nepodarilo otvoriť."), RafaColours::record);
        return;
    }

    edit->playInStopEnabled = true;
    edit->getTransport().addChangeListener (this);
    edit->getTransport().ensureContextAllocated();

    // predvolene anglicke mena stop prelozime
    for (auto* t : te::getAudioTracks (*edit))
        if (t->getName().startsWith ("Track "))
            t->setName (sk ("Stopa ") + t->getName().fromFirstOccurrenceOf ("Track ", false, false));

    initTempoDefaults();
    applyMonitoring();
    trackList.setEdit (edit.get());
    resized();

    songLabel.setText (editFile.getFileNameWithoutExtension(), juce::dontSendNotification);
    settings->setValue ("lastSong", editFile.getFullPathName());
    settings->saveIfNeeded();

    saveEdit();
    updateTransportButtons();
}

void MainComponent::saveEdit()
{
    if (edit != nullptr)
        te::EditFileOperations (*edit).save (false, true, false);
}

//==============================================================================
// Zariadenia a vstupy
//==============================================================================
void MainComponent::ensureMonoInputs()
{
    // Gitaru nahravame mono: kazdy vstupny kanal zariadenia bude samostatny vstup.
    auto& dm = engine.getDeviceManager();
    bool needsChange = false;

    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
        if (auto* w = dm.getWaveInDevice (i))
            if (w->getChannels().getNumChannels() != 1)
                needsChange = true;

    if (needsChange)
        dm.setAllWaveInputsToNumChannels (1);

    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
        if (auto* w = dm.getWaveInDevice (i))
            if (! w->isEnabled())
                dm.setDeviceEnabled (*w, true);
}

void MainComponent::refreshInputList()
{
    auto& dm = engine.getDeviceManager();
    auto previous = getSelectedInputID();

    if (previous.isEmpty())
        previous = settings->getValue ("inputDeviceID");

    inputCombo.clear (juce::dontSendNotification);
    inputDeviceIDs.clear();

    juce::String deviceName;

    // meno vstupneho zariadenia (pri Windows Audio je getName() meno vystupu, napr. reproduktorov)
    if (auto* dev = dm.deviceManager.getCurrentAudioDevice())
    {
        deviceName = dm.deviceManager.getAudioDeviceSetup().inputDeviceName;

        if (deviceName.isEmpty())
            deviceName = dev->getName();
    }

    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
    {
        if (auto* w = dm.getWaveInDevice (i))
        {
            inputDeviceIDs.add (w->getDeviceID());
            inputCombo.addItem (w->getName() + (deviceName.isNotEmpty() ? "   (" + deviceName + ")" : juce::String()),
                                inputDeviceIDs.size());
        }
    }

    auto index = inputDeviceIDs.indexOf (previous);
    inputCombo.setSelectedId (index >= 0 ? index + 1 : (inputDeviceIDs.isEmpty() ? 0 : 1),
                              juce::dontSendNotification);
}

juce::String MainComponent::getSelectedInputID() const
{
    auto id = inputCombo.getSelectedId();
    return id > 0 ? inputDeviceIDs[id - 1] : juce::String();
}

void MainComponent::detachMeter()
{
    // Zariadenie mohlo medzitym zaniknut (napr. odpojene kombo) - odhlasime sa len z existujuceho
    if (meteredDevice != nullptr)
    {
        auto& dm = engine.getDeviceManager();

        for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
            if (dm.getWaveInDevice (i) == meteredDevice)
                meteredDevice->levelMeasurer.removeClient (meterClient);
    }

    meteredDevice = nullptr;
    meterClient.reset();
}

void MainComponent::attachMeterToSelectedInput()
{
    detachMeter();

    auto id = getSelectedInputID();
    auto& dm = engine.getDeviceManager();

    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
    {
        if (auto* w = dm.getWaveInDevice (i); w != nullptr && w->getDeviceID() == id)
        {
            meteredDevice = w;
            meteredDevice->levelMeasurer.addClient (meterClient);
            break;
        }
    }
}

void MainComponent::applyMonitoring()
{
    auto mode = monitorToggle.getToggleState() ? te::InputDevice::MonitorMode::on
                                               : te::InputDevice::MonitorMode::off;

    auto& dm = engine.getDeviceManager();

    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
        if (auto* w = dm.getWaveInDevice (i))
            w->setMonitorMode (mode);
}

void MainComponent::updateDeviceInfo()
{
    auto& dm  = engine.getDeviceManager();
    auto& adm = dm.deviceManager;

    juce::String info;

    if (auto* dev = adm.getCurrentAudioDevice())
    {
        auto sr = dev->getCurrentSampleRate();
        auto msOf = [sr] (int samples) { return sr > 0 ? juce::String ((double) samples * 1000.0 / sr, 1) : juce::String ("?"); };

        auto setup = adm.getAudioDeviceSetup();
        auto inName  = setup.inputDeviceName.isNotEmpty()  ? setup.inputDeviceName  : dev->getName();
        auto outName = setup.outputDeviceName.isNotEmpty() ? setup.outputDeviceName : dev->getName();

        auto totalMs = msOf (dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples()
                             + dev->getCurrentBufferSizeSamples());

        info << sk ("Ovládač: ") << adm.getCurrentAudioDeviceType() << "\n"
             << sk ("Vstup: ") << inName << "\n"
             << sk ("Výstup: ") << outName << "\n"
             << juce::String (sr / 1000.0, 1) << " kHz  ·  buffer " << dev->getCurrentBufferSizeSamples()
             << sk (" vzoriek  ·  odhad oneskorenia ") << totalMs << " ms"
             << "  ·  CPU " << juce::String (dm.getCpuUsage() * 100.0f, 0) << " %";
    }
    else
    {
        info << sk ("Žiadne zvukové zariadenie nie je otvorené.\nKlikni na „Nastavenia zvuku“ a vyber zariadenie.");
    }

    deviceInfoLabel.setText (info, juce::dontSendNotification);
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (engine.getDeviceManager().deviceManager,
                                                             0, 64,      // vstupy
                                                             0, 64,      // vystupy
                                                             false,      // MIDI vstupy (Etapa 3)
                                                             false,      // MIDI vystupy
                                                             false,      // kanaly samostatne, nie v paroch
                                                             false);
    selector->setSize (560, 520);

    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = sk ("Nastavenia zvuku");
    o.dialogBackgroundColour = RafaColours::panel;
    o.content.setOwned (selector);
    o.resizable = true;
    o.useNativeTitleBar = true;
    o.launchAsync();
}

void MainComponent::runDiagnostics()
{
    // Skenovanie zariadeni moze narusit zvuk - len ked je vsetko zastavene
    if (edit != nullptr && (edit->getTransport().isPlaying() || edit->getTransport().isRecording()))
    {
        setStatus (sk ("Diagnostiku spusti, keď je prehrávanie zastavené."), RafaColours::accent);
        return;
    }

    auto report = Diagnostics::buildReport (engine, edit.get(), editFile);
    auto file = Diagnostics::saveReport (report);
    juce::SystemClipboard::copyTextToClipboard (report);

    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
        sk ("Diagnostika"),
        sk ("Správa je skopírovaná do schránky – stačí ju vložiť do chatu (Ctrl+V).\n\nUložená je aj tu:\n")
            + file.getFullPathName());
}

void MainComponent::exportMix()
{
    if (edit == nullptr)
        return;

    if (te::getAudioTracks (*edit).isEmpty() || edit->getLength().inSeconds() <= 0.0)
    {
        setStatus (sk ("Nie je čo exportovať – najprv niečo nahraj."), RafaColours::accent);
        return;
    }

    stopTransport();
    saveEdit();

    auto exportFolder = editFile.getParentDirectory().getChildFile ("Export");
    exportFolder.createDirectory();
    auto out = exportFolder.getNonexistentChildFile (editFile.getFileNameWithoutExtension() + " - mix", ".wav", false);

    setStatus (sk ("Exportujem…"));

    if (te::Renderer::renderToFile (*edit, out, false))
    {
        setStatus (sk ("Mix uložený: ") + out.getFileName(), RafaColours::play);
        out.revealToUser();
    }
    else
    {
        setStatus (sk ("Export sa nepodaril."), RafaColours::record);
    }
}

//==============================================================================
// Transport
//==============================================================================
te::AudioTrack* MainComponent::prepareTrackForRecording()
{
    auto inputID = getSelectedInputID();

    if (inputID.isEmpty())
    {
        setStatus (sk ("Najprv vyber vstup pre gitaru."), RafaColours::accent);
        return nullptr;
    }

    // Pouzijeme poslednu prazdnu stopu, inak pridame novu
    auto tracks = te::getAudioTracks (*edit);
    te::AudioTrack* target = nullptr;

    if (! tracks.isEmpty() && tracks.getLast()->getClips().isEmpty())
        target = tracks.getLast();

    if (target == nullptr)
    {
        edit->ensureNumberOfAudioTracks (tracks.size() + 1);
        target = te::getAudioTracks (*edit).getLast();
    }

    if (target == nullptr)
        return nullptr;

    target->setName (sk ("Gitara ") + juce::String (te::getAudioTracks (*edit).indexOf (target) + 1));

    edit->getTransport().ensureContextAllocated();

    bool assigned = false;
    juce::String assignError;

    for (auto* instance : edit->getAllInputDevices())
    {
        if (instance->getInputDevice().getDeviceType() != te::InputDevice::waveDevice)
            continue;

        // vsetko ostatne odzbrojime
        for (auto id : instance->getTargets())
            instance->setRecordingEnabled (id, false);

        if (instance->getInputDevice().getDeviceID() == inputID)
        {
            auto result = instance->setTarget (target->itemID, true, &edit->getUndoManager(), 0);

            if (! result.has_value())
            {
                assignError = result.error();
                continue;
            }

            instance->setRecordingEnabled (target->itemID, true);

            // overime, ze vstup je naozaj nasmerovany do tejto stopy a pripraveny nahravat
            if (instance->getTargets().contains (target->itemID)
                 && instance->isRecordingEnabled (target->itemID))
                assigned = true;
            else
                assignError = sk ("vstup sa nepodarilo zapnúť na nahrávanie");
        }
    }

    if (! assigned)
    {
        if (assignError.isEmpty())
            assignError = sk ("vybraný vstup sa nenašiel – skontroluj Nastavenia zvuku");

        setStatus (sk ("Nahrávanie sa nespustilo: ") + assignError, RafaColours::record);
        return nullptr;
    }

    return target;
}

void MainComponent::startRecording()
{
    if (edit == nullptr)
        return;

    auto& transport = edit->getTransport();

    if (transport.isRecording())
    {
        // v slucke REC ukonci vrstvu a slucka hra dalej; inak REC = STOP
        if (loopRecordPass)
            finishLoopRecordingPass();
        else
            stopTransport();

        return;
    }

    if (transport.looping && transport.getLoopRange().getLength().inSeconds() < 2.0)
    {
        setStatus (sk ("Slučka je príliš krátka na nahrávanie (minimum 2 s). Predĺž ju."), RafaColours::accent);
        return;
    }

    if (transport.isPlaying())
        transport.stop (false, false);

    currentRecordingTrack = prepareTrackForRecording();

    if (currentRecordingTrack == nullptr)
        return;

    recordingTrackID = currentRecordingTrack->itemID;
    clipsBeforeRecording = currentRecordingTrack->getClips().size();
    pendingRecordingCheck = false;

    trackList.refreshIfChanged();
    trackList.setRecordingTrack (currentRecordingTrack);

    if (transport.looping)
    {
        // Slucka: nahra sa presne jeden prechod od LOOP IN po LOOP OUT, potom slucka hra dalej
        loopRecordPass = true;
        loopWrapped = false;
        lastLoopPosition = -1.0e9;
        transport.setPosition (transport.getLoopRange().getStart());
        transport.record (false);
        setStatus (sk ("● Nahrávam vrstvu slučky do stopy „") + currentRecordingTrack->getName()
                     + sk ("“ – jeden prechod, potom hrá ďalej."), RafaColours::record);
    }
    else
    {
        // Rezim vrstvenia: kazda nova vrstva zacina od zaciatku skladby (pozicia 0),
        // aby sedela s predchadzajucimi. Nahravanie z aktualnej pozicie zatial nie je.
        transport.setPosition (te::TimePosition());
        transport.record (false);
        setStatus (sk ("● Nahrávam od začiatku do stopy „") + currentRecordingTrack->getName() + sk ("“ – STOP ukončí nahrávanie."),
                   RafaColours::record);
    }
    updateTransportButtons();
}

void MainComponent::togglePlay()
{
    if (edit == nullptr)
        return;

    auto& transport = edit->getTransport();

    if (transport.isPlaying() || transport.isRecording())
        stopTransport();
    else
    {
        if (transport.looping)
        {
            auto range = transport.getLoopRange();
            auto pos = transport.getPosition();

            if (pos < range.getStart() || pos >= range.getEnd())
                transport.setPosition (range.getStart());
        }

        transport.play (false);
        setStatus (transport.looping ? sk ("Prehrávam slučku…") : sk ("Prehrávam…"), RafaColours::play);
    }

    updateTransportButtons();
}

void MainComponent::stopTransport()
{
    if (edit == nullptr)
        return;

    auto& transport = edit->getTransport();
    const bool wasRecording = transport.isRecording();
    loopRecordPass = false;

    transport.stop (false, false);

    if (wasRecording)
    {
        // klip vznika az po ukonceni nahravania - overime ho v casovaci
        pendingRecordingCheck = true;
        recordingCheckDeadline = juce::Time::getMillisecondCounter() + 4000;
        setStatus (sk ("Ukladám nahrávku…"));
    }
    else
    {
        setStatus (sk ("Zastavené."));
    }

    trackList.setRecordingTrack (nullptr);
    currentRecordingTrack = nullptr;
    updateTransportButtons();
}

void MainComponent::rewind()
{
    if (edit == nullptr || edit->getTransport().isRecording())
        return;

    auto& tr = edit->getTransport();
    tr.setPosition (tr.looping ? tr.getLoopRange().getStart() : te::TimePosition());
}

void MainComponent::checkFinishedRecording()
{
    if (edit == nullptr)
    {
        pendingRecordingCheck = false;
        return;
    }

    auto& transport = edit->getTransport();

    if (transport.isRecording() || transport.isRecordingStopping())
        return;

    te::AudioTrack* track = nullptr;

    for (auto* t : te::getAudioTracks (*edit))
        if (t->itemID == recordingTrackID)
            track = t;

    te::WaveAudioClip* newClip = nullptr;

    if (track != nullptr && track->getClips().size() > clipsBeforeRecording)
        newClip = dynamic_cast<te::WaveAudioClip*> (track->getClips().getLast());

    if (newClip != nullptr)
    {
        // Odpocitanie (count-in) sa nahrava tiez - klip orezeme na zaciatok
        // skladby / slucky a v slucke aj na jej koniec (zvuk ostava na mieste).
        {
            auto& tr = edit->getTransport();
            auto loopRange = tr.getLoopRange();
            auto minStart = tr.looping ? loopRange.getStart() : te::TimePosition();

            if (newClip->getPosition().getStart() < minStart)
                newClip->setStart (minStart, true, false);

            if (tr.looping && newClip->getPosition().getEnd() > loopRange.getEnd())
                newClip->setEnd (loopRange.getEnd(), true);
        }

        auto file = newClip->getOriginalFile();
        auto seconds = newClip->getPosition().getLength().inSeconds();

        if (file.existsAsFile() && file.getSize() > 0 && seconds > 0.0)
        {
            pendingRecordingCheck = false;
            saveEdit();
            if (transport.looping && transport.isPlaying())
                setStatus (sk ("Vrstva uložená (") + juce::String (seconds, 1)
                             + sk (" s) – slučka hrá ďalej. REC = ďalšia vrstva."), RafaColours::play);
            else
                setStatus (sk ("Nahrávka uložená (") + juce::String (seconds, 1)
                             + sk (" s). Stlač PREHRAŤ alebo nahraj ďalšiu vrstvu."), RafaColours::play);
            return;
        }
    }

    if (juce::Time::getMillisecondCounter() > recordingCheckDeadline)
    {
        pendingRecordingCheck = false;
        saveEdit();
        setStatus (sk ("Nahrávka nevznikla alebo je prázdna. Skontroluj vstup a pošli Diagnostiku."),
                   RafaColours::record);
    }
}

void MainComponent::deleteTrack (te::AudioTrack& t)
{
    if (edit == nullptr || edit->getTransport().isRecording())
        return;

    auto name = t.getName();
    juce::Component::SafePointer<MainComponent> safeThis (this);
    juce::ReferenceCountedObjectPtr<te::AudioTrack> trackPtr (&t);
    auto* editAtRequest = edit.get();

    auto options = juce::MessageBoxOptions{}
                       .withIconType (juce::MessageBoxIconType::QuestionIcon)
                       .withTitle (sk ("Zmazať stopu"))
                       .withMessage (sk ("Naozaj zmazať stopu „") + name + sk ("“?\n(Zvukový súbor ostane v priečinku skladby.)"))
                       .withButton (sk ("Zmazať"), 1)
                       .withButton (sk ("Zrušiť"), 0);

    juce::AlertWindow::showAsync (options, [safeThis, trackPtr, editAtRequest] (int result)
    {
        if (result != 1 || safeThis == nullptr || safeThis->edit == nullptr)
            return;

        // kym bolo otvorene okno, mohla sa zmenit skladba alebo zacat nahravanie
        auto& ed = *safeThis->edit;

        if (&ed != editAtRequest || ed.getTransport().isRecording()
             || ! te::getAudioTracks (ed).contains (trackPtr.get()))
            return;

        safeThis->edit->getTransport().stop (false, false);

        // merac stopy odpojime skor, nez stopa zanikne
        auto meterIt = safeThis->trackMeters.find (trackPtr->itemID.toString());

        if (meterIt != safeThis->trackMeters.end())
        {
            if (meterIt->second.measurer != nullptr && meterIt->second.client != nullptr)
                meterIt->second.measurer->removeClient (*meterIt->second.client);

            safeThis->trackMeters.erase (meterIt);
        }

        safeThis->edit->deleteTrack (trackPtr.get());
        safeThis->saveEdit();
        safeThis->trackList.refreshIfChanged();
        safeThis->setStatus (sk ("Stopa zmazaná."));
    });
}

void MainComponent::updateTransportButtons()
{
    if (edit == nullptr)
        return;

    auto& transport = edit->getTransport();

    recordButton.setButtonText (transport.isRecording() ? sk ("■ UKONČIŤ NAHRÁVANIE") : sk ("NAHRÁVAŤ  (R)"));
    playButton.setButtonText (transport.isPlaying() && ! transport.isRecording() ? sk ("PAUZA  (medzerník)")
                                                                                 : sk ("PREHRAŤ  (medzerník)"));

    for (auto* b : { &newSongButton, &openSongButton, &exportButton, &audioSettingsButton })
        b->setEnabled (! transport.isRecording());

    diagnosticsButton.setEnabled (! transport.isRecording() && ! transport.isPlaying());

    inputCombo.setEnabled (! transport.isRecording());
}

//==============================================================================
void MainComponent::timerCallback()
{
    {
        auto db = meteredDevice != nullptr ? meterClient.getAndClearAudioLevel (0).dB : -100.0f;
        inputMeterDb = db > inputMeterDb ? db : juce::jmax (db, inputMeterDb - 1.5f);

        if (! webUIActive)
            inputMeter.setLevelDb (db);
    }

    checkLoopRecordingPass();
    syncTrackMeters();

    if (pendingRecordingCheck)
        checkFinishedRecording();

    if (edit != nullptr)
    {
        auto secs = edit->getTransport().getPosition().inSeconds();
        auto mins = (int) (secs / 60.0);
        timeLabel.setText (juce::String::formatted ("%d:%04.1f", mins, secs - mins * 60.0),
                           juce::dontSendNotification);
        timeLabel.setColour (juce::Label::textColourId,
                             edit->getTransport().isRecording() ? RafaColours::record : RafaColours::text);

        trackList.updatePlayhead();
    }

    if (++timerTicks % 6 == 0)
    {
        trackList.refreshIfChanged();

        if (trackList.getHeight() != trackList.getIdealHeight())
            trackList.setSize (trackViewport.getMaximumVisibleWidth(), trackList.getIdealHeight());
    }

    if (timerTicks % 15 == 0)
        updateDeviceInfo();

    if (webUIActive)
    {
        if (timerTicks % 2 == 0)
            sendStateToWeb();

        if (timerTicks % 6 == 0)
            sendTracksToWebIfChanged (false);
    }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &engine.getDeviceManager())
    {
        // zmenilo sa zvukove zariadenie (napr. pripojene kombo alebo zmena v nastaveniach)
        detachMeter();
        ensureMonoInputs();
        applyMonitoring();
        refreshInputList();
        attachMeterToSelectedInput();
        updateDeviceInfo();

        if (edit != nullptr && ! edit->getTransport().isRecording())
            edit->getTransport().ensureContextAllocated();
    }
    else if (edit != nullptr && source == &edit->getTransport())
    {
        updateTransportButtons();
    }
}
