#pragma once

#include "Texts.h"

/** Zoznam stop s vlnovym priebehom nahravok.
    Kazda stopa ma meno, tlacidlo Stlmit a Zmazat.
    Kliknutim do vlnoveho priebehu sa presunie prehravacia hlava. */
class TrackListComponent : public juce::Component
{
public:
    TrackListComponent();
    ~TrackListComponent() override;

    void setEdit (te::Edit* newEdit);

    /** Skontroluje, ci sa zmenili stopy alebo klipy, a ak ano, prestavi zoznam. */
    void refreshIfChanged();

    /** Prekreslenie prehravacej hlavy. */
    void updatePlayhead();

    /** Stopa, do ktorej sa prave nahrava (zobrazi sa cervenou). */
    void setRecordingTrack (te::AudioTrack* t);

    std::function<void (te::AudioTrack&)> onDeleteTrack;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    static constexpr int headerWidth = 210;
    static constexpr int rowHeight   = 92;

    int getIdealHeight() const;

private:
    class Row;

    te::Edit* edit = nullptr;
    juce::OwnedArray<Row> rows;
    juce::String lastSignature;
    te::AudioTrack* recordingTrack = nullptr;
    double lastPlayheadX = -1.0;

    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 64 };
    std::map<juce::String, std::unique_ptr<juce::AudioThumbnail>> thumbnails;

    juce::AudioThumbnail& getThumbnailFor (const juce::File&);
    juce::String buildSignature() const;
    void rebuildRows();

    double getVisibleLengthSeconds() const;
    double xToSeconds (float x) const;
    float secondsToX (double seconds) const;

    friend class Row;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackListComponent)
};
