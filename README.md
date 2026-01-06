# ZMK Keyscan Diagnostics Module

> 🔍 **Debug keyboard matrix issues with ease**

This ZMK module helps identify and diagnose keyboard matrix problems such as chattering, insufficient soldering, and GPIO issues. Perfect for keyboard builders who need to troubleshoot hot-swap socket installations.

## 🎯 Key Features

- **🔴 Chattering Detection**: Automatically identify keys with unstable contacts
- **📊 Real-time Monitoring**: Track key events as they happen
- **🔌 GPIO Visualization**: See your keyboard's GPIO configuration
- **🌐 Interactive Web UI**: User-friendly diagnostics interface
- **⚡ Charlieplex Support**: Optimized for charlieplex matrix keyboards
- **🔧 Extensible**: Easy to add support for other kscan drivers

## 🚀 Quick Start

See **[USAGE.md](./USAGE.md)** for detailed setup and usage instructions.

### Basic Setup

1. Add to your `config/west.yml`:
```yaml
projects:
  - name: zmk-module-keyscan-diagnostics
    remote: cormoran
    revision: main
```

2. Enable in your `config/<keyboard>.conf`:
```conf
CONFIG_ZMK_STUDIO=y
CONFIG_ZMK_KEYSCAN_DIAGNOSTICS=y
CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_STUDIO_RPC=y
```

3. Build, flash, and connect via the web UI!

## 📖 Documentation

- **[USAGE.md](./USAGE.md)** - Complete usage guide and troubleshooting
- **[web/README.md](./web/README.md)** - Web UI development guide

## 🌐 Web UI

Access the live web interface: **https://cormoran.github.io/zmk-module-keyscan-diagnostics/**

### Features
- Connect to your keyboard via Serial
- Start/stop monitoring with configurable chattering threshold
- View GPIO pin configuration
- See chattering statistics with severity indicators
- Monitor recent key events in real-time

## 🧪 Testing

```bash
# Firmware tests
python -m unittest

# Web UI tests
cd web
npm test
```

## 📦 What's Included

- **Firmware Module**: Core diagnostics engine with event capture and analysis
- **RPC Protocol**: Custom Studio RPC for communication
- **Web UI**: React-based diagnostics interface
- **Tests**: Both firmware and web UI test suites
- **Documentation**: Comprehensive usage and development guides

## 🎯 Use Cases

### For Keyboard Builders
- Verify hot-swap socket soldering quality
- Identify chattering switches quickly
- Debug matrix wiring issues
- Confirm GPIO pin assignments

### For Keyboard Designers
- Test prototype keyboards
- Validate matrix configurations
- Debug custom kscan implementations
- Gather statistics on key behavior

## 🛠️ Development

This project follows the ZMK module template structure with custom Studio RPC support.

### Project Structure
```
├── include/          # Public headers
├── src/
│   ├── diagnostics/  # Core implementation
│   └── studio/       # RPC handlers
├── proto/            # Protocol buffers
├── web/              # React web UI
└── tests/            # Test suites
```

### Contributing

Contributions welcome! Areas for improvement:
- Additional kscan driver support (GPIO matrix, direct, demux)
- Enhanced visualization (heatmaps, timeline view)
- Data export (CSV, JSON)
- Statistical analysis features

## 📋 Requirements

- ZMK with custom Studio RPC support
- nRF52840 or compatible board
- Serial connection capability
- Currently: Charlieplex matrix keyboard

## 🔗 Links

- **Web UI**: https://cormoran.github.io/zmk-module-keyscan-diagnostics/
- **Issues**: https://github.com/cormoran/zmk-module-keyscan-diagnostics/issues
- **ZMK Discord**: https://zmk.dev/community/discord/invite

## 📄 License

MIT License - See [LICENSE](./LICENSE) for details

## 🙏 Acknowledgments

Built using:
- [ZMK Firmware](https://zmk.dev/)
- [react-zmk-studio](https://github.com/cormoran/react-zmk-studio)
- ZMK Module Template by [@cormoran](https://github.com/cormoran)

## Development Guide

### Setup

There are two west workspace layout options.

#### Option1: Download dependencies in parent directory

This option is west's standard way. Choose this option if you want to re-use dependent projects in other zephyr module development.

```bash
mkdir west-workspace
cd west-workspace # this directory becomes west workspace root (topdir)
git clone <this repository>
# rm -r .west # if exists to reset workspace
west init -l . --mf tests/west-test.yml
west update --narrow
west zephyr-export
```

The directory structure becomes like below:

```
west-workspace
  - .west/config
  - build : build output directory
  - <this repository>
  # other dependencies
  - zmk
  - zephyr
  - ...
  # You can develop other zephyr modules in this workspace
  - your-other-repo
```

You can switch between modules by removing `west-workspace/.west` and re-executing `west init ...`.

#### Option2: Download dependencies in ./dependencies (Enabled in dev-container)

Choose this option if you want to download dependencies under this directory (like node_modules in npm). This option is useful for specifying cache target in CI. The layout is relatively easy to recognize if you want to isolate dependencies.

```bash
git clone <this repository>
cd <cloned directory>
west init -l west --mf west-test-standalone.yml
# If you use dev container, start from below commands. Above commands are executed
# automatically.
west update --narrow
west zephyr-export
```

The directory structure becomes like below:

```
<this repository>
  - .west/config
  - build : build output directory
  - dependencies
    - zmk
    - zephyr
    - ...
```

### Dev container

Dev container is configured for setup option2. The container creates below volumes to re-use resources among containers.

- zmk-dependencies: dependencies dir for setup option2
- zmk-build: build output directory
- zmk-root-user: /root, the same to ZMK's official dev container

### Web UI

Please refer [./web/README.md](./web/README.md).

## Test

**ZMK firmware test**

`./tests` directory contains test config for posix to confirm module functionality and config for xiao board to confirm build works.

Tests can be executed by below command:

```bash
# Run all test case and verify results
python -m unittest
```

If you want to execute west command manually, run below. (for zmk-build, the result is not verified.)

```
# Build test firmware for xiao
# `-m tests/zmk-config .` means tests/zmk-config and this repo are added as additional zephyr module
west zmk-build tests/zmk-config/config -m tests/zmk-config .

# Run zmk test cases
# -m . is required to add this module to build
west zmk-test tests -m .
```

**Web UI test**

The `./web` directory includes Jest tests. See [./web/README.md](./web/README.md#testing) for more details.

```bash
cd web
npm test
```

## Publishing Web UI

Github actions are pre-configured to publish web UI to github pages.

1. Visit Settings>Pages
1. Set source as "Github Actions"
1. Visit Actions>"Test and Build Web UI"
1. Click "Run workflow"

Then, the Web UI will be available in
`https://<your github account>.github.io/<repository name>/` like https://cormoran.github.io/zmk-module-template-with-custom-studio-rpc.

## More Info

For more info on modules, you can read through through the
[Zephyr modules page](https://docs.zephyrproject.org/3.5.0/develop/modules.html)
and [ZMK's page on using modules](https://zmk.dev/docs/features/modules).
[Zephyr's west manifest page](https://docs.zephyrproject.org/3.5.0/develop/west/manifest.html#west-manifests)
may also be of use.
