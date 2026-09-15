# Local PaperMono development on macOS

The project uses a **uv-created `.venv`** for Python tooling. PlatformIO manages its own ESP32 packages
and internal Python environment; leave those under its control.

Installed and checked on 2026-09-15:

- uv 0.9.10, Python 3.13.3, PlatformIO 6.2.0.
- ESP32-S3 GCC 14.2.0, esptool 5.3.0, and the framework selected by `platformio.ini`.
- PySerial, PyYAML, and clang-format 21.1.8 (invoke formatting only through the repository wrapper).

`.venv`, `.pio`, and `platformio.local.ini` are ignored by Git. The local INI contains:

```ini
[platformio]
core_dir = .pio/core
```

From the repository root:

```sh
source .venv/bin/activate
rtk pio run -e papermono                         # build
rtk pio device list                             # find the USB serial port
rtk pio run -e papermono -t upload               # flash the connected device
rtk pio device monitor -b 115200                 # serial log; Ctrl+C exits
rtk ./bin/clang-format-fix -g
```

If several devices are connected, pass the intended port with `--upload-port` for upload or `--port` for
the monitor. Use a USB data cable. A real device connection/upload has not been tested by this setup.

To recreate the project Python environment, use uv:

```sh
uv venv --python 3.13 .venv
uv pip install --python .venv/bin/python 'platformio==6.2.0' PyYAML 'clang-format>=21,<22'
```

The pinned SDK source is already populated in this checkout. The PaperMono firmware build passed before
the uv migration; package versions were preserved during migration, and compiler, flashing-tool, serial
enumeration, and PlatformIO configuration checks passed afterward. No redundant firmware rebuild was run.
