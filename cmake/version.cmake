# Copyright (c) 2026 Codium Electronique
# SPDX-License-Identifier: Apache-2.0

if(EXISTS ${ZEPHYR_OPENER_MODULE_DIR}/.git)
    find_package(Git QUIET)
    if(GIT_FOUND)
        add_custom_command(
            OUTPUT ${PROJECT_BINARY_DIR}/include/generated/opener/build_info.h
            COMMAND ${CMAKE_COMMAND}
                -DZEPHYR_BASE=${ZEPHYR_BASE}
                -DZEPHYR_OPENER_MODULE_DIR=${ZEPHYR_OPENER_MODULE_DIR}
                -DOUT_FILE=${PROJECT_BINARY_DIR}/include/generated/opener/build_info.h
                -P ${ZEPHYR_OPENER_MODULE_DIR}/cmake/gen_opener_build_info.h.cmake
            DEPENDS ${ZEPHYR_OPENER_MODULE_DIR}/.git
        )
        add_custom_target(opener_build_info_h
            DEPENDS ${PROJECT_BINARY_DIR}/include/generated/opener/build_info.h
        )
        add_dependencies(app opener_build_info_h)
        zephyr_compile_definitions(OPENER_HAS_BUILD_INFO)
    endif()
else()
    message(FATAL_ERROR "Opener: no .git directory found, build_info.h cannot be generated.")
endif()
