# FSR Matrix Readout Firmware

ESP-IDF firmware for scanning a 16 x 16 force-sensitive resistor (FSR)
matrix with an ESP32-S3. The firmware selects each drive line, samples all 16
sense channels, averages repeated ADC conversions, and streams the result as
WiReSens-compatible serial packets.

## Project Layout

| Path | Purpose |
| --- | --- |
| `main/main.c` | Initializes the scanner and WiReSens transport, then continuously acquires and sends frames. |
| `main/src/decoder.c` | Drives the 4-bit address of the 16-way drive-line decoder. |
| `main/src/sense.c` | Configures continuous ADC sampling, switches drive lines, discards settling samples, and averages each sense channel. |
| `main/src/wiresens.c` | Packs frames in the WiReSens wire format and transmits them over UART. |
| `main/include/pin_defs.h` | Board pin assignments and the sense-to-ADC channel map. |
| `main/include/sense.h` | ADC scanner configuration and frame-reading interface. |
| `main/include/wiresens.h` | WiReSens transport configuration and packet interface. |
| `WiSensConfig.json` | Ready-to-load configuration for the WiReSens web interface. |
| `sdkconfig` | ESP-IDF target, flash, ADC, and console configuration. |

## Connections

The UART data channel and the diagnostic console are intentionally separate:

- **WiReSens data:** UART0 on GPIO43 (TX) and GPIO44 (RX), using a USB-to-UART
  bridge. The default baud rate is `921600`.
- **Logs and diagnostics:** the ESP32-S3 native USB Serial/JTAG connection.
  ESP-IDF logs, `ESP_ERROR_CHECK` failures, and panic backtraces are routed to
  this port so they cannot corrupt the binary WiReSens stream.

Both USB connections can be used at the same time. The serial port configured
in `WiSensConfig.json` must be the USB-to-UART bridge port, not the USB
Serial/JTAG console port.

Only one application can normally open a serial port at a time.
Stop VS Code Serial Monitor or any other terminal connected to the data port
before starting the WiReSens backend.

## Build and Flash

The firmware targets ESP32-S3 and ESP-IDF v6.0.1. From this directory, run:

```powershell
idf.py build
idf.py -p <USB_SERIAL_JTAG_PORT> flash monitor
```

If the port is unclear, you can also run `idf.py` without `-p` and it will automatically detect the USB Serial/JTAG port.

Use the native USB Serial/JTAG port for `flash monitor`. UART0 is reserved for
the WiReSens binary stream.

## Visualize and Record with WiReSens

This firmware only requires the
[WiReSensBackend](https://github.com/WiReSens-Toolkit/WiReSensBackend) from the
WiReSens Toolkit. The backend requires Python 3.10 or later.

### 1. Install the backend

Follow the installation instructions in the
[WiReSensBackend documentation](https://github.com/WiReSens-Toolkit/WiReSensBackend/blob/main/readme.md).

### 2. Check the sensor configuration

Before loading [`WiSensConfig.json`](WiSensConfig.json), update these fields if
necessary:

```json
{
  "sensors": [
    {
      "id": 1,
      "serialPort": "COM5"
    }
  ],
  "serialOptions": {
    "baudrate": 921600,
    "numNodes": 64
  }
}
```

- `serialPort` must match the USB-to-UART bridge detected by the operating
  system, such as `COM5` on Windows or `/dev/ttyUSB0` on Linux.
- `baudrate` must match the `baud_rate` selected in the
  `fsr_wiresens_cfg_t` passed to `fsr_wiresens_init()`. When
  `fsr_wiresens_default_cfg()` is used without an override, the default is
  `921600`.
- `numNodes` must match the selected `nodes_per_packet`. When the default
  configuration is used without an override, the value is 64.
- The sensor `id` must match the selected `sensor_id`. The default
  configuration uses 1.

### 3. Start the backend

From the WiReSensBackend directory, run:

```powershell
python startBackend.py
```

Keep this terminal running. The backend starts its local Flask-SocketIO server
on port `5328`.

### 4. Load the configuration and record

1. Open the [WiReSens web interface](https://wi-re-sens-web.vercel.app/).
2. Click **Load Config** on the top bar.
3. Select `WiSensConfig.json` from this directory.
4. Click **Record** on the top bar.

The live sensor values should now appear in the web interface. Recordings are
written as HDF5 files under the backend's `recordings/` directory.
