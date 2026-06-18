# Copyright (c) 2026 Codium Electronique
# SPDX-License-Identifier: Apache-2.0

include (cmake/version.cmake)

zephyr_include_directories(include)

add_subdirectory(drivers)
add_subdirectory(subsys)
