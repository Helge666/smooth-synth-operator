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
    }

    juce::PropertiesFile& get() noexcept { return *file; }

    static juce::File getDataDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Smooth Synth Operator");
    }

private:
    std::unique_ptr<juce::PropertiesFile> file;
};

} // namespace sso
