# Build step (cmake -P): writes <app>_Version.h with the app's current version - only when it
# changed, so an unchanged version recompiles nothing. See SSOVersion.cmake.

include ("${CMAKE_CURRENT_LIST_DIR}/SSOVersion.cmake")

string (REPLACE "|" ";" code_paths "${CODE_PATHS}")
sso_compute_version (version "${VERSION_FILE}" "${REPO_ROOT}" "${GIT_EXECUTABLE}" ${code_paths})

set (content "#pragma once\n#define SSO_APP_VERSION \"${version}\"\n")

if (EXISTS "${OUTPUT}")
    file (READ "${OUTPUT}" existing)
endif ()

if (NOT "${existing}" STREQUAL "${content}")
    file (WRITE "${OUTPUT}" "${content}")
    message (STATUS "App version ${version}")
endif ()
