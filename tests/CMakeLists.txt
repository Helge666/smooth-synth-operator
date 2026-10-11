juce_add_console_app(SSO_Tests PRODUCT_NAME "SSO Tests")

target_sources(SSO_Tests PRIVATE TestMain.cpp Kiwi3PDeviceTests.cpp KiwiSixTests.cpp LibraryTests.cpp GeneratorTests.cpp GuiTests.cpp OwnershipTests.cpp MidiLoopTests.cpp NoteForwarderTests.cpp Vst3HostTest.cpp Vst3TakeOverTest.cpp HardwareTest.cpp Snapshot.cpp
    # the app's processor and GUI, for offscreen snapshots
    ../apps/SSO-Kiwi3P/Source/PluginProcessor.cpp
    ../apps/SSO-Kiwi3P/Source/PluginEditor.cpp
    ../apps/SSO-Kiwi3P/Source/Kiwi3PPanel.cpp
    ../apps/SSO-KiwiSix/Source/PluginProcessor.cpp
    ../apps/SSO-KiwiSix/Source/PluginEditor.cpp
    ../apps/SSO-KiwiSix/Source/KiwiSixPanel.cpp)

target_compile_definitions(SSO_Tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_UNIT_TESTS=1
    JucePlugin_Name="SSO Kiwi-3P"
    # Personal backup of the synth's memory (gitignored); tests using it are skipped if absent
    SSO_LOCAL_TONE_BACKUP="${CMAKE_SOURCE_DIR}/backups/2026-10-09_jx3p_full/tones_all_256.syx"
    SSO_LOCAL_KIWISIX_BACKUP="${CMAKE_SOURCE_DIR}/backups/2026-10-11_polysix_v5_after_update/tones_all_512.syx"
    SSO_LOCAL_KIWISIX_FACTORY="${CMAKE_SOURCE_DIR}/backups/kiwisix_downloads/Kiwi6_factory3.syx")

target_link_libraries(SSO_Tests
    PRIVATE
        sso_core
        sso_kiwitechnics
        sso_kiwi3p
        sso_kiwisix
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
