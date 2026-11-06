#pragma once

#include "Texts.h"

/** Zostavi textovu spravu o pocitaci, zvukovych zariadeniach a MIDI.
    Pouziva sa tlacidlom "Diagnostika" - spravu staci vlozit do chatu. */
namespace Diagnostics
{
    juce::String buildReport (te::Engine& engine, const te::Edit* edit, const juce::File& editFile);

    /** Ulozi spravu do Dokumenty/RAFA MUSIC STUDIO/diagnostika.txt a vrati subor. */
    juce::File saveReport (const juce::String& report);
}
