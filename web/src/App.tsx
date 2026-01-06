import { useContext, useEffect, useMemo, useState } from "react";
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
  KeyStatus,
  SnapshotResponse,
} from "./proto/zmk/keyscan/diagnostics";

// Custom subsystem identifier - must match firmware registration
export const SUBSYSTEM_IDENTIFIER = "zmk__keyscan_diag";

function App() {
  return (
    <div className="app">
      <header className="app-header">
        <h1>🧭 Keyscan Diagnostics</h1>
        <p>Find stuck, missing, or chattering keys with live Studio data.</p>
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

            <RPCTestSection />
          </>
        )}
      />

      <footer className="app-footer">
        <p>
          <strong>Keyscan diagnostics</strong> • Charlieplex-first, extensible
          for other matrices.
        </p>
      </footer>
    </div>
  );
}

function keyClassname(key: KeyStatus) {
  if (key.chatterCount > 0) return "status-chatter";
  if (key.neverSeen) return "status-missing";
  if (key.pressed) return "status-pressed";
  return "";
}

function KeyGrid({ snapshot }: { snapshot: SnapshotResponse }) {
  const positionedKeys = snapshot.keys.filter(
    (k) => k.shape && (k.shape.width || k.shape.height)
  );

  if (positionedKeys.length === 0) {
    return (
      <div className="key-grid key-grid--list">
        {snapshot.keys.map((key) => (
          <div key={key.position} className={`key-chip ${keyClassname(key)}`}>
            <div className="key-label">Key {key.position}</div>
            <div className="key-detail">
              {key.chatterCount > 0
                ? `${key.chatterCount} chatter`
                : `${key.pressCount} presses`}
            </div>
          </div>
        ))}
      </div>
    );
  }

  const xs = positionedKeys.map((k) => k.shape?.x ?? 0);
  const ys = positionedKeys.map((k) => k.shape?.y ?? 0);
  const widths = positionedKeys.map((k) => k.shape?.width ?? 80);
  const heights = positionedKeys.map((k) => k.shape?.height ?? 80);
  const minX = Math.min(...xs);
  const minY = Math.min(...ys);
  const maxX = Math.max(...xs.map((x, i) => x + (widths[i] ?? 0)));
  const maxY = Math.max(...ys.map((y, i) => y + (heights[i] ?? 0)));
  const layoutWidth = maxX - minX;
  const layoutHeight = maxY - minY;
  const scale =
    Math.max(layoutWidth / 560, layoutHeight / 320, 1) || 1;

  const toPx = (v?: number) => `${((v ?? 0) - minX) / scale}px`;

  return (
    <div
      className="key-grid key-grid--layout"
      style={{
        width: `${layoutWidth / scale}px`,
        height: `${layoutHeight / scale}px`,
      }}
    >
      {positionedKeys.map((key) => (
        <div
          key={key.position}
          className={`key-chip ${keyClassname(key)}`}
          style={{
            left: toPx(key.shape?.x),
            top: toPx(key.shape?.y),
            width: `${(key.shape?.width ?? 80) / scale}px`,
            height: `${(key.shape?.height ?? 80) / scale}px`,
          }}
          title={`Drive ${key.lineDrive ?? "-"}, Sense ${key.lineSense ?? "-"}`}
        >
          <div className="key-label">Key {key.position}</div>
          <div className="key-detail">
            {key.chatterCount > 0
              ? `${key.chatterCount} chatter`
              : `${key.pressCount} presses`}
          </div>
        </div>
      ))}
    </div>
  );
}

export function RPCTestSection() {
  const zmkApp = useContext(ZMKAppContext);
  const [snapshot, setSnapshot] = useState<SnapshotResponse | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  if (!zmkApp) return null;

  const subsystem = zmkApp.findSubsystem(SUBSYSTEM_IDENTIFIER);

  const fetchSnapshot = async (resetCounters = false) => {
    if (!zmkApp.state.connection || !subsystem) return;
    setIsLoading(true);
    setError(null);

    try {
      const service = new ZMKCustomSubsystem(
        zmkApp.state.connection,
        subsystem.index
      );

      const request = Request.create({
        snapshot: { resetCounters },
      });

      const payload = Request.encode(request).finish();
      const responsePayload = await service.callRPC(payload);

      if (responsePayload) {
        const resp = Response.decode(responsePayload);

        if (resp.snapshot) {
          setSnapshot(resp.snapshot);
        } else if (resp.error) {
          setError(resp.error.message);
        }
      }
    } catch (err) {
      console.error("RPC call failed:", err);
      setError(
        err instanceof Error ? err.message : "Unknown error requesting snapshot"
      );
    } finally {
      setIsLoading(false);
    }
  };

  useEffect(() => {
    if (subsystem && zmkApp.state.connection) {
      fetchSnapshot(false);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [subsystem, zmkApp.state.connection]);

  if (!subsystem) {
    return (
      <section className="card">
        <div className="warning-message">
          <p>
            ⚠️ Subsystem "{SUBSYSTEM_IDENTIFIER}" not found. Make sure your
            firmware includes the diagnostics module.
          </p>
        </div>
      </section>
    );
  }

  const chatteringKeys =
    snapshot?.keys.filter((k) => k.chatterCount > 0).length ?? 0;
  const missingKeys =
    snapshot?.keys.filter((k) => k.neverSeen).length ?? 0;
  const suspectLines =
    snapshot?.lines?.filter((l) => l.suspectedFault).length ?? 0;

  return (
    <section className="card">
      <div className="section-header">
        <div>
          <h2>Keyscan Diagnostics</h2>
          <p>
            Live snapshot of key transitions, chattering bursts, and Charlieplex
            line usage.
          </p>
        </div>
        <div className="button-group">
          <button
            className="btn btn-secondary"
            disabled={isLoading}
            onClick={() => fetchSnapshot(false)}
          >
            🔄 Refresh
          </button>
          <button
            className="btn btn-primary"
            disabled={isLoading}
            onClick={() => fetchSnapshot(true)}
          >
            ♻️ Refresh & Reset
          </button>
        </div>
      </div>

      {isLoading && <p>Collecting keyscan snapshot...</p>}
      {error && <div className="error-message">🚨 {error}</div>}

      {snapshot ? (
        <>
          <div className="stats-row">
            <div className="stat-pill">
              <div className="stat-value">{snapshot.keys.length}</div>
              <div className="stat-label">Keys tracked</div>
            </div>
            <div className="stat-pill">
              <div className="stat-value">{chatteringKeys}</div>
              <div className="stat-label">Keys with chatter</div>
            </div>
            <div className="stat-pill">
              <div className="stat-value">{missingKeys}</div>
              <div className="stat-label">Never seen</div>
            </div>
            <div className="stat-pill">
              <div className="stat-value">{suspectLines}</div>
              <div className="stat-label">Suspected lines</div>
            </div>
          </div>

          <KeyGrid snapshot={snapshot} />

          {snapshot.lines && snapshot.lines.length > 0 && (
            <div className="line-table">
              <div className="line-table__header">
                <span>Line</span>
                <span>Port/Pin</span>
                <span>Activity</span>
                <span>Keys</span>
                <span>Status</span>
              </div>
              {snapshot.lines.map((line) => (
                <div key={line.index} className="line-table__row">
                  <span>#{line.index}</span>
                  <span>
                    {line.port || "?"}:{line.pin}
                  </span>
                  <span>{line.activity}</span>
                  <span>
                    {line.involvedKeys} / chatter {line.chatterKeys}
                  </span>
                  <span className={line.suspectedFault ? "badge danger" : "badge"}>
                    {line.suspectedFault ? "Investigate" : "Normal"}
                  </span>
                </div>
              ))}
            </div>
          )}
        </>
      ) : (
        <div className="warning-message">
          <p>
            No snapshot yet. Refresh to pull diagnostics from the firmware and
            highlight noisy or silent switches.
          </p>
        </div>
      )}
    </section>
  );
}

export default App;
