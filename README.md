# ZMK Keyscan Diagnostics Module

A ZMK module with Web UI for diagnosing keyboard switch and soldering issues. This module helps keyboard kit builders and users identify problems like:

- Keys that don't register (insufficient soldering)
- Chattering keys (intermittent contact due to cold solder joints)
- Multiple keys affected by the same GPIO pin issues

## Features

### Firmware Module
- **Real-time key event monitoring** - Track all key presses and releases with timestamps
- **Chattering detection** - Automatically detect rapid state changes indicating poor solder joints
- **GPIO pin information** - View which GPIO pins are used for each key position
- **Key statistics** - Track press/release counts per key to identify problematic switches

### Web UI
- **Interactive key matrix visualization** - See which keys are pressed, have recent activity, or are chattering
- **Event log** - Real-time stream of key events with GPIO information
- **GPIO pin viewer** - Visual representation of pin configuration with Xiao board pinout
- **Chattering alerts** - Highlighted warnings when chattering is detected
- **Troubleshooting guide** - Built-in help for common soldering issues

### Currently Supported Keyscan Types
- ✅ **Charlieplex matrix** (`zmk,kscan-gpio-charlieplex`)
- 🔜 GPIO matrix (planned)
- 🔜 Direct GPIO (planned)

## Quick Start

### 1. Add dependency to your `config/west.yml`

```yaml
manifest:
  remotes:
    - name: cormoran
      url-base: https://github.com/cormoran
  projects:
    - name: zmk-module-keyscan-diagnostics
      remote: cormoran
      revision: main
    # Use the custom studio protocol fork of ZMK
    - name: zmk
      remote: cormoran
      revision: v0.3+custom-studio-protocol
      import:
        file: app/west.yml
```

### 2. Enable in your `config/<shield>.conf`

```conf
# Enable keyscan diagnostics
CONFIG_ZMK_KEYSCAN_DIAGNOSTICS=y

# Enable Studio RPC for web UI (requires ZMK Studio)
CONFIG_ZMK_STUDIO=y
CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_STUDIO_RPC=y
```

### 3. Build with Studio RPC snippet

```bash
west build -b your_board -- -DSHIELD=your_shield -DSNIPPET=studio-rpc-usb-uart
```

### 4. Use the Web UI

Open the web UI at https://cormoran.github.io/zmk-module-keyscan-diagnostics (or run locally with `npm run dev` in the `web/` directory).

1. Connect your keyboard via USB
2. Click "Connect Serial"
3. Start monitoring to see real-time key events
4. Press keys to test - problematic keys will be highlighted

## Understanding the Diagnostics

### Key Matrix View
The key matrix shows all possible key positions in the charlieplex matrix:
- **Green**: Currently pressed
- **Blue**: Recent activity
- **Orange (pulsing)**: Chattering detected - likely soldering issue
- **Gray**: Idle / No activity

### Chattering Detection
Chattering occurs when a key rapidly switches between pressed and released states. This typically indicates:
- Cold solder joint on hot-swap socket
- Debris in switch or socket  
- Loose switch not fully seated

The module detects chattering by tracking the number of events within a configurable time window.

### GPIO Information
For charlieplex keyboards, each key is identified by two GPIO pins:
- **Row (Output)**: The pin driving the signal
- **Column (Input)**: The pin reading the signal

If multiple keys in the same row or column are affected, check the shared GPIO pin connection.

## Configuration Options

```conf
# Maximum events to buffer (default: 64)
CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_MAX_EVENTS=64

# Chattering detection window in ms (default: 50)
CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_CHATTERING_WINDOW_MS=50

# Events in window to trigger alert (default: 4)
CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_CHATTERING_THRESHOLD=4
```

## Development

### Setup

```bash
# Clone repository
git clone https://github.com/cormoran/zmk-module-keyscan-diagnostics
cd zmk-module-keyscan-diagnostics

# Initialize west workspace
west init -l west --mf west-test-standalone.yml
west update --narrow
west zephyr-export
```

### Running Tests

**Firmware tests:**
```bash
python -m unittest
```

**Web UI tests:**
```bash
cd web
npm install
npm test
```

### Running Web UI locally

```bash
cd web
npm install
npm run dev
```

Then open http://localhost:5173 in your browser.

## Troubleshooting

### Key doesn't register at all
1. Check if the hot-swap socket is properly soldered
2. Verify the switch is fully inserted into the socket
3. Check for cold solder joints (dull, grainy appearance)
4. Use the GPIO Pins tab to identify which pin might have an issue

### Key shows chattering
1. Reflow solder on the hot-swap socket pads
2. Check for debris in the switch or socket
3. Try a different switch to rule out switch defects
4. Consider increasing debounce time: `CONFIG_ZMK_KSCAN_DEBOUNCE_PRESS_MS=10`

### Multiple keys affected in same row/column
1. Check the shared GPIO pin connection
2. Look for solder bridges between adjacent pads
3. Verify continuity of traces on the PCB

## License

MIT License - See [LICENSE](LICENSE) for details.

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.
