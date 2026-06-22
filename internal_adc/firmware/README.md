# FSR Matrix Readout Firmware

ESP-IDF firmware for scanning a 16 x 16 force-sensitive resistor (FSR)
matrix with an ESP32-S3. The firmware selects each drive line, samples all 16
sense channels, averages repeated ADC conversions, and streams the result as
WiReSens-compatible serial packets.

## Project Layout

| Path                      | Purpose                                                                                                               |
| ------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| `main/main.c`             | Initializes the scanner and WiReSens transport, then continuously acquires and sends frames.                          |
| `main/src/decoder.c`      | Drives the 4-bit address of the 16-way drive-line decoder.                                                            |
| `main/src/sense.c`        | Configures continuous ADC sampling, switches drive lines, discards settling samples, and averages each sense channel. |
| `main/src/wiresens.c`     | Packs frames in the WiReSens wire format and transmits them over the selected UART or USB Serial/JTAG transport.      |
| `main/include/pin_defs.h` | Board pin assignments and the sense-to-ADC channel map.                                                               |
| `main/include/sense.h`    | ADC scanner configuration and frame-reading interface.                                                                |
| `main/include/wiresens.h` | WiReSens transport configuration and packet interface.                                                                |
| `WiSensConfig.json`       | Ready-to-load configuration for the WiReSens web interface.                                                           |
| `sdkconfig`               | ESP-IDF target, flash, ADC, and console configuration.                                                                |

## Connections

WiReSens data can be transmitted through either of these interfaces:

- **UART:** UART0 on GPIO43 (TX) and GPIO44 (RX), normally through a
  USB-to-UART bridge. The default baud rate is `921600`.
- **USB Serial/JTAG:** the ESP32-S3 native USB Serial/JTAG connection. Its USB
  transfer rate is independent of the baud rate selected by the host.

WiReSens packets and ESP-IDF console logs must not share an interface. Logs,
panic output, or other text inserted into the binary WiReSens stream will
corrupt packet framing. Route the console to the other interface or disable it.

Configure the interfaces with `idf.py menuconfig`, (you can also press `/` to search for the item):

- WiReSens data: `(Top) > FSR Matrix Readout > WiReSens transport`
- Primary console: `(Top) > Component config > ESP-STDIO > Channel for console output`
- Secondary console: `(Top) > Component config > ESP-STDIO > Channel for console secondary output`

To disable console output, select **None** for the primary console and
**No secondary console** for the secondary console. 

The project Kconfig selects **UART** as the default WiReSens interface.

Unavailable transport choices are hidden when they conflict with the current
console configuration. Configure the console first, then select the WiReSens
transport. 

Only one application can normally open a serial port at a time.
Stop VS Code Serial Monitor or any other terminal connected to the data port before starting the WiReSens backend.

## Build and Flash

The firmware targets ESP32-S3 and ESP-IDF v6.0.1. From this directory, run:

```powershell
idf.py build
idf.py -p <USB_SERIAL_JTAG_PORT> flash monitor
```

If the port is unclear, you can also run `idf.py` without `-p` and it will automatically detect the USB Serial/JTAG port.

The native USB Serial/JTAG port can always be used for flashing. Run `monitor`
on the configured console port, not on the interface carrying WiReSens data.

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

- `serialPort` must match the selected WiReSens transport: either the
  USB-to-UART bridge or the native USB Serial/JTAG serial port detected by the
  operating system.
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
