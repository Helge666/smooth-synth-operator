#pragma once

namespace sso
{

/** Settings shared by all instances (standalone and plugins) of all SSO programmers,
    stored in %APPDATA%/Smooth Synth Operator/SSO.settings (or the platform equivalent).
    Use via juce::SharedResourcePointer<AppSettings>. */
class AppSettings
{
public:
    AppSettings()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "SSO";
        options.folderName = "Smooth Synth Operator";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.millisecondsBeforeSaving = 500;
        file = std::make_unique<juce::PropertiesFile> (options);
        loadedVersion = file->getFile().getLastModificationTime();
    }

    /** The settings, re-read first if another process (standalone app, another host) saved
        the file since - otherwise this process would write its stale copy back over them. */
    juce::PropertiesFile& get()
    {
        const juce::ScopedLock sl (lock);
        auto modified = file->getFile().getLastModificationTime();

        if (modified != loadedVersion && ! file->needsToBeSaved())
        {
            file->reload();
            loadedVersion = modified;
        }

        return *file;
    }

    static juce::File getDataDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Smooth Synth Operator");
    }

private:
    std::unique_ptr<juce::PropertiesFile> file;
    juce::Time loadedVersion;
    juce::CriticalSection lock;
};

} // namespace sso
