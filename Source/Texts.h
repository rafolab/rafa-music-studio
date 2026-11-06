#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

/** Vsetky texty v zdrojakoch su v UTF-8 (slovencina s diakritikou).
    Tato funkcia ich bezpecne prevedie na juce::String. */
inline juce::String sk (const char* utf8Text)
{
    return juce::String::fromUTF8 (utf8Text);
}

namespace RafaPaths
{
    /** Hlavny priecinok aplikacie v Dokumentoch: Dokumenty/RAFA MUSIC STUDIO */
    inline juce::File getRootFolder()
    {
        auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile ("RAFA MUSIC STUDIO");
        f.createDirectory();
        return f;
    }

    /** Priecinok so skladbami: Dokumenty/RAFA MUSIC STUDIO/Skladby */
    inline juce::File getSongsFolder()
    {
        auto f = getRootFolder().getChildFile ("Skladby");
        f.createDirectory();
        return f;
    }
}
