/**
 * Tests for DiagnosticsSection component
 * 
 * This test file demonstrates testing the keyscan diagnostics functionality
 * including monitoring control and data visualization.
 */

import { render, screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import {
  createConnectedMockZMKApp,
  ZMKAppProvider,
} from "@cormoran/zmk-studio-react-hook/testing";
import { DiagnosticsSection, SUBSYSTEM_IDENTIFIER } from "../src/App";
import { Response } from "../src/proto/zmk/keyscan_diagnostics/custom";

describe("DiagnosticsSection Component", () => {
  it("should show warning when subsystem is not found", () => {
    const mockZMKApp = createConnectedMockZMKApp({
      deviceName: "Test Device",
      subsystems: [], // No subsystems
    });

    render(
      <ZMKAppProvider value={mockZMKApp}>
        <DiagnosticsSection />
      </ZMKAppProvider>
    );

    expect(
      screen.getByText(/Subsystem "zmk__keyscan_diagnostics" not found/i)
    ).toBeInTheDocument();
  });

  it("should render monitoring controls when subsystem is present", () => {
    const mockZMKApp = createConnectedMockZMKApp({
      deviceName: "Test Device",
      subsystems: [SUBSYSTEM_IDENTIFIER],
    });

    render(
      <ZMKAppProvider value={mockZMKApp}>
        <DiagnosticsSection />
      </ZMKAppProvider>
    );

    expect(screen.getByText(/Monitoring Control/i)).toBeInTheDocument();
    expect(screen.getByText(/Chattering Threshold/i)).toBeInTheDocument();
    expect(screen.getByText(/Start Monitoring/i)).toBeInTheDocument();
  });

  it("should call start monitoring RPC when start button is clicked", async () => {
    const mockZMKApp = createConnectedMockZMKApp({
      deviceName: "Test Device",
      subsystems: [SUBSYSTEM_IDENTIFIER],
    });

    // Mock successful start monitoring response
    const mockResponse = Response.encode({
      startMonitoring: {
        success: true,
        gpioCount: 4,
      },
    }).finish();

    mockZMKApp.mockRPCResponse(SUBSYSTEM_IDENTIFIER, mockResponse);

    render(
      <ZMKAppProvider value={mockZMKApp}>
        <DiagnosticsSection />
      </ZMKAppProvider>
    );

    const user = userEvent.setup();
    const startButton = screen.getByText(/Start Monitoring/i);
    await user.click(startButton);

    await waitFor(() => {
      expect(screen.getByText(/Stop Monitoring/i)).toBeInTheDocument();
    });
  });

  it("should display GPIO configuration when loaded", async () => {
    const mockZMKApp = createConnectedMockZMKApp({
      deviceName: "Test Device",
      subsystems: [SUBSYSTEM_IDENTIFIER],
    });

    // Mock GPIO config response
    const mockResponse = Response.encode({
      getGpioConfig: {
        gpioPins: [
          { pin: 0, portName: "P0" },
          { pin: 1, portName: "P0" },
          { pin: 2, portName: "P0" },
          { pin: 3, portName: "P0" },
        ],
        matrixRows: 4,
        matrixCols: 4,
      },
    }).finish();

    mockZMKApp.mockRPCResponse(SUBSYSTEM_IDENTIFIER, mockResponse);

    render(
      <ZMKAppProvider value={mockZMKApp}>
        <DiagnosticsSection />
      </ZMKAppProvider>
    );

    await waitFor(() => {
      expect(screen.getByText(/GPIO Configuration/i)).toBeInTheDocument();
      expect(screen.getByText(/4 rows × 4 columns/i)).toBeInTheDocument();
    });
  });

  it("should display chattering stats when available", async () => {
    const mockZMKApp = createConnectedMockZMKApp({
      deviceName: "Test Device",
      subsystems: [SUBSYSTEM_IDENTIFIER],
    });

    // Mock state with chattering stats
    const mockResponse = Response.encode({
      getState: {
        monitoringActive: true,
        totalEvents: 10,
        recentEvents: [],
        chatterStats: [
          {
            row: 0,
            col: 1,
            eventCount: 10,
            chatterCount: 5,
            lastEventMs: BigInt(1000),
            minIntervalMs: 20,
          },
        ],
        gpioPins: [],
      },
    }).finish();

    mockZMKApp.mockRPCResponse(SUBSYSTEM_IDENTIFIER, mockResponse);

    render(
      <ZMKAppProvider value={mockZMKApp}>
        <DiagnosticsSection />
      </ZMKAppProvider>
    );

    await waitFor(() => {
      expect(screen.getByText(/Chattering Detection/i)).toBeInTheDocument();
      expect(screen.getByText(/Row 0, Col 1/i)).toBeInTheDocument();
    });
  });

  it("should display recent events when available", async () => {
    const mockZMKApp = createConnectedMockZMKApp({
      deviceName: "Test Device",
      subsystems: [SUBSYSTEM_IDENTIFIER],
    });

    // Mock state with events
    const mockResponse = Response.encode({
      getState: {
        monitoringActive: true,
        totalEvents: 2,
        recentEvents: [
          {
            row: 0,
            col: 0,
            pressed: true,
            timestampMs: BigInt(100),
          },
          {
            row: 0,
            col: 0,
            pressed: false,
            timestampMs: BigInt(200),
          },
        ],
        chatterStats: [],
        gpioPins: [],
      },
    }).finish();

    mockZMKApp.mockRPCResponse(SUBSYSTEM_IDENTIFIER, mockResponse);

    render(
      <ZMKAppProvider value={mockZMKApp}>
        <DiagnosticsSection />
      </ZMKAppProvider>
    );

    await waitFor(() => {
      expect(screen.getByText(/Recent Events/i)).toBeInTheDocument();
      expect(screen.getByText(/Press/i)).toBeInTheDocument();
      expect(screen.getByText(/Release/i)).toBeInTheDocument();
    });
  });
});
