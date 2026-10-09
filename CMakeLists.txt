cmake_minimum_required(VERSION 3.22)

project(SmoothSynthOperator VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(external/JUCE)

# SSO framework and device modules (JUCE module format)
juce_add_module(framework/sso_core)
juce_add_module(devices/sso_kiwi3p)

add_subdirectory(apps/SSO-Kiwi3P)
add_subdirectory(tests)
