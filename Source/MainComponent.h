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
