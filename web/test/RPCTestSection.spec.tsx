import { render, screen, waitFor } from "@testing-library/react";
import {
  createConnectedMockZMKApp,
  ZMKAppProvider,
} from "@cormoran/zmk-studio-react-hook/testing";
import { ZMKCustomSubsystem } from "@cormoran/zmk-studio-react-hook";
import { RPCTestSection, SUBSYSTEM_IDENTIFIER } from "../src/App";
import { Response } from "../src/proto/zmk/keyscan/diagnostics";

describe("RPCTestSection Component", () => {
  const encodedSnapshot = Response.encode({
    snapshot: {
      keys: [
        {
          position: 0,
          pressCount: 3,
          releaseCount: 2,
          chatterCount: 1,
          lineDrive: 0,
          lineSense: 1,
          shape: { x: 0, y: 0, width: 100, height: 100 },
        },
        {
          position: 1,
          pressCount: 0,
          releaseCount: 0,
          chatterCount: 0,
          neverSeen: true,
          lineDrive: 1,
          lineSense: 2,
          shape: { x: 120, y: 0, width: 100, height: 100 },
        },
      ],
      lines: [
        {
          index: 0,
          port: "P0",
          pin: 1,
          activity: 4,
          involvedKeys: 2,
          chatterKeys: 1,
          suspectedFault: false,
        },
        {
          index: 1,
          port: "P0",
          pin: 2,
          activity: 0,
          involvedKeys: 1,
          chatterKeys: 0,
          suspectedFault: true,
        },
      ],
      chatterBurstThreshold: 3,
      chatterWindowMs: 30,
    },
  }).finish();

  beforeAll(() => {
    jest
      .spyOn(ZMKCustomSubsystem.prototype, "callRPC")
      .mockResolvedValue(encodedSnapshot);
  });

  describe("With Subsystem", () => {
    it("should render diagnostics summary and grid when subsystem is found", async () => {
      const mockZMKApp = createConnectedMockZMKApp({
        deviceName: "Test Device",
        subsystems: [SUBSYSTEM_IDENTIFIER],
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <RPCTestSection />
        </ZMKAppProvider>
      );

      await waitFor(() =>
        expect(screen.getByText(/Keys tracked/i)).toBeInTheDocument()
      );
      expect(screen.getByText(/Keys with chatter/i)).toBeInTheDocument();
      expect(screen.getByText(/Never seen/i)).toBeInTheDocument();
      expect(screen.getByText(/Investigate/i)).toBeInTheDocument();
    });
  });

  describe("Without Subsystem", () => {
    it("should show warning when subsystem is not found", () => {
      const mockZMKApp = createConnectedMockZMKApp({
        deviceName: "Test Device",
        subsystems: [], // No subsystems
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <RPCTestSection />
        </ZMKAppProvider>
      );

      expect(
        screen.getByText(/Subsystem "zmk__keyscan_diag" not found/i)
      ).toBeInTheDocument();
      expect(
        screen.getByText(/includes the diagnostics module/i)
      ).toBeInTheDocument();
    });
  });

  describe("Without ZMKAppContext", () => {
    it("should not render when ZMKAppContext is not provided", () => {
      const { container } = render(<RPCTestSection />);
      expect(container.firstChild).toBeNull();
    });
  });
});
