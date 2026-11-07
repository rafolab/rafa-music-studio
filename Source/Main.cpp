/*
    RAFA MUSIC STUDIO
    Vlastna nahravacia aplikacia postavena na Tracktion Engine (GPLv3) a JUCE.
*/

#include "MainComponent.h"

//==============================================================================
// Zaznam priebehu (Dokumenty/RAFA MUSIC STUDIO/log.txt) - na hladanie chyb pri starte
static void rafaCrashHandler (void*)
{
    juce::Logger::writeToLog ("!!! PAD PROGRAMU !!!\n" + juce::SystemStats::getStackBacktrace());
}

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
        setResizeLimits (980, 600, 4000, 3000);

        // okno nesmie byt vacsie ako obrazovka (notebooky maju casto mensiu pracovnu plochu)
        int w = getWidth(), h = getHeight();

        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            auto area = display->userArea;
            w = juce::jmin (w, area.getWidth()  - 20);
            h = juce::jmin (h, area.getHeight() - 40);
        }

        centreWithSize (w, h);
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
        logger.reset (new juce::FileLogger (RafaPaths::getRootFolder().getChildFile ("log.txt"),
                                            "RAFA MUSIC STUDIO " + getApplicationVersion()
                                              + " - start " + juce::Time::getCurrentTime().toString (true, true)
                                              + " - " + juce::SystemStats::getOperatingSystemName(),
                                            512 * 1024));
        juce::Logger::setCurrentLogger (logger.get());
        juce::SystemStats::setApplicationCrashHandler (rafaCrashHandler);

        juce::Logger::writeToLog ("1. spustam motor");
        engine = std::make_unique<te::Engine> (getApplicationName());
        juce::Logger::writeToLog ("2. motor bezi, vytvaram okno");
        mainWindow = std::make_unique<MainWindow> (getApplicationName(), *engine);
        juce::Logger::writeToLog ("3. okno je otvorene");
    }

    void anotherInstanceStarted (const juce::String&) override
    {
        // ak uz program bezi, len ho prenesieme dopredu
        juce::Logger::writeToLog ("pokus o druhe spustenie - prenasam okno dopredu");

        if (mainWindow != nullptr)
        {
            mainWindow->setMinimised (false);
            mainWindow->toFront (true);
        }
    }

    void shutdown() override
    {
        juce::Logger::writeToLog ("koniec programu");
        mainWindow = nullptr;   // najprv okno (ulozi skladbu), potom motor
        engine = nullptr;
        juce::Logger::setCurrentLogger (nullptr);
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    std::unique_ptr<juce::FileLogger> logger;
    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (RafaMusicStudioApplication)
