/**
 * Tests for DiagnosticsPanel component
 * 
 * This test demonstrates how to use react-zmk-studio test helpers to test
 * components that interact with ZMK devices.
 */

import { render, screen } from "@testing-library/react";
import {
  createConnectedMockZMKApp,
  ZMKAppProvider,
} from "@cormoran/zmk-studio-react-hook/testing";
import { DiagnosticsPanel, SUBSYSTEM_IDENTIFIER } from "../src/App";

describe("DiagnosticsPanel Component", () => {
  describe("With Subsystem", () => {
    it("should render diagnostics panel when subsystem is found", () => {
      // Create a connected mock ZMK app with the required subsystem
      const mockZMKApp = createConnectedMockZMKApp({
        deviceName: "Test Device",
        subsystems: [SUBSYSTEM_IDENTIFIER],
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <DiagnosticsPanel />
        </ZMKAppProvider>
      );

      // Check for diagnostics UI elements
      expect(screen.getByText(/Key Monitoring/i)).toBeInTheDocument();
      expect(screen.getByText(/Start Monitoring/i)).toBeInTheDocument();
    });

    it("should show tab navigation", () => {
      const mockZMKApp = createConnectedMockZMKApp({
        subsystems: [SUBSYSTEM_IDENTIFIER],
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <DiagnosticsPanel />
        </ZMKAppProvider>
      );

      // Check for tab buttons
      expect(screen.getByText(/Key Matrix/i)).toBeInTheDocument();
      expect(screen.getByText(/Event Log/i)).toBeInTheDocument();
      expect(screen.getByText(/GPIO Pins/i)).toBeInTheDocument();
    });

    it("should show troubleshooting guide", () => {
      const mockZMKApp = createConnectedMockZMKApp({
        subsystems: [SUBSYSTEM_IDENTIFIER],
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <DiagnosticsPanel />
        </ZMKAppProvider>
      );

      // Check for troubleshooting section
      expect(screen.getByText(/Troubleshooting Guide/i)).toBeInTheDocument();
      expect(screen.getByText(/Key doesn't register at all/i)).toBeInTheDocument();
    });
  });

  describe("Without Subsystem", () => {
    it("should show warning when subsystem is not found", () => {
      // Create a connected mock ZMK app without the required subsystem
      const mockZMKApp = createConnectedMockZMKApp({
        deviceName: "Test Device",
        subsystems: [], // No subsystems
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <DiagnosticsPanel />
        </ZMKAppProvider>
      );

      // Check for warning message
      expect(screen.getByText(/Subsystem "zmk__keyscan_diagnostics" not found/i)).toBeInTheDocument();
      expect(screen.getByText(/CONFIG_ZMK_KEYSCAN_DIAGNOSTICS=y/i)).toBeInTheDocument();
    });
  });

  describe("Without ZMKAppContext", () => {
    it("should not render when ZMKAppContext is not provided", () => {
      const { container } = render(<DiagnosticsPanel />);

      // Component should return null when context is not available
      expect(container.firstChild).toBeNull();
    });
  });
});
