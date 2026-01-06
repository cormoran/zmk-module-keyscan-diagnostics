/**
 * ZMK Keyscan Diagnostics - Main Application
 * Interactive web UI for debugging keyboard matrix issues
 */

import { useContext, useState, useEffect } from "react";
import "./App.css";
import { connect as serial_connect } from "@zmkfirmware/zmk-studio-ts-client/transport/serial";
import {
  ZMKConnection,
  ZMKCustomSubsystem,
  ZMKAppContext,
} from "@cormoran/zmk-studio-react-hook";
import {
  Request,
  Response,
  GetStateResponse,
  GetGPIOConfigResponse,
  ChatteringStats,
  KeyEvent,
} from "./proto/zmk/keyscan_diagnostics/custom";

export const SUBSYSTEM_IDENTIFIER = "zmk__keyscan_diagnostics";

function App() {
  return (
    <div className="app">
      <header className="app-header">
        <h1>🔧 ZMK Keyscan Diagnostics</h1>
        <p>Interactive Matrix Debugging Tool</p>
      </header>

      <ZMKConnection
        renderDisconnected={({ connect, isLoading, error }) => (
          <section className="card">
            <h2>Device Connection</h2>
            {isLoading && <p>⏳ Connecting...</p>}
            {error && (
              <div className="error-message">
                <p>🚨 {error}</p>
              </div>
            )}
            {!isLoading && (
              <button
                className="btn btn-primary"
                onClick={() => connect(serial_connect)}
              >
                🔌 Connect Serial
              </button>
            )}
          </section>
        )}
        renderConnected={({ disconnect, deviceName }) => (
          <>
            <section className="card">
              <h2>Device Connection</h2>
              <div className="device-info">
                <h3>✅ Connected to: {deviceName}</h3>
              </div>
              <button className="btn btn-secondary" onClick={disconnect}>
                Disconnect
              </button>
            </section>

            <DiagnosticsSection />
          </>
        )}
      />

      <footer className="app-footer">
        <p>
          <strong>Keyscan Diagnostics Module</strong> - Identify soldering
          issues and chattering
        </p>
      </footer>
    </div>
  );
}

export function DiagnosticsSection() {
  const zmkApp = useContext(ZMKAppContext);
  const [monitoring, setMonitoring] = useState(false);
  const [gpioConfig, setGpioConfig] = useState<GetGPIOConfigResponse | null>(
    null
  );
  const [state, setState] = useState<GetStateResponse | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [chatterThreshold, setChatterThreshold] = useState(50);

  if (!zmkApp) return null;

  const subsystem = zmkApp.findSubsystem(SUBSYSTEM_IDENTIFIER);

  const callRPC = async (request: Request): Promise<Response | null> => {
    if (!zmkApp.state.connection || !subsystem) return null;

    try {
      const service = new ZMKCustomSubsystem(
        zmkApp.state.connection,
        subsystem.index
      );
      const payload = Request.encode(request).finish();
      const responsePayload = await service.callRPC(payload);
      if (responsePayload) {
        return Response.decode(responsePayload);
      }
    } catch (error) {
      console.error("RPC call failed:", error);
    }
    return null;
  };

  const loadGPIOConfig = async () => {
    const resp = await callRPC({
      getGpioConfig: {},
    });
    if (resp?.getGpioConfig) {
      setGpioConfig(resp.getGpioConfig);
    }
  };

  const loadState = async () => {
    const resp = await callRPC({
      getState: {},
    });
    if (resp?.getState) {
      setState(resp.getState);
      setMonitoring(resp.getState.monitoringActive);
    }
  };

  const startMonitoring = async () => {
    setIsLoading(true);
    const resp = await callRPC({
      startMonitoring: {
        enableEventStreaming: true,
        chatterThresholdMs: chatterThreshold,
      },
    });
    if (resp?.startMonitoring?.success) {
      setMonitoring(true);
      await loadState();
    }
    setIsLoading(false);
  };

  const stopMonitoring = async () => {
    setIsLoading(true);
    const resp = await callRPC({
      stopMonitoring: {},
    });
    if (resp?.stopMonitoring?.success) {
      setMonitoring(false);
      await loadState();
    }
    setIsLoading(false);
  };

  const clearData = async () => {
    setIsLoading(true);
    await callRPC({
      clearData: {},
    });
    await loadState();
    setIsLoading(false);
  };

  // Load initial data
  useEffect(() => {
    loadGPIOConfig();
    loadState();
  }, []);

  // Poll for updates when monitoring
  useEffect(() => {
    if (!monitoring) return;

    const interval = setInterval(() => {
      loadState();
    }, 1000);

    return () => clearInterval(interval);
  }, [monitoring]);

  if (!subsystem) {
    return (
      <section className="card">
        <div className="warning-message">
          <p>
            ⚠️ Subsystem "{SUBSYSTEM_IDENTIFIER}" not found. Make sure your
            firmware includes the keyscan diagnostics module.
          </p>
        </div>
      </section>
    );
  }

  return (
    <>
      <section className="card">
        <h2>Monitoring Control</h2>
        <div className="control-group">
          <div className="input-group">
            <label htmlFor="chatter-threshold">
              Chattering Threshold (ms):
            </label>
            <input
              id="chatter-threshold"
              type="number"
              value={chatterThreshold}
              onChange={(e) => setChatterThreshold(parseInt(e.target.value))}
              disabled={monitoring}
              min="10"
              max="500"
            />
          </div>
          <div className="button-group">
            {!monitoring ? (
              <button
                className="btn btn-primary"
                onClick={startMonitoring}
                disabled={isLoading}
              >
                {isLoading ? "⏳ Starting..." : "▶️ Start Monitoring"}
              </button>
            ) : (
              <button
                className="btn btn-secondary"
                onClick={stopMonitoring}
                disabled={isLoading}
              >
                {isLoading ? "⏳ Stopping..." : "⏸️ Stop Monitoring"}
              </button>
            )}
            <button
              className="btn btn-danger"
              onClick={clearData}
              disabled={isLoading}
            >
              🗑️ Clear Data
            </button>
            <button className="btn" onClick={loadState} disabled={isLoading}>
              🔄 Refresh
            </button>
          </div>
        </div>
        {state && (
          <div className="stats-summary">
            <p>
              <strong>Status:</strong>{" "}
              {state.monitoringActive ? "🟢 Monitoring" : "🔴 Stopped"}
            </p>
            <p>
              <strong>Total Events:</strong> {state.totalEvents}
            </p>
          </div>
        )}
      </section>

      {gpioConfig && (
        <section className="card">
          <h2>GPIO Configuration</h2>
          <div className="gpio-info">
            <p>
              <strong>Matrix Size:</strong> {gpioConfig.matrixRows} rows ×{" "}
              {gpioConfig.matrixCols} columns
            </p>
            <p>
              <strong>GPIO Pins:</strong> {gpioConfig.gpioPins.length}
            </p>
            <div className="gpio-list">
              {gpioConfig.gpioPins.map((pin, idx) => (
                <div key={idx} className="gpio-pin">
                  <span className="pin-label">P{idx}</span>
                  <span className="pin-info">
                    {pin.portName}.{pin.pin}
                  </span>
                </div>
              ))}
            </div>
          </div>
        </section>
      )}

      {state && state.chatterStats.length > 0 && (
        <section className="card">
          <h2>⚠️ Chattering Detection</h2>
          <div className="warning-message">
            <p>
              Keys with chattering detected (may indicate insufficient
              soldering):
            </p>
          </div>
          <ChatteringStatsTable stats={state.chatterStats} />
        </section>
      )}

      {state && state.recentEvents.length > 0 && (
        <section className="card">
          <h2>Recent Events</h2>
          <EventsTable events={state.recentEvents} />
        </section>
      )}
    </>
  );
}

function ChatteringStatsTable({ stats }: { stats: ChatteringStats[] }) {
  const problematicKeys = stats.filter((s) => s.chatterCount > 0);

  return (
    <div className="table-container">
      <table>
        <thead>
          <tr>
            <th>Position</th>
            <th>Total Events</th>
            <th>Chatter Count</th>
            <th>Min Interval</th>
            <th>Status</th>
          </tr>
        </thead>
        <tbody>
          {problematicKeys.map((stat, idx) => (
            <tr
              key={idx}
              className={stat.chatterCount > 5 ? "error-row" : "warning-row"}
            >
              <td>
                Row {stat.row}, Col {stat.col}
              </td>
              <td>{stat.eventCount}</td>
              <td>{stat.chatterCount}</td>
              <td>{stat.minIntervalMs}ms</td>
              <td>
                {stat.chatterCount > 10
                  ? "🔴 Critical"
                  : stat.chatterCount > 5
                    ? "🟡 Warning"
                    : "🟢 Minor"}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

function EventsTable({ events }: { events: KeyEvent[] }) {
  return (
    <div className="table-container">
      <table>
        <thead>
          <tr>
            <th>Time</th>
            <th>Position</th>
            <th>Action</th>
          </tr>
        </thead>
        <tbody>
          {events
            .slice()
            .reverse()
            .map((event, idx) => (
              <tr key={idx}>
                <td>{event.timestampMs}ms</td>
                <td>
                  Row {event.row}, Col {event.col}
                </td>
                <td className={event.pressed ? "press-event" : "release-event"}>
                  {event.pressed ? "⬇️ Press" : "⬆️ Release"}
                </td>
              </tr>
            ))}
        </tbody>
      </table>
    </div>
  );
}

export default App;
