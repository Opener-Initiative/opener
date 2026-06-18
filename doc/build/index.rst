Build guide
###########

Adding Opener as a Zephyr module
*********************************

The Opener stack is provided as an out-of-tree Zephyr module plugged into CMake
through the ``EXTRA_ZEPHYR_MODULES`` variable, as specified in the
`Zephyr module documentation <https://docs.zephyrproject.org/latest/develop/modules.html#integrate-modules-in-zephyr-build-system>`_.

Example west command:

.. code-block:: bash

    west build ... -- -DEXTRA_ZEPHYR_MODULES=<path_to_opener>

Using snippets
**************

We provide Zephyr snippets to easily provide ready-made configuration.

For example, you can choose to build with the nrf91 driver as follows:

.. code-block:: bash

    west build .. -S driver_nrf91

See the `Zephyr snippets documentation <https://docs.zephyrproject.org/latest/build/snippets/index.html>`_ for more details.

Building the documentation
**************************

This documentation, along with the API reference generated from Doxygen comments,
can be generated automatically.

Dependencies
============

- Python 3.8+
- Doxygen 1.12+
- Mscgen

Build and serve
===============

You must ensure the Zephyr environment is available (``ZEPHYR_BASE`` must be set).

.. code-block:: bash

    cd <opener_path>/doc
    # If not already installed
    python -m pip install -r requirements.txt
    cmake -GNinja -S. -Bdoc_output
    cmake --build doc_output
    cd doc_output
    python -m http.server

Access the documentation at http://localhost:8000/html/opener
