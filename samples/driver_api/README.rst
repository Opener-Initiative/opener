Opener driver API Sample
########################

Overview
********

This sample shows how to include the Opener NR+ stack as an out-of-tree
Zephyr module in your project.

Supported boards
****************

- ``nrf9151dk/nrf9151/ns``

Building
********

.. code-block:: bash

    west build --build-dir ./build_phy --pristine --board nrf9151dk/nrf9151/ns --sysbuild -S driver_nrf91 -- -DCONFIG_DEBUG_OPTIMIZATIONS=y -DCONFIG_DEBUG_THREAD_INFO=y -DEXTRA_ZEPHYR_MODULES="$(pwd)/../.."
