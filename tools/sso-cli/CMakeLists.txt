juce_add_console_app(SSO_CLI PRODUCT_NAME "sso-cli")

target_sources(SSO_CLI PRIVATE Main.cpp)

target_compile_definitions(SSO_CLI PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(SSO_CLI
    PRIVATE
        sso_core
        sso_kiwitechnics
        sso_kiwi3p
        sso_kiwisix
        sso_hawk800
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
