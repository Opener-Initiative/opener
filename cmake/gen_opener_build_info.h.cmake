# Copyright (c) 2026 Codium Electronique
# SPDX-License-Identifier: Apache-2.0

# If git found and is a git dir, check nearest tag match VERSION file 
# and retrieve commit and dirty status
find_package(Git QUIET)
if(GIT_FOUND)
    # Get nearest opener version from git tag
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --abbrev=0 --tags
        WORKING_DIRECTORY               ${ZEPHYR_OPENER_MODULE_DIR}
        OUTPUT_VARIABLE                 OPENER_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE                  stderr
        RESULT_VARIABLE                 return_code
    )
    if(return_code)
        message(STATUS "Failed to get latest opener git tag: ${stderr}")
    elseif(NOT "${stderr}" STREQUAL "")
        message(STATUS "Warning getting latest opener git tag: ${stderr}")
    endif()

    if (NOT DEFINED OPENER_TAG OR "${OPENER_TAG}" STREQUAL "")
        set(OPENER_VERSION_MAJOR "0")
        set(OPENER_VERSION_MINOR "0")
        set(OPENER_PATCHLEVEL "99")
    else()
        string(REGEX MATCH "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ ${OPENER_TAG})
        if(NOT CMAKE_MATCH_1)
            message(FATAL_ERROR "The git tag is not a valid version in vX.X.X format")
        endif()

        set(OPENER_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(OPENER_VERSION_MINOR ${CMAKE_MATCH_2})
        set(OPENER_PATCHLEVEL ${CMAKE_MATCH_3})
    endif()

    # Get latest opener git commit, truncated to 12 characters
    execute_process(
        COMMAND                         ${GIT_EXECUTABLE} rev-parse --short=12 HEAD
        WORKING_DIRECTORY               ${ZEPHYR_OPENER_MODULE_DIR}
        OUTPUT_VARIABLE                 OPENER_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE                  stderr
        RESULT_VARIABLE                 return_code
    )
    if(return_code OR NOT "${stderr}" STREQUAL "")
        message(FATAL_ERROR "Failed to get latest opener git commit: ${stderr}")
    endif()
    message(STATUS "Opener commit: ${OPENER_GIT_COMMIT}")

    # Get opener git dirty status
    # FIXME: This doesn't detect untracked files
    execute_process(
        COMMAND                 ${GIT_EXECUTABLE} diff --quiet --exit-code
        WORKING_DIRECTORY       ${ZEPHYR_OPENER_MODULE_DIR}
        ERROR_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE          stderr
        RESULT_VARIABLE         return_code
    )
    if (return_code)
        if (NOT "${stderr}" STREQUAL "")
            message(STATUS "opener failed to get dirty status: ${stderr}")
        endif()
        set(OPENER_DIRTY "1")
    else()
        set(OPENER_DIRTY "0")
    endif()
    message(STATUS "Opener dirty: ${OPENER_DIRTY}")

    file(READ ${ZEPHYR_OPENER_MODULE_DIR}/cmake/opener_build_info.h.in opener_build_info_content)
    string(CONFIGURE "${opener_build_info_content}" opener_build_info_content)

    if(EXISTS ${OUT_FILE})
        file(READ ${OUT_FILE} current_contents)
    else()
        set(current_contents "")
    endif()

    if(NOT "${current_contents}" STREQUAL "${opener_build_info_content}")
        file(WRITE ${OUT_FILE} "${opener_build_info_content}")
    endif()

endif()
