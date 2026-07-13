# Copyright (c) 2026 Codium Electronique
# SPDX-License-Identifier: Apache-2.0

zephyr_library_named(dectnrp_driver)

zephyr_library_sources_ifdef(CONFIG_DECTNRP_DRIVER_NRF91
    dectnrp_nrf91.c
)