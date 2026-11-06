#include "TrackListComponent.h"
#include "RafaLookAndFeel.h"

//==============================================================================
class TrackListComponent::Row : public juce::Component
{
public:
    Row (TrackListComponent& o, te::AudioTrack& t)
        : owner (o), track (&t)
    {
        nameLabel.setText (t.getName(), juce::dontSendNotification);
        nameLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        nameLabel.setEditable (false, true);   // dvojklik = premenovat
        nameLabel.onTextChange = [this]
        {
            if (track != nullptr)
                track->setName (nameLabel.getText());
        };
        addAndMakeVisible (nameLabel);

        muteButton.setButtonText (sk ("Stlmiť"));
        muteButton.setToggleState (t.isMuted (false), juce::dontSendNotification);
        muteButton.onClick = [this]
        {
            if (track != nullptr)
                track->setMute (muteButton.getToggleState());
            repaint();
        };
        addAndMakeVisible (muteButton);

        deleteButton.setButtonText (sk ("Zmazať"));
        deleteButton.onClick = [this]
        {
            if (track != nullptr && owner.onDeleteTrack)
                owner.onDeleteTrack (*track);
        };
        addAndMakeVisible (deleteButton);
    }

    void resized() override
    {
        auto r = getLocalBounds().removeFromLeft (headerWidth).reduced (10, 8);
        nameLabel.setBounds (r.removeFromTop (28));
        r.removeFromTop (6);
        auto buttons = r.removeFromTop (30);
        muteButton.setBounds (buttons.removeFromLeft (90));
        buttons.removeFromLeft (8);
        deleteButton.setBounds (buttons.removeFromLeft (84));
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        auto header = bounds.removeFromLeft (headerWidth);
        auto lane = bounds.reduced (0, 6);

        const bool isRecording = (track.get() == owner.recordingTrack);
        const bool muted = track != nullptr && track->isMuted (false);

        g.setColour (RafaColours::panel);
        g.fillRect (header);

        if (isRecording)
        {
            g.setColour (RafaColours::record);
            g.fillRect (header.removeFromLeft (5));
        }

        g.setColour (RafaColours::panel.darker (0.25f));
        g.fillRect (lane);

        if (track == nullptr)
            return;

        for (auto* clip : track->getClips())
        {
            auto* wave = dynamic_cast<te::WaveAudioClip*> (clip);

            if (wave == nullptr)
                continue;

            auto pos = wave->getPosition();
            auto x1 = owner.secondsToX (pos.getStart().inSeconds()) - (float) headerWidth;
            auto x2 = owner.secondsToX (pos.getEnd().inSeconds())   - (float) headerWidth;

            juce::Rectangle<float> clipArea ((float) lane.getX() + x1, (float) lane.getY(),
                                             juce::jmax (2.0f, x2 - x1), (float) lane.getHeight());

            auto base = muted ? RafaColours::textDim : RafaColours::waveform;

            g.setColour (base.withAlpha (0.18f));
            g.fillRoundedRectangle (clipArea, 6.0f);
            g.setColour (base.withAlpha (0.6f));
            g.drawRoundedRectangle (clipArea, 6.0f, 1.0f);

            auto file = wave->getOriginalFile();

            if (file.existsAsFile())
            {
                auto& thumb = owner.getThumbnailFor (file);
                auto offset = pos.getOffset().inSeconds();

                g.setColour (base);
                thumb.drawChannels (g, clipArea.reduced (2.0f, 4.0f).toNearestInt(),
                                    offset, offset + pos.getLength().inSeconds(), 1.0f);
            }

            g.setColour (RafaColours::text.withAlpha (0.8f));
            g.setFont (juce::FontOptions (12.0f));
            g.drawText (wave->getName(), clipArea.reduced (6.0f, 3.0f).toNearestInt(),
                        juce::Justification::topLeft, true);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x <= headerWidth || owner.edit == nullptr)
            return;

        auto seconds = owner.xToSeconds ((float) e.x);
        owner.edit->getTransport().setPosition (te::TimePosition::fromSeconds (juce::jmax (0.0, seconds)));
    }

    TrackListComponent& owner;
    juce::ReferenceCountedObjectPtr<te::AudioTrack> track;
    juce::Label nameLabel;
    juce::ToggleButton muteButton;
    juce::TextButton deleteButton;
};

//==============================================================================
TrackListComponent::TrackListComponent()
{
    formatManager.registerBasicFormats();
}

TrackListComponent::~TrackListComponent()
{
    rows.clear();
}

void TrackListComponent::setEdit (te::Edit* newEdit)
{
    edit = newEdit;
    recordingTrack = nullptr;
    lastSignature = {};
    thumbnails.clear();
    rebuildRows();
}

juce::String TrackListComponent::buildSignature() const
{
    if (edit == nullptr)
        return {};

    juce::String s;

    for (auto* t : te::getAudioTracks (*edit))
    {
        s << t->itemID.toString() << ":" << t->getName() << "[";

        for (auto* c : t->getClips())
        {
            auto p = c->getPosition();
            s << c->itemID.toString() << "@" << juce::String (p.getStart().inSeconds(), 3)
              << "+" << juce::String (p.getLength().inSeconds(), 3) << ",";
        }

        s << "]";
    }

    return s;
}

void TrackListComponent::refreshIfChanged()
{
    auto sig = buildSignature();

    if (sig != lastSignature)
    {
        lastSignature = sig;
        rebuildRows();
    }
}

void TrackListComponent::rebuildRows()
{
    rows.clear();

    if (edit != nullptr)
    {
        for (auto* t : te::getAudioTracks (*edit))
        {
            auto* row = rows.add (new Row (*this, *t));
            addAndMakeVisible (row);
        }
    }

    setSize (getWidth(), getIdealHeight());
    resized();
    repaint();
}

int TrackListComponent::getIdealHeight() const
{
    return juce::jmax (rowHeight, rows.size() * rowHeight);
}

void TrackListComponent::setRecordingTrack (te::AudioTrack* t)
{
    if (recordingTrack != t)
    {
        recordingTrack = t;

        for (auto* r : rows)
            r->repaint();
    }
}

void TrackListComponent::resized()
{
    int y = 0;

    for (auto* r : rows)
    {
        r->setBounds (0, y, getWidth(), rowHeight - 4);
        y += rowHeight;
    }
}

void TrackListComponent::paint (juce::Graphics& g)
{
    g.fillAll (RafaColours::background);

    if (rows.isEmpty())
    {
        g.setColour (RafaColours::textDim);
        g.setFont (juce::FontOptions (16.0f));
        g.drawText (sk ("Zatiaľ žiadna nahrávka. Vyber vstup a stlač NAHRÁVAŤ."),
                    getLocalBounds(), juce::Justification::centred);
    }
}

void TrackListComponent::paintOverChildren (juce::Graphics& g)
{
    if (edit == nullptr || rows.isEmpty())
        return;

    auto x = secondsToX (edit->getTransport().getPosition().inSeconds());

    g.setColour (RafaColours::accent);
    g.fillRect (x - 1.0f, 0.0f, 2.0f, (float) getHeight());
}

void TrackListComponent::updatePlayhead()
{
    if (edit == nullptr)
        return;

    auto x = (double) secondsToX (edit->getTransport().getPosition().inSeconds());

    if (std::abs (x - lastPlayheadX) >= 1.0)
    {
        // prekreslime len uzky pas okolo starej a novej pozicie
        auto h = getHeight();
        repaint ((int) lastPlayheadX - 3, 0, 6, h);
        repaint ((int) x - 3, 0, 6, h);
        lastPlayheadX = x;
    }

    // pocas nahravania rastie vlnovy priebeh, a nove nahravky sa nacitavaju postupne
    bool needsRepaint = edit->getTransport().isRecording();

    for (auto& t : thumbnails)
        if (! t.second->isFullyLoaded())
            needsRepaint = true;

    if (needsRepaint)
        for (auto* r : rows)
            r->repaint();
}

double TrackListComponent::getVisibleLengthSeconds() const
{
    double len = 30.0;

    if (edit != nullptr)
    {
        len = juce::jmax (len, edit->getLength().inSeconds() + 5.0);
        len = juce::jmax (len, edit->getTransport().getPosition().inSeconds() + 5.0);
    }

    return len;
}

double TrackListComponent::xToSeconds (float x) const
{
    auto laneWidth = juce::jmax (1, getWidth() - headerWidth);
    return (double) (x - (float) headerWidth) / (double) laneWidth * getVisibleLengthSeconds();
}

float TrackListComponent::secondsToX (double seconds) const
{
    auto laneWidth = juce::jmax (1, getWidth() - headerWidth);
    return (float) headerWidth + (float) (seconds / getVisibleLengthSeconds() * (double) laneWidth);
}

juce::AudioThumbnail& TrackListComponent::getThumbnailFor (const juce::File& file)
{
    auto key = file.getFullPathName();
    auto it = thumbnails.find (key);

    if (it == thumbnails.end())
    {
        auto thumb = std::make_unique<juce::AudioThumbnail> (512, formatManager, thumbnailCache);
        thumb->setSource (new juce::FileInputSource (file));
        it = thumbnails.emplace (key, std::move (thumb)).first;
    }

    return *it->second;
}
