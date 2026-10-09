juce_add_plugin(SSO_Kiwi3P
    PRODUCT_NAME "SSO Kiwi-3P"
    COMPANY_NAME "Smooth Synth Operator"
    BUNDLE_ID "org.smoothsynthoperator.kiwi3p"
    PLUGIN_MANUFACTURER_CODE Ssop
    PLUGIN_CODE Kw3p
    FORMATS Standalone VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument External
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(SSO_Kiwi3P PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Kiwi3PPanel.cpp)

target_compile_definitions(SSO_Kiwi3P PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(SSO_Kiwi3P
    PRIVATE
        sso_core
        sso_kiwi3p
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
