juce_add_console_app(SSO_Tests PRODUCT_NAME "SSO Tests")

target_sources(SSO_Tests PRIVATE TestMain.cpp Kiwi3PDeviceTests.cpp LibraryTests.cpp HardwareTest.cpp)

target_compile_definitions(SSO_Tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_UNIT_TESTS=1
    # Personal backup of the synth's memory (gitignored); tests using it are skipped if absent
    SSO_LOCAL_TONE_BACKUP="${CMAKE_SOURCE_DIR}/backups/2026-10-09_jx3p_full/tones_all_256.syx")

target_link_libraries(SSO_Tests
    PRIVATE
        sso_core
        sso_kiwi3p
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
