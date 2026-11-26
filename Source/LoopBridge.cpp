#include "LoopBridge.h"

//==============================================================================
void LoopBridgePlugin::arm (juce::AudioBuffer<float>&& audio, double sampleRate, te::TimePosition loopStart)
{
    armed.store (false);              // zvukove vlakno prestane citat stare data
    waitForAudioThread();             // a dockame, kym dociita prave beziaci blok
    data = std::move (audio);
    dataRate = sampleRate;
    startSec = loopStart.inSeconds();
    bridgedSamples.store (0);
    result.store ((int) Result::waiting);
    armed.store (true, std::memory_order_release);
}

void LoopBridgePlugin::waitForAudioThread()
{
    // hlavne vlakno: najviac ~100 ms (jeden blok trva par ms)
    for (int i = 0; i < 1000 && inCallback.load() > 0; ++i)
        juce::Thread::sleep (0), juce::Thread::yield();

    for (int i = 0; i < 100 && inCallback.load() > 0; ++i)
        juce::Thread::sleep (1);
}

// Zvukove vlakno: ziadna alokacia, ziadny zamok, ziadny disk.
void LoopBridgePlugin::applyToBuffer (const te::PluginRenderContext& rc)
{
    struct Guard { std::atomic<int>& c; Guard (std::atomic<int>& x) : c (x) { c.fetch_add (1); } ~Guard() { c.fetch_sub (1); } } guard (inCallback);

    if (! armed.load() || rc.destBuffer == nullptr || ! rc.isPlaying)
        return;

    const auto state = (Result) result.load (std::memory_order_relaxed);

    if (state != Result::waiting && state != Result::playing)
        return;

    const int n = rc.bufferNumSamples;
    const int dataLen = data.getNumSamples();

    if (n <= 0 || dataLen <= 0 || dataRate <= 0.0)
        return;

    const double t0 = rc.editTime.getStart().inSeconds();
    const double t1 = rc.editTime.getEnd().inSeconds();
    const double windowEnd = startSec + dataLen / dataRate;

    if (t0 >= windowEnd && state == Result::playing)
    {
        result.store ((int) Result::timedOut);   // okno premostenia uplynulo
        return;
    }

    if (t1 <= startSec || t0 >= windowEnd)
        return;

    auto& buf = *rc.destBuffer;
    const int chans = juce::jmin (buf.getNumChannels(), juce::jmax (outChannels, data.getNumChannels()));
    const int dataChans = data.getNumChannels();
    const double dt = (t1 - t0) / n;

    // 1) Je uz v stope skutocny klip? Vstup X (klip + pripadny odposluch) porovname so zaciatkom
    //    slucky B: ak X obsahuje B, tak energia (X - B) je vyrazne mensia ako energia X.
    double eX = 0.0, eD = 0.0, eB = 0.0, xb = 0.0;
    int count = 0;

    for (int i = 0; i < n; ++i)
    {
        const auto idx = (juce::int64) std::llround ((t0 + i * dt - startSec) * dataRate);

        if (idx < 0 || idx >= dataLen)
            continue;

        ++count;

        for (int c = 0; c < chans; ++c)
        {
            const float b = data.getSample (juce::jmin (c, dataChans - 1), (int) idx);
            const float x = buf.getSample (c, rc.bufferStartSample + i);
            eX += (double) x * x;
            eD += (double) (x - b) * (x - b);
            eB += (double) b * b;
            xb += (double) x * b;
        }
    }

    if (count == 0)
        return;

    const bool bridgeAudible = eB > 1.0e-7 * count * chans;    // nad ~ -70 dBFS

    // Klip hra, ak vstup obsahuje B: bud je rozdiel vyrazne slabsi ako vstup, alebo (pri odposluchu
    // a hrani cez zaciatok slucky) vstup s B silno koreluje - ziva gitara s B presne v case nekoreluje.
    const bool clipPresent = eD < 0.9 * eX
                              || (xb > 0.6 * eB && xb > 0.25 * std::sqrt (eX * eB));

    if (bridgeAudible && clipPresent)
    {
        // Klip uz hra - premostenie konci, nic sa nezdvoji. V bloku, kde sa novy graf prepol,
        // zvykne klip zacat az o 1-2 vzorky neskor (presne nuly) - tie doplnime z pamate.
        for (int i = 0; i < n; ++i)
        {
            const auto idx = (juce::int64) std::llround ((t0 + i * dt - startSec) * dataRate);

            if (idx < 0 || idx >= dataLen)
                continue;

            for (int c = 0; c < chans; ++c)
                if (buf.getSample (c, rc.bufferStartSample + i) == 0.0f)
                    buf.setSample (c, rc.bufferStartSample + i, data.getSample (juce::jmin (c, dataChans - 1), (int) idx));
        }

        result.store ((int) Result::handedOver);
        return;
    }

    // 2) Klip este nehra: zahrame zaciatok slucky z pamate
    for (int i = 0; i < n; ++i)
    {
        const auto idx = (juce::int64) std::llround ((t0 + i * dt - startSec) * dataRate);

        if (idx < 0 || idx >= dataLen)
            continue;

        for (int c = 0; c < chans; ++c)
            buf.addSample (c, rc.bufferStartSample + i, data.getSample (juce::jmin (c, dataChans - 1), (int) idx));
    }

    bridgedSamples.fetch_add (count, std::memory_order_relaxed);
    result.store ((int) Result::playing, std::memory_order_relaxed);
}

//==============================================================================
bool GrowingWavReader::read (const juce::File& f, juce::int64 startFrame, int numFrames,
                             juce::AudioBuffer<float>& out, double& sampleRate, juce::int64& timeReference,
                             juce::String& error)
{
    juce::FileInputStream in (f);

    if (! in.openedOk())
    {
        error = "subor nahravky sa neda otvorit";
        return false;
    }

    char id[5] = {};

    if (in.read (id, 4) != 4 || juce::String (id) != "RIFF")
    {
        error = "nie je to WAV";
        return false;
    }

    in.readInt();
    in.read (id, 4);

    if (juce::String (id) != "WAVE")
    {
        error = "nie je to WAV";
        return false;
    }

    int formatTag = 0, channels = 0, bits = 0;
    juce::int64 dataStart = -1;
    timeReference = -1;
    sampleRate = 0.0;

    while (! in.isExhausted())
    {
        if (in.read (id, 4) != 4)
            break;

        const auto size = (juce::int64) (juce::uint32) in.readInt();
        const auto chunkStart = in.getPosition();
        const juce::String cid (id, 4);

        if (cid == "fmt ")
        {
            formatTag = (juce::uint16) in.readShort();
            channels = (juce::uint16) in.readShort();
            sampleRate = (double) (juce::uint32) in.readInt();
            in.readInt();     // byteRate
            in.readShort();   // blockAlign
            bits = (juce::uint16) in.readShort();

            if (formatTag == 0xFFFE && size >= 40)   // WAVE_FORMAT_EXTENSIBLE: skutocny format v SubFormat
            {
                in.setPosition (chunkStart + 24);
                formatTag = (juce::uint16) in.readShort();
            }
        }
        else if (cid == "bext" && size >= 346)
        {
            in.setPosition (chunkStart + 338);
            const auto lo = (juce::uint32) in.readInt();
            const auto hi = (juce::uint32) in.readInt();
            timeReference = (juce::int64) (((juce::uint64) hi << 32) | lo);
        }
        else if (cid == "data")
        {
            dataStart = chunkStart;   // dlzka moze byt este neplatna - nahravka rastie
            break;
        }

        in.setPosition (chunkStart + size + (size & 1));
    }

    if (dataStart < 0 || channels <= 0 || sampleRate <= 0.0 || ! (formatTag == 1 || formatTag == 3))
    {
        error = "neznamy format nahravky";
        return false;
    }

    const int bytesPerSample = bits / 8;

    if (! ((formatTag == 1 && (bits == 16 || bits == 24 || bits == 32)) || (formatTag == 3 && bits == 32)))
    {
        error = "nepodporovana bitova hlbka";
        return false;
    }

    const int frameBytes = bytesPerSample * channels;
    const auto available = (f.getSize() - dataStart) / frameBytes;

    if (startFrame < 0 || startFrame + numFrames > available)
    {
        error = "zaciatok slucky este nie je na disku";
        return false;
    }

    juce::HeapBlock<char> raw ((size_t) numFrames * (size_t) frameBytes);
    in.setPosition (dataStart + startFrame * frameBytes);

    if (in.read (raw.get(), numFrames * frameBytes) != numFrames * frameBytes)
    {
        error = "citanie nahravky zlyhalo";
        return false;
    }

    out.setSize (channels, numFrames);
    auto* p = reinterpret_cast<const juce::uint8*> (raw.get());

    for (int i = 0; i < numFrames; ++i)
    {
        for (int c = 0; c < channels; ++c)
        {
            float v = 0.0f;

            if (formatTag == 3)
            {
                float fl;
                std::memcpy (&fl, p, 4);
                v = fl;
            }
            else if (bits == 16)
                v = (float) (juce::int16) (p[0] | (p[1] << 8)) / 32768.0f;
            else if (bits == 24)
                v = (float) ((juce::int32) ((juce::uint32) p[0] << 8 | (juce::uint32) p[1] << 16 | (juce::uint32) p[2] << 24) >> 8) / 8388608.0f;
            else
                v = (float) ((double) (juce::int32) (p[0] | (p[1] << 8) | (p[2] << 16) | ((juce::uint32) p[3] << 24)) / 2147483648.0);

            out.setSample (c, i, v);
            p += bytesPerSample;
        }
    }

    return true;
}
