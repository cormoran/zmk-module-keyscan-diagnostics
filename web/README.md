# ZMK Keyscan Diagnostics - Web Frontend

Interactive web application for diagnosing keyboard switch and soldering issues
using ZMK firmware's keyscan diagnostics module.

## Features

- **Device Connection**: Connect to ZMK devices via USB Serial
- **Real-time Key Monitoring**: See key presses as they happen
- **Chattering Detection**: Automatic alerts for keys with poor solder joints
- **GPIO Visualization**: View pin configuration with Xiao board pinout
- **Troubleshooting Guide**: Built-in help for common issues

## Quick Start

```bash
# Install dependencies
npm install

# Generate TypeScript types from proto
npm run generate

# Run development server
npm run dev

# Build for production
npm run build

# Run tests
npm test
```

## Project Structure

```
src/
├── main.tsx              # React entry point
├── App.tsx               # Main application with diagnostics UI
├── App.css               # Styles
└── proto/                # Generated protobuf TypeScript types
    └── zmk/keyscan_diagnostics/
        └── diagnostics.ts

test/
├── App.spec.tsx              # Tests for App component
└── DiagnosticsPanel.spec.tsx # Tests for diagnostics functionality
```

## How It Works

### 1. Protocol Definition

The protobuf schema is defined in `../proto/zmk/keyscan_diagnostics/diagnostics.proto`:

```proto
message Request {
    oneof request_type {
        GetKscanConfigRequest get_kscan_config = 1;
        GetKeyMatrixRequest get_key_matrix = 2;
        StartMonitoringRequest start_monitoring = 3;
        StopMonitoringRequest stop_monitoring = 4;
        GetEventsRequest get_events = 5;
        // ... more request types
    }
}
```

### 2. Code Generation

TypeScript types are generated using `ts-proto`:

```bash
npm run generate
```

### 3. Using react-zmk-studio

The app uses the `@cormoran/zmk-studio-react-hook` library:

```typescript
import { ZMKConnection, ZMKCustomSubsystem } from "@cormoran/zmk-studio-react-hook";

// Connect and make RPC calls
const service = new ZMKCustomSubsystem(connection, subsystemIndex);
const response = await service.callRPC(Request.encode(request).finish());
const decoded = Response.decode(response);
```

## Testing

```bash
# Run all tests
npm test

# Run tests in watch mode
npm run test:watch

# Run tests with coverage
npm run test:coverage
```

## UI Components

### Key Matrix Display
Visual grid showing all key positions with color-coded status:
- Green: Currently pressed
- Blue: Recent activity
- Orange (pulsing): Chattering detected
- Gray: Idle

### Event Log
Chronological list of key events with:
- Timestamp
- Position (row, column)
- Action (press/release)
- GPIO pin information

### GPIO Pin Display
- List of all GPIO pins with port and pin numbers
- Visual Xiao board pinout showing which pins are used
- Polarity information (active low/high)

### Chattering Alerts
Prominent warning when chattering is detected showing:
- Affected key position
- GPIO pins involved
- Event count and duration
- Suggested fixes

## Development Notes

- Connection state is managed by the `useZMKApp` hook
- RPC calls are made through `ZMKCustomSubsystem`
- Polling for events uses `setInterval` with 500ms interval
- Events are buffered (last 100) for display

## See Also

- [Main README](../README.md) - Full module documentation
- [react-zmk-studio](https://github.com/cormoran/react-zmk-studio) - React hooks library
