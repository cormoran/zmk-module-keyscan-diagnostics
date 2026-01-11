# ZMK Keyscan Diagnostics Module

A comprehensive ZMK module for diagnosing keyboard matrix issues, including chattering detection and GPIO state monitoring. This module helps identify problems like insufficient soldering, which are common when users build their own keyboards with hot-swap sockets.

## Features

- **Real-time Monitoring**: Track key press/release events as they happen
- **Chattering Detection**: Automatically identify keys with unstable contacts
- **GPIO Visualization**: Display GPIO pin configuration for debugging
- **Interactive Web UI**: User-friendly interface for diagnostics
- **Charlieplex Support**: Currently optimized for charlieplex matrix keyboards
- **Extensible Design**: Architecture allows easy addition of other kscan driver support

## Quick Start

### 1. Add Dependency to Your Keyboard

Add this module to your `config/west.yml`:

```yaml
manifest:
  remotes:
    - name: cormoran
      url-base: https://github.com/cormoran
  projects:
    - name: zmk-module-keyscan-diagnostics
      remote: cormoran
      revision: main
    # Required: Custom ZMK fork with Studio RPC support
    - name: zmk
      remote: cormoran
      revision: v0.3+custom-studio-protocol
      import:
        file: app/west.yml
```

### 2. Enable in Your Keyboard Configuration

Add to your `config/<keyboard>.conf`:

```conf
# Enable ZMK Studio and Keyscan Diagnostics
CONFIG_ZMK_STUDIO=y
CONFIG_ZMK_KEYSCAN_DIAGNOSTICS=y
CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_STUDIO_RPC=y

# Optional: Configure settings
CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_EVENT_BUFFER_SIZE=100
CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_CHATTER_THRESHOLD_MS=50
```

### 3. Build and Flash

Build your firmware as usual:

```bash
west build -b <your_board> -d build/left config
```

### 4. Use the Web UI

1. Visit the deployed web UI: https://cormoran.github.io/zmk-module-keyscan-diagnostics/
2. Connect to your keyboard via Serial
3. Start monitoring and test your keys
4. Check for chattering or other issues

## Configuration Options

### Event Buffer Size

Controls how many recent events are stored for analysis:

```conf
CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_EVENT_BUFFER_SIZE=100
```

- Default: 100 events
- Larger values use more RAM but provide more history
- Recommended: 50-200 depending on available RAM

### Chattering Threshold

Sets the interval threshold for detecting chattering (in milliseconds):

```conf
CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_CHATTER_THRESHOLD_MS=50
```

- Default: 50ms
- Events occurring faster than this are considered chattering
- Can be adjusted in the web UI at runtime

## Using the Diagnostics Module

### Web UI Features

#### Connection
- Click "Connect Serial" to connect to your keyboard
- Make sure your keyboard has ZMK Studio enabled

#### Monitoring Control
- **Start Monitoring**: Begin capturing key events
- **Stop Monitoring**: Pause event capture
- **Clear Data**: Reset all collected statistics
- **Refresh**: Manually update the display
- **Chattering Threshold**: Adjust sensitivity (10-500ms)

#### GPIO Configuration
- Shows the GPIO pins used in your keyboard matrix
- Displays matrix dimensions (rows × columns)
- Useful for identifying which physical pins correspond to which keys

#### Chattering Detection
- Automatically highlights keys with chattering issues
- Shows:
  - Key position (row, col)
  - Total events recorded
  - Number of chattering events detected
  - Minimum interval between events
  - Severity indicator (🟢 Minor / 🟡 Warning / 🔴 Critical)

#### Recent Events
- Displays the last 20 key events
- Shows press (⬇️) and release (⬆️) actions
- Timestamps help identify timing issues

### Troubleshooting Common Issues

#### Key doesn't work at all
1. Check GPIO configuration to ensure the pin is correct
2. Verify the physical connection/soldering
3. Check if events appear when you bridge the switch contacts directly

#### Key is unstable/chattering
1. Look at the Chattering Detection section
2. Check the minimum interval - values under 20ms indicate bad contact
3. Resolder the hot-swap socket or switch
4. Consider replacing the switch if chattering persists

#### Multiple keys affected
1. Check if they share a common GPIO pin
2. May indicate a cold solder joint on that pin
3. Check GPIO pin info to identify the shared pin

## Architecture

### Firmware Components

```
include/zmk/keyscan_diagnostics.h     - Public API
src/diagnostics/keyscan_diagnostics.c - Core diagnostics engine
src/studio/keyscan_diagnostics_handler.c - RPC handler
proto/zmk/keyscan_diagnostics/custom.proto - Protocol definition
```

### How It Works

1. **Event Capture**: The module subscribes to ZMK keycode state change events
2. **Data Storage**: Events are stored in a circular buffer
3. **Analysis**: Each event is analyzed for chattering patterns
4. **RPC Communication**: Web UI communicates via custom Studio RPC protocol
5. **Visualization**: Real-time updates displayed in the web interface

### Extending to Other Kscan Drivers

The module is designed to be extensible. To add support for other kscan drivers:

1. Update device tree parsing in `keyscan_diagnostics_init()`
2. Add driver-specific GPIO extraction logic
3. Adjust matrix size calculation if needed
4. Test with the new driver type

Current support:
- ✅ Charlieplex matrix (zmk_kscan_gpio_charlieplex)
- 🔄 GPIO matrix (planned)
- 🔄 GPIO direct (planned)
- 🔄 GPIO demux (planned)

## Development

### Building Tests

```bash
# Run firmware tests
python -m unittest

# Run web UI tests
cd web
npm test
```

### Local Web UI Development

```bash
cd web
npm install
npm run dev
```

The development server will start at http://localhost:5173

### Project Structure

```
├── include/           # Public header files
├── src/
│   ├── diagnostics/  # Core diagnostics implementation
│   └── studio/       # RPC handler
├── proto/            # Protocol buffer definitions
├── web/              # Web UI (React + TypeScript)
│   ├── src/         # Source code
│   └── test/        # Jest tests
└── tests/            # Firmware tests
```

## Requirements

### Firmware Requirements
- ZMK with custom Studio RPC support
- Charlieplex matrix keyboard (for now)
- Serial connection capability

### Supported Boards
- Seeed Studio XIAO nRF52840
- Seeed Studio XIAO nRF52840 BLE
- Other nRF52840-based boards with Studio support

## Known Limitations

1. **Charlieplex Only**: Currently optimized for charlieplex matrices
2. **Event Capture**: Uses keycode events, not direct kscan events (may miss some edge cases)
3. **Memory**: Event buffer size affects RAM usage
4. **No Bluetooth**: Diagnostics work over Serial only

## Contributing

Contributions are welcome! Areas for improvement:

- Support for additional kscan driver types
- Enhanced visualization in web UI
- Export diagnostics data to CSV/JSON
- Statistical analysis of key behavior
- Hardware-specific GPIO pin mapping

## License

MIT License - See LICENSE file for details

## Credits

Built on the ZMK Module Template by [@cormoran](https://github.com/cormoran)

Uses:
- [ZMK Firmware](https://zmk.dev/)
- [react-zmk-studio](https://github.com/cormoran/react-zmk-studio)
- React, TypeScript, Vite

## Support

For issues and questions:
- GitHub Issues: https://github.com/cormoran/zmk-module-keyscan-diagnostics/issues
- ZMK Discord: https://zmk.dev/community/discord/invite

## See Also

- [ZMK Documentation](https://zmk.dev/docs)
- [ZMK Studio](https://zmk.dev/docs/features/studio)
- [Module Creation Guide](https://zmk.dev/docs/development/module-creation)
