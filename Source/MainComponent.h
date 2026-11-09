#pragma once

#include "Texts.h"
#include "RafaLookAndFeel.h"
#include "TrackListComponent.h"

//==============================================================================
/** Jednoduchy merac urovne vstupu (aby bolo vidiet, ze gitara "ide" do pocitaca). */
class InputMeter : public juce::Component
{
public:
    void setLevel (float linearLevel);
    void setLevelDb (float db);
    void paint (juce::Graphics&) override;

private:
    float levelDb = -100.0f, peakDb = -100.0f;
    int peakHoldFrames = 0;
};

//==============================================================================
/** Strazca vypadkov zvuku: druhy (tichy) odberatel zvukoveho zariadenia meria casy medzi
    volaniami. Ked pride blok neskoro (viac ako ~2,5x dlzka bufferu), zvukova karta mohla
    hrat ticho = praskanie / vypadok. Pri ASIO a DirectSound sa pocitaju aj hlasenia ovladaca.
    Na zvuk nema vplyv (svoj vystup len vynuluje). */
class AudioWatch : public juce::AudioIODeviceCallback
{
public:
    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        for (int c = 0; c < numOut; ++c)
            if (out[c] != nullptr)
                juce::FloatVectorOperations::clear (out[c], numSamples);

        const auto now = juce::Time::getHighResolutionTicks();

        if (lastTicks != 0 && expectedMs > 0.0)
        {
            const double ms = juce::Time::highResolutionTicksToSeconds (now - lastTicks) * 1000.0;

            if (ms > juce::jmax (expectedMs * 2.5, expectedMs + 8.0))
                lateBlocks.fetch_add (1);

            if (ms > worstGapMs.load())
                worstGapMs.store (ms);
        }

        lastTicks = now;
    }

    void audioDeviceAboutToStart (juce::AudioIODevice* d) override
    {
        lastTicks = 0;
        expectedMs = d != nullptr && d->getCurrentSampleRate() > 0
                       ? d->getCurrentBufferSizeSamples() * 1000.0 / d->getCurrentSampleRate() : 0.0;
    }

    void audioDeviceStopped() override   { lastTicks = 0; }

    std::atomic<int> lateBlocks { 0 };
    std::atomic<double> worstGapMs { 0.0 };

private:
    juce::int64 lastTicks = 0;
    double expectedMs = 0.0;
};

//==============================================================================
/** Hlavna obrazovka RAFA MUSIC STUDIO - Etapa 1 (skuska zvuku). */
class MainComponent : public juce::Component,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    explicit MainComponent (te::Engine&);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    te::Engine& engine;
    std::unique_ptr<te::Edit> edit;
    juce::File editFile;
    std::unique_ptr<juce::PropertiesFile> settings;
    RafaLookAndFeel lookAndFeel;

    // hlavicka
    juce::Label titleLabel, subtitleLabel, songLabel;
    juce::TextButton newSongButton, openSongButton, folderButton,
                     audioSettingsButton, diagnosticsButton, exportButton;

    // zvukove zariadenie
    juce::Label deviceTitleLabel, deviceInfoLabel, inputTitleLabel;
    juce::ComboBox inputCombo;
    juce::StringArray inputDeviceIDs;
    juce::ToggleButton monitorToggle;
    InputMeter inputMeter;
    float inputMeterDb = -100.0f;

    // merac meria len vybrany vstup (kanal) pre gitaru
    te::LevelMeasurer::Client meterClient;
    te::WaveInputDevice* meteredDevice = nullptr;
    void attachMeterToSelectedInput();
    void detachMeter();

    // transport
    juce::TextButton recordButton, playButton, stopButton, rewindButton;
    juce::Label timeLabel, statusLabel;

    // stopy
    juce::Viewport trackViewport;
    TrackListComponent trackList;

    te::AudioTrack* currentRecordingTrack = nullptr;

    // kontrola, ci po nahravani naozaj vznikol klip so zvukovym suborom
    te::EditItemID recordingTrackID;
    int clipsBeforeRecording = 0;
    bool pendingRecordingCheck = false;
    juce::uint32 recordingCheckDeadline = 0;
    void checkFinishedRecording();
    int timerTicks = 0;

    // --- vypadky zvuku (diagnostika praskania) ---
    AudioWatch audioWatch;
    int dropouts = 0, lastLate = 0, lastXruns = -1;
    float cpuPeak = 0.0f;
    juce::StringArray dropoutLog;
    void checkAudioDropouts();
    void resetAudioDropouts();
    void setBufferSize (int samples);
    void setRecordOffsetMs (double ms);
    double getRecordOffsetMs() const;

    // --- skladby ---
    void createNewSong();
    void openSong();
    void loadEdit (const juce::File&);
    void saveEdit();
    void closeEdit();

    // --- vstupy a zariadenia ---
    void ensureMonoInputs();
    void rememberOrRestorePreferredInput (bool userChange);   // zapamata si BEAM a po zapojeni ho znova zvoli
    void refreshInputList();
    juce::String getSelectedInputID() const;
    void applyMonitoring();
    void updateDeviceInfo();
    void showAudioSettings();
    void runDiagnostics();
    void exportMix();

    // --- transport ---
    void startRecording();
    void togglePlay();
    void stopTransport();
    void rewind();
    te::AudioTrack* prepareTrackForRecording();
    void deleteTrack (te::AudioTrack&);
    void updateTransportButtons();

    void setStatus (const juce::String&, juce::Colour = RafaColours::textDim);
    juce::String statusLevel { "info" };

    // --- tempo, metronom, LOOP (API v2) - implementacia v TempoLoop.cpp ---
    bool loopRecordPass = false;        // nahrava sa jeden prechod slucky
    double lastLoopPosition = -1.0e9;   // na zistenie, ze slucka sa pretocila
    bool loopWrapped = false;
    void initTempoDefaults();
    void setTempoBpm (double bpm);
    void setTimeSignature (int numerator, int denominator);
    void setMetronome (bool on);
    void setMetronomeVolume (double volume);
    void setCountInBars (int bars);
    void setLoopOn (bool on);
    void setLoopRangeSeconds (double start, double end);
    void setLoopBars (int bars);
    void setLoopPointAtPlayhead (bool isStart);
    void finishLoopRecordingPass();
    void checkLoopRecordingPass();
    bool flattenLoopTakes (te::WaveAudioClip&);
    double snapToNearestBeat (double seconds) const;
    double getSecondsPerBar() const;
    juce::var buildTempoLoopState();

    // --- mix vrstiev (API v3) - implementacia v Mixer.cpp ---
    struct TrackMeter
    {
        te::LevelMeasurer* measurer = nullptr;
        std::unique_ptr<te::LevelMeasurer::Client> client;
        float levelDb = -100.0f;
    };
    std::map<juce::String, TrackMeter> trackMeters;   // podla id stopy
    void syncTrackMeters();
    void detachAllTrackMeters();
    void setTrackVolumeDb (const juce::String& trackId, double db);
    void setTrackPan (const juce::String& trackId, double pan);
    void setTrackSolo (const juce::String& trackId, bool solo);
    void setMasterVolumeDb (double db);
    juce::var buildMixerState();

    // --- API v4: konzola Model 5 - implementacia v Console.cpp ---
    te::LevelMeasurer::Client outMeterClient;          // skutocny vystup L/R (VU metre)
    te::WaveOutputDevice* meteredOut = nullptr;
    float outLevelL = -100.0f, outLevelR = -100.0f;
    juce::String selectedTrackId;                      // vybrana vrstva (tlacidla 1-4, DATA)
    float inputGainDb = 0.0f;                          // gombik INPUT
    void syncOutputMeter();
    void requestSave();                                // ulozenie o chvilu (fadery/gombiky posielaju vela zmien)
    juce::uint32 saveDueMs = 0;
    juce::uint32 recordingStartedMs = 0;

    // --- Etapa 2D: historia pokusov (UNDO / REDO) - History.cpp ---
    struct HistoryEntry
    {
        enum Type { take, deleteTrack } type = take;
        juce::String label;
        juce::String trackId;
        juce::ValueTree clipState;          // take: stav nahraneho klipu
        bool trackCreatedByTake = false;    // take: stopa vznikla az pri nahravani
        juce::ValueTree trackState;         // stav stopy (pri odstraneni / vrateni)
        int trackIndex = -1;                // poradie stopy medzi audio stopami
    };
    std::vector<HistoryEntry> undoStack, redoStack;
    bool recordTrackWasCreated = false;
    void clearHistory();
    void pushHistory (HistoryEntry);
    bool undoLast();
    bool redoLast();
    void deleteTrackWithHistory (te::AudioTrack&);
    void removeTrackSafely (te::AudioTrack&);
    te::AudioTrack* insertTrackFromState (const juce::ValueTree&, int index);
    juce::var buildHistoryState();

    // --- Etapa 2D: projekty - Projects.cpp ---
    juce::var songList;                 // zoznam skladieb pre obrazovku (na poziadanie)
    bool isDraftSong() const;
    bool songHasRecordings();
    juce::String songDisplayName() const;
    void refreshSongList();
    bool saveSongAs (const juce::String& name);
    void discardOrDeleteSong (bool onlyDraft, const juce::String& thenOpenPath);
    void openSongPath (const juce::String& path);
    void moveSongsToDrafts (const juce::var& paths);
    void exportTracks();
    void openExportFolder();
    void releaseEditFiles();
    void cleanupEmptyDraft();
    juce::File lastExportFolder;
public:
    /** Pri zatvarani programu: ak je nepomenovana skladba s nahravkami, opyta sa Ulozit / Zahodit / Zrusit. */
    void requestQuit (std::function<void()> doQuit);
private:
    void detachOutputMeter();
    void applyInputGain();
    void selectTrack (const juce::String& trackId);
    void addEmptyTrack();
    juce::String getSelectedTrackId();

    // --- obrazovka v HTML (ui/) - implementacia vo WebBridge.cpp ---
    std::unique_ptr<juce::WebBrowserComponent> webView;
    bool webUIActive = false;
    juce::String lastTracksSignature;
    std::map<juce::String, juce::Array<juce::var>> peaksCache;

    void setupWebUI();
    void handleWebCommand (const juce::var& message);
    void sendStateToWeb();
    void sendTracksToWebIfChanged (bool force);
    juce::var buildStateObject();
    juce::var buildTracksObject();
    juce::Array<juce::var> getPeaksFor (te::WaveAudioClip&);
    te::AudioTrack* findTrackById (const juce::String&);
    std::optional<juce::WebBrowserComponent::Resource> getWebResource (const juce::String& url);

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
