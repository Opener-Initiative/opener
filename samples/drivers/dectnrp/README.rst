Opener dectnrp driver
#####################

Overview
********

This sample shows how to include the Opener NR+ stack as an out-of-tree
Zephyr module in your project, and start using the dectnrp driver API.

Supported boards
****************

- ``nrf9151dk/nrf9151/ns``

Building
********

Builds with nRF NCS v3.1.1 using nrfutil managed toolchain and SDK.

.. code-block:: bash

    nrfutil sdk-manager toolchain launch --ncs-version v3.1.1 --shell
    source ~/ncs/v3.1.1/zephyr/zephyr-env.sh
    west build --build-dir ./build_phy --pristine --board nrf9151dk/nrf9151/ns --sysbuild -S driver_nrf91 -- -DCONFIG_DEBUG_OPTIMIZATIONS=y -DCONFIG_DEBUG_THREAD_INFO=y -DEXTRA_ZEPHYR_MODULES="$(pwd)/../../.."
