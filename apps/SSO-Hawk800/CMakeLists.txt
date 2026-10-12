juce_add_plugin(SSO_Hawk800
    PRODUCT_NAME "SSO Hawk-800"
    COMPANY_NAME "Smooth Synth Operator"
    BUNDLE_ID "org.smoothsynthoperator.hawk800"
    PLUGIN_MANUFACTURER_CODE Ssop
    PLUGIN_CODE Hk80
    FORMATS Standalone VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument External
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(SSO_Hawk800 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PluginEntry.cpp
    Source/Hawk800Panel.cpp)

target_compile_definitions(SSO_Hawk800 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(SSO_Hawk800
    PRIVATE
        sso_core
        sso_hawk800
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
