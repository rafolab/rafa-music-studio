#include "Diagnostics.h"

namespace Diagnostics
{

static juce::String msFromSamples (int samples, double sampleRate)
{
    if (sampleRate <= 0.0)
        return "?";

    return juce::String ((double) samples * 1000.0 / sampleRate, 1) + " ms";
}

juce::String buildReport (te::Engine& engine, const te::Edit* edit, const juce::File& editFile)
{
    juce::String r;
    auto nl = "\r\n";

    r << "=== RAFA MUSIC STUDIO - diagnostika ===" << nl
      << "Verzia aplikacie: " << ProjectInfo::versionString << nl
      << "Cas: " << juce::Time::getCurrentTime().toString (true, true) << nl
      << "System: " << juce::SystemStats::getOperatingSystemName()
      << (juce::SystemStats::isOperatingSystem64Bit() ? " (64-bit)" : " (32-bit)") << nl
      << "Procesor: " << juce::SystemStats::getCpuModel()
      << ", jadra: " << juce::SystemStats::getNumPhysicalCpus()
      << " / vlakna: " << juce::SystemStats::getNumCpus() << nl
      << "RAM: " << juce::SystemStats::getMemorySizeInMegabytes() << " MB" << nl << nl;

    auto& dm  = engine.getDeviceManager();
    auto& adm = dm.deviceManager;

    r << "--- Aktualne zvukove zariadenie ---" << nl;
    r << "Typ ovladaca: " << adm.getCurrentAudioDeviceType() << nl;

    if (auto* dev = adm.getCurrentAudioDevice())
    {
        auto sr = dev->getCurrentSampleRate();
        auto setup = adm.getAudioDeviceSetup();

        r << "Zariadenie: " << dev->getName() << nl
          << "Vstupne zariadenie (nastavenie): " << setup.inputDeviceName << nl
          << "Vystupne zariadenie (nastavenie): " << setup.outputDeviceName << nl
          << "Vzorkovacia frekvencia: " << sr << " Hz" << nl
          << "Buffer: " << dev->getCurrentBufferSizeSamples() << " vzoriek ("
          << msFromSamples (dev->getCurrentBufferSizeSamples(), sr) << ")" << nl
          << "Latencia vstupu: " << dev->getInputLatencyInSamples() << " vzoriek ("
          << msFromSamples (dev->getInputLatencyInSamples(), sr) << ")" << nl
          << "Latencia vystupu: " << dev->getOutputLatencyInSamples() << " vzoriek ("
          << msFromSamples (dev->getOutputLatencyInSamples(), sr) << ")" << nl
          << "Kompenzacia nahravky (motor): " << juce::String (dm.getRecordAdjustmentMs(), 1) << " ms" << nl
          << "Aktivne vstupne kanaly: " << dev->getActiveInputChannels().toString (2) << nl
          << "Mena vstupnych kanalov: " << dev->getInputChannelNames().joinIntoString (" | ") << nl
          << "Mena vystupnych kanalov: " << dev->getOutputChannelNames().joinIntoString (" | ") << nl
          << "Dostupne frekvencie: ";

        for (auto rate : dev->getAvailableSampleRates())
            r << rate << " ";

        r << nl << "Dostupne buffre: ";

        for (auto b : dev->getAvailableBufferSizes())
            r << b << " ";

        r << nl;
    }
    else
    {
        r << "!!! Ziadne zvukove zariadenie nie je otvorene." << nl;
    }

    r << "Vytazenie CPU zvukom: " << juce::String (dm.getCpuUsage() * 100.0f, 1) << " %" << nl << nl;

    r << "--- Vsetky typy ovladacov a zariadenia ---" << nl;

    for (auto* type : adm.getAvailableDeviceTypes())
    {
        type->scanForDevices();
        r << "[" << type->getTypeName() << "]" << nl
          << "  vstupy:  " << type->getDeviceNames (true).joinIntoString (" | ") << nl
          << "  vystupy: " << type->getDeviceNames (false).joinIntoString (" | ") << nl;
    }

    r << nl << "--- Vstupy motora (Tracktion) ---" << nl;

    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
        if (auto* w = dm.getWaveInDevice (i))
            r << "  " << w->getName() << (w->isEnabled() ? "  [zapnuty]" : "  [vypnuty]") << nl;

    r << nl << "--- MIDI vstupy ---" << nl;

    auto midiIns = juce::MidiInput::getAvailableDevices();

    if (midiIns.isEmpty())
        r << "  (ziadne)" << nl;

    for (auto& m : midiIns)
        r << "  " << m.name << nl;

    r << nl << "--- Skladba ---" << nl
      << "Subor: " << editFile.getFullPathName() << nl;

    if (edit != nullptr)
    {
        r << "Stopy: " << te::getAudioTracks (*edit).size() << nl
          << "Dlzka: " << juce::String (edit->getLength().inSeconds(), 2) << " s" << nl;
    }

    return r;
}

juce::File saveReport (const juce::String& report)
{
    auto f = RafaPaths::getRootFolder().getChildFile ("diagnostika.txt");
    f.replaceWithText (report, false, false, "\r\n");
    return f;
}

}
