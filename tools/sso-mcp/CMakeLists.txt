juce_add_console_app(SSO_MCP PRODUCT_NAME "sso-mcp")

target_sources(SSO_MCP PRIVATE Main.cpp)

target_compile_definitions(SSO_MCP PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(SSO_MCP
    PRIVATE
        sso_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# the file users put into their AI program's configuration
set_target_properties(SSO_MCP PROPERTIES OUTPUT_NAME "sso-mcp")
