/*
    Etapa 2D: sprava skladieb a export stop.

    - Nova skladba vznika v "Rozpracované" (vlastny jednoznacny priecinok), uklada sa automaticky.
    - "Uložiť ako…" ju presunie (nepomenovanu) alebo skopiruje (pomenovanu) do "Skladby".
      Odkazy na zvuk su relativne k priecinku skladby, presun ich nepokazi.
    - Zahodit / odstranit = presun do kosa Windows (da sa obnovit).
    - Upratanie starych skladieb: len vybrane, s potvrdenim a so zalohou v "Záloha".
*/

#include "MainComponent.h"

static juce::String legalSongName (const juce::String& name)
{
    return juce::File::createLegalFileName (name.trim()).trim().substring (0, 60);
}

bool MainComponent::isDraftSong() const
{
    return editFile != juce::File() && editFile.isAChildOf (RafaPaths::getDraftsFolder());
}

juce::String MainComponent::songDisplayName() const
{
    return isDraftSong() ? sk ("Bez názvu") : editFile.getFileNameWithoutExtension();
}

bool MainComponent::songHasRecordings()
{
    if (edit == nullptr)
        return false;

    for (auto* t : te::getAudioTracks (*edit))
        if (! t->getClips().isEmpty())
            return true;

    return false;
}

void MainComponent::releaseEditFiles()
{
    // Windows nedovoli presunut priecinok, kym su zvukove subory otvorene
    engine.getAudioFileManager().releaseAllFiles();
}

// Prazdnu nepomenovanu skladbu (bez jedinej nahravky) pri odchode zmazeme, aby nevznikali prazdne priecinky.
void MainComponent::cleanupEmptyDraft()
{
    if (edit == nullptr || ! isDraftSong() || songHasRecordings())
        return;

    auto folder = editFile.getParentDirectory();

    if (! folder.findChildFiles (juce::File::findFiles, true, "*.wav").isEmpty())
        return;     // pre istotu: ak je tam zvuk, nechame ho

    closeEdit();
    releaseEditFiles();
    folder.deleteRecursively();
}

//==============================================================================
void MainComponent::refreshSongList()
{
    juce::Array<juce::var> list;

    auto addFolder = [&] (const juce::File& root, bool draft)
    {
        juce::Array<juce::File> found;

        for (auto& dir : root.findChildFiles (juce::File::findDirectories, false))
        {
            auto f = dir.getChildFile (dir.getFileName() + ".tracktionedit");

            if (! f.existsAsFile())
            {
                auto any = dir.findChildFiles (juce::File::findFiles, false, "*.tracktionedit");
                if (any.isEmpty()) continue;
                f = any.getFirst();
            }

            found.add (f);
        }

        std::sort (found.begin(), found.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getLastModificationTime() > b.getLastModificationTime(); });

        for (auto& f : found)
        {
            auto o = std::make_unique<juce::DynamicObject>();
            o->setProperty ("name", draft ? sk ("Bez názvu · ") + f.getLastModificationTime().formatted ("%d.%m. %H:%M")
                                          : f.getFileNameWithoutExtension());
            o->setProperty ("path", f.getFullPathName());
            o->setProperty ("draft", draft);
            o->setProperty ("current", f == editFile);
            o->setProperty ("modified", f.getLastModificationTime().formatted ("%d.%m.%Y %H:%M"));
            list.add (o.release());
        }
    };

    addFolder (RafaPaths::getSongsFolder(), false);
    addFolder (RafaPaths::getDraftsFolder(), true);
    songList = list;
}

void MainComponent::openSongPath (const juce::String& path)
{
    juce::File f (path);

    // len skladby z nasich priecinkov
    if (! f.existsAsFile() || ! f.isAChildOf (RafaPaths::getRootFolder()) || ! f.hasFileExtension ("tracktionedit"))
        return;

    if (edit != nullptr && edit->getTransport().isRecording())
        return;

    if (f != editFile)
        cleanupEmptyDraft();

    loadEdit (f);
    setStatus (sk ("Otvorená skladba „") + songDisplayName() + sk ("“."), RafaColours::play);
}

//==============================================================================
bool MainComponent::saveSongAs (const juce::String& rawName)
{
    if (edit == nullptr || edit->getTransport().isRecording())
        return false;

    auto name = legalSongName (rawName);

    if (name.isEmpty())
    {
        setStatus (sk ("Zadaj názov skladby."), RafaColours::accent);
        return false;
    }

    auto dest = RafaPaths::getSongsFolder().getChildFile (name);

    if (dest.exists())
    {
        setStatus (sk ("Skladba „") + name + sk ("“ už existuje – zvoľ iný názov."), RafaColours::accent);
        return false;
    }

    const bool draft = isDraftSong();
    auto srcFolder = editFile.getParentDirectory();
    auto oldName = editFile.getFileName();

    edit->getTransport().stop (false, false);
    closeEdit();                        // ulozi a zatvori
    releaseEditFiles();

    bool ok = false;

    if (draft)
        ok = srcFolder.moveFileTo (dest);

    if (! ok)                           // pomenovana skladba (alebo presun zlyhal): kopia, original ostava
        ok = srcFolder.copyDirectoryTo (dest);

    if (! ok)
    {
        loadEdit (srcFolder.getChildFile (oldName));
        setStatus (sk ("Skladbu sa nepodarilo uložiť pod novým názvom."), RafaColours::record);
        return false;
    }

    // subor skladby premenujeme podla nazvu (zvuk je odkazovany relativne, ostava platny)
    auto newFile = dest.getChildFile (name + ".tracktionedit");
    dest.getChildFile (oldName).moveFileTo (newFile);

    juce::Logger::writeToLog ("Ulozit ako: " + srcFolder.getFullPathName() + " -> " + newFile.getFullPathName()
                              + (draft && ! srcFolder.exists() ? " (presun)" : " (kopia)"));

    loadEdit (newFile);
    setStatus (sk ("Skladba uložená ako „") + name + sk ("“."), RafaColours::play);
    refreshSongList();
    return true;
}

void MainComponent::discardOrDeleteSong (bool onlyDraft, const juce::String& thenOpenPath)
{
    if (edit == nullptr || edit->getTransport().isRecording())
        return;

    if (onlyDraft && ! isDraftSong())
        return;

    auto folder = editFile.getParentDirectory();

    // bezpecnost: len priecinok skladby vnutri nasich priecinkov
    if (! (folder.isAChildOf (RafaPaths::getDraftsFolder()) || folder.isAChildOf (RafaPaths::getSongsFolder())))
        return;

    auto name = songDisplayName();
    closeEdit();
    releaseEditFiles();

    const bool trashed = folder.moveToTrash();
    juce::Logger::writeToLog ("Do kosa: " + folder.getFullPathName() + (trashed ? " OK" : " ZLYHALO"));

    juce::File next (thenOpenPath);

    if (thenOpenPath.isNotEmpty() && next.existsAsFile() && next.isAChildOf (RafaPaths::getRootFolder()))
        loadEdit (next);
    else
        createNewSong();

    if (trashed)
        setStatus (sk ("Skladba „") + name + sk ("“ je v koši Windows."), RafaColours::play);
    else
        setStatus (sk ("Skladbu sa nepodarilo presunúť do koša – ostala v priečinku."), RafaColours::accent);

    refreshSongList();
}

void MainComponent::moveSongsToDrafts (const juce::var& paths)
{
    if (! paths.isArray())
        return;

    auto backupRoot = RafaPaths::getBackupFolder().getChildFile (juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S"));
    int moved = 0, failed = 0;

    for (auto& p : *paths.getArray())
    {
        juce::File f (p.toString());
        auto folder = f.getParentDirectory();

        if (! f.existsAsFile() || ! folder.isAChildOf (RafaPaths::getSongsFolder()) || f == editFile)
        {
            ++failed;
            continue;
        }

        // najprv zaloha, az potom presun
        auto backup = backupRoot.getChildFile (folder.getFileName());

        if (! folder.copyDirectoryTo (backup))
        {
            ++failed;
            continue;
        }

        auto dest = RafaPaths::getDraftsFolder().getNonexistentChildFile (folder.getFileName(), {}, false);

        if (folder.moveFileTo (dest))
            ++moved;
        else
            ++failed;
    }

    juce::Logger::writeToLog ("Upratanie skladieb: presunute " + juce::String (moved) + ", chyby " + juce::String (failed)
                              + ", zaloha " + backupRoot.getFullPathName());

    setStatus (sk ("Presunuté do Rozpracované: ") + juce::String (moved)
                 + (failed > 0 ? sk (", nepresunuté: ") + juce::String (failed) : juce::String())
                 + sk (". Záloha je v priečinku Záloha."), failed > 0 ? RafaColours::accent : RafaColours::play);
    refreshSongList();
}

//==============================================================================
// Export stop pre Cakewalk Sonar: surove WAV, vsetky od 0 s a rovnako dlhe
//==============================================================================
void MainComponent::exportTracks()
{
    if (edit == nullptr || edit->getTransport().isRecording())
        return;

    auto tracks = te::getAudioTracks (*edit);
    auto length = edit->getLength();

    if (tracks.isEmpty() || length.inSeconds() <= 0.0)
    {
        setStatus (sk ("Nie je čo exportovať – najprv niečo nahraj."), RafaColours::accent);
        return;
    }

    stopTransport();
    saveEdit();

    auto folder = editFile.getParentDirectory().getChildFile ("Export")
                      .getNonexistentChildFile (songDisplayName() + sk (" - stopy"), {}, false);
    folder.createDirectory();

    // M/S nesmu ovplyvnit export - docasne ich vypneme a potom vratime
    juce::Array<bool> mute, solo;

    for (auto* t : tracks)
    {
        mute.add (t->isMuted (false));
        solo.add (t->isSolo (false));
        t->setMute (false);
        t->setSolo (false);
    }

    auto allTracks = te::getAllTracks (*edit);
    int ok = 0;

    for (int i = 0; i < tracks.size(); ++i)
    {
        auto* t = tracks[i];

        if (t->getClips().isEmpty())
            continue;

        juce::BigInteger bits;
        bits.setBit (allTracks.indexOf (t));

        auto file = folder.getChildFile (juce::String (i + 1).paddedLeft ('0', 2) + " "
                                         + legalSongName (t->getName()) + ".wav");

        // usePlugins = false: bez fadera, panoramy a hlavnej hlasitosti (surove audio)
        if (te::Renderer::renderToFile (sk ("Export stopy"), file, *edit,
                                        te::TimeRange (te::TimePosition(), length),
                                        bits, false, false, {}, false))
            ++ok;
    }

    for (int i = 0; i < tracks.size(); ++i)
    {
        tracks[i]->setMute (mute[i]);
        tracks[i]->setSolo (solo[i]);
    }

    lastExportFolder = folder;

    if (ok > 0)
    {
        setStatus (sk ("Exportované stopy: ") + juce::String (ok) + sk (" (") + juce::String (length.inSeconds(), 1)
                     + sk (" s, od začiatku) – priečinok ") + folder.getFileName(), RafaColours::play);
        folder.revealToUser();
    }
    else
    {
        setStatus (sk ("Export stôp sa nepodaril."), RafaColours::record);
    }
}

void MainComponent::openExportFolder()
{
    auto folder = editFile.getParentDirectory().getChildFile ("Export");
    folder.createDirectory();

    if (lastExportFolder.isDirectory() && lastExportFolder.isAChildOf (folder))
        lastExportFolder.revealToUser();
    else
        folder.startAsProcess();
}

//==============================================================================
// Zatvorenie programu: nepomenovana skladba s nahravkami -> Ulozit / Zahodit / Zrusit
//==============================================================================
void MainComponent::requestQuit (std::function<void()> doQuit)
{
    if (edit == nullptr || ! isDraftSong() || ! songHasRecordings())
    {
        doQuit();
        return;
    }

    if (edit->getTransport().isRecording())
        stopTransport();

    auto* aw = new juce::AlertWindow (sk ("Skladba nie je pomenovaná"),
                                      sk ("Chceš ju uložiť pod názvom? (Inak ostane v Rozpracované a nájdeš ju pri ďalšom spustení.)"),
                                      juce::MessageBoxIconType::QuestionIcon);
    aw->addTextEditor ("name", {}, sk ("Názov skladby:"));
    aw->addButton (sk ("Uložiť"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton (sk ("Zahodiť"), 2);
    aw->addButton (sk ("Zrušiť"), 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<MainComponent> safeThis (this);

    aw->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, aw, doQuit] (int result)
    {
        auto name = aw->getTextEditorContents ("name");

        if (safeThis == nullptr || result == 0)
            return;

        if (result == 1)
        {
            if (safeThis->saveSongAs (name))
                doQuit();
            return;     // zly nazov - program ostava otvoreny, hlasenie je v stavovom riadku
        }

        safeThis->discardOrDeleteSong (true, {});
        doQuit();
    }), true);
}
