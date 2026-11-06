/*
    RAFA MUSIC STUDIO
    Vlastna nahravacia aplikacia postavena na Tracktion Engine (GPLv3) a JUCE.
*/

#include "MainComponent.h"

//==============================================================================
class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (const juce::String& name, te::Engine& engine)
        : DocumentWindow (name, RafaColours::background, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MainComponent (engine), true);
        setResizable (true, true);
        setResizeLimits (980, 640, 4000, 3000);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

//==============================================================================
class RafaMusicStudioApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "RAFA MUSIC STUDIO"; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return false; }

    void initialise (const juce::String&) override
    {
        engine = std::make_unique<te::Engine> (getApplicationName());
        mainWindow = std::make_unique<MainWindow> (getApplicationName(), *engine);
    }

    void shutdown() override
    {
        mainWindow = nullptr;   // najprv okno (ulozi skladbu), potom motor
        engine = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (RafaMusicStudioApplication)
