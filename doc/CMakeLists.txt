# Copyright (c) 2026 Codium Electronique
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
project(opener-doc LANGUAGES NONE)

set(SPHINXOPTS "-j auto -W --keep-going -T" CACHE STRING "Default Sphinx Options")
set(SPHINXOPTS_EXTRA "" CACHE STRING "Extra Sphinx Options")

separate_arguments(SPHINXOPTS)
separate_arguments(SPHINXOPTS_EXTRA)

find_package(Python 3.8)
set(DOXYGEN_SKIP_DOT True)
find_package(Doxygen 1.12.0 REQUIRED)

find_program(SPHINXBUILD sphinx-build)
if(NOT SPHINXBUILD)
  message(FATAL_ERROR "The 'sphinx-build' command was not found")
endif()

find_program(WEST west)
if(NOT WEST)
  message(FATAL_ERROR "The 'west' command was not found")
endif()

set(DOCSET_CFG_DIR ${CMAKE_CURRENT_LIST_DIR})
set(DOCSET_BUILD_DIR ${CMAKE_BINARY_DIR}/opener)
set(DOCSET_SRC_DIR ${CMAKE_BINARY_DIR}/opener/src)
set(DOCSET_DOCTREE_DIR ${CMAKE_BINARY_DIR}/opener/doctree)
set(DOCSET_HTML_DIR ${CMAKE_BINARY_DIR}/html/opener)
set(DOCSET_LINKCHECK_DIR ${CMAKE_BINARY_DIR}/linkcheck/opener)
set(DOCSET_MAKE_DIRS ${DOCSET_BUILD_DIR};${DOCSET_SRC_DIR};${DOCSET_HTML_DIR})
set(DOCSET_CLEAN_DIRS ${DOCSET_BUILD_DIR};${DOCSET_HTML_DIR})
set(DOCSET_ENV DOCSET_BUILD_DIR=${DOCSET_BUILD_DIR};DOXYGEN_EXECUTABLE=${DOXYGEN_EXECUTABLE};DOCSET_VERSION=${version};DOCS_HTML_DIR=${DOCSET_HTML_DIR})

add_custom_target(
    opener-docs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DOCSET_MAKE_DIRS}
    COMMAND ${CMAKE_COMMAND} -E env ${DOCSET_ENV}
    ${SPHINXBUILD}
      -b html
      -c ${DOCSET_CFG_DIR}
      -d ${DOCSET_DOCTREE_DIR}
      -w ${DOCSET_BUILD_DIR}/html.log
      ${SPHINXOPTS}
      ${SPHINXOPTS_EXTRA}
      ${DOCSET_SPHINXOPTS}
      ${EXTRA_ARGS}
      ${DOCSET_SRC_DIR}
      ${DOCSET_HTML_DIR}
    USES_TERMINAL
    COMMENT "Building ${name} docset..."
)

add_custom_target(
    opener-docs-clean
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${DOCSET_CLEAN_DIRS}
)

add_custom_target(build-all ALL)
add_dependencies(build-all
    opener-docs
)