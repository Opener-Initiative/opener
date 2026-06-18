# Copyright (c) 2026 Codium Electronique
# SPDX-License-Identifier: Apache-2.0

import os
from pathlib import Path
import sys
import re

# Paths ------------------------------------------------------------------------

OPENER_BASE = Path(__file__).absolute().parents[1]
_zephyr_base = os.environ.get("ZEPHYR_BASE")
if not _zephyr_base:
    raise OSError("ZEPHYR_BASE environment variable is not set. Source a Zephyr environment before building docs.")
ZEPHYR_BASE = Path(_zephyr_base)

# General configuration --------------------------------------------------------

project = "Opener"
copyright = "2026, Opener Initiative"
author = "Opener Initiative"
version = release = "0.0.99"

sys.path.insert(0, str(ZEPHYR_BASE / "doc" / "_extensions"))

extensions = [
    "zephyr.doxyrunner",
    "zephyr.doxybridge",
    "zephyr.external_content",
    "notfound.extension",
]

# Options for HTML output ------------------------------------------------------

html_theme = "sphinx_rtd_theme"
html_static_path = [str(OPENER_BASE / "doc" / "_static")]
html_last_updated_fmt = "%b %d, %Y"
html_show_sourcelink = True
html_show_sphinx = False
html_title = "Opener documentation"

html_logo = "_static/opener_logo.svg"

html_theme_options = {
    "logo_only": True,
    "collapse_navigation": False,
    "sticky_navigation": True,
    "navigation_depth": 4,
    "prev_next_buttons_location": "bottom",
    "style_external_links": False,
}

html_css_files = ["custom.css"]

# -- Options for doxyrunner plugin ---------------------------------------------

doxyrunner_outdir = Path(os.environ["DOCSET_BUILD_DIR"]) / "doxygen"

doxyrunner_doxygen = "doxygen"
doxyrunner_projects = {
    "opener": {
        "doxyfile": OPENER_BASE / "doc" / "doxyfile.in",
        "outdir": doxyrunner_outdir,
        "fmt": True,
        "fmt_vars": {
            "OPENER_BASE": str(OPENER_BASE),
            "DOCSET_SOURCE_BASE": str(OPENER_BASE),
            "DOCSET_BUILD_DIR": str(doxyrunner_outdir),
            "DOCSET_VERSION": version,
        },
        "outdir_var": "DOCSET_BUILD_DIR",
    },
}

nitpick_ignore = [
    # ignore C standard identifiers (they are not defined in Zephyr docs)
    ("c:identifier", "FILE"),
    ("c:identifier", "int8_t"),
    ("c:identifier", "int16_t"),
    ("c:identifier", "int32_t"),
    ("c:identifier", "int64_t"),
    ("c:identifier", "intptr_t"),
    ("c:identifier", "off_t"),
    ("c:identifier", "size_t"),
    ("c:identifier", "ssize_t"),
    ("c:identifier", "time_t"),
    ("c:identifier", "uint8_t"),
    ("c:identifier", "uint16_t"),
    ("c:identifier", "uint32_t"),
    ("c:identifier", "uint64_t"),
    ("c:identifier", "uintptr_t"),
    ("c:identifier", "va_list"),
]

# -- Options for zephyr.external_content ----------------------------------

external_content_contents = [
    (OPENER_BASE / "doc", "[!_]*"),
    (OPENER_BASE, "boards/**/*.rst"),
    (OPENER_BASE, "boards/**/doc"),
    (OPENER_BASE, "samples/**/*.html"),
    (OPENER_BASE, "samples/**/*.rst"),
    (OPENER_BASE, "samples/**/doc"),
    (OPENER_BASE, "snippets/**/*.rst"),
    (OPENER_BASE, "snippets/**/doc"),
    (OPENER_BASE, "tests/**/*.pts"),
]
external_content_keep = [
    "reference/kconfig/*",
    "develop/manifest/index.rst",
    "build/dts/api/bindings.rst",
    "build/dts/api/bindings/**/*",
    "build/dts/api/compatibles/**/*",
]

# -- Options for doxybridge plugin ---------------------------------------------

doxybridge_projects = {
    "opener": doxyrunner_outdir,
}

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

root_doc = "index"