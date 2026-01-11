/**
 * ZMK Keyscan Diagnostics - Main Application
 * Interactive diagnostic tool for keyboard switch and soldering issues
 */

import { useContext, useState, useEffect, useCallback } from "react";
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
  KscanType,
  KscanConfig,
  GpioPin,
  KeyInfo,
  KeyEvent,
  ChatteringAlert,
  GetKeyMatrixResponse,
} from "./proto/zmk/keyscan_diagnostics/diagnostics";

// Custom subsystem identifier - must match firmware registration
export const SUBSYSTEM_IDENTIFIER = "zmk__keyscan_diagnostics";

// Polling interval for events (ms)
const POLL_INTERVAL = 500;

function App() {
  return (
    <div className="app">
      <header className="app-header">
        <h1>🔧 ZMK Keyscan Diagnostics</h1>
        <p>Diagnose keyboard switch and soldering issues</p>
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
            <div className="info-box">
              <h3>📋 Requirements</h3>
              <ul>
                <li>Your keyboard must have ZMK firmware with the keyscan diagnostics module enabled</li>
                <li>Connect via USB serial</li>
                <li>Enable <code>CONFIG_ZMK_KEYSCAN_DIAGNOSTICS=y</code> and <code>CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_STUDIO_RPC=y</code></li>
              </ul>
            </div>
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

            <DiagnosticsPanel />
          </>
        )}
      />

      <footer className="app-footer">
        <p>
          <strong>Keyscan Diagnostics Module</strong> - Identify soldering and switch issues
        </p>
      </footer>
    </div>
  );
}

/**
 * Custom hook for RPC communication with the diagnostics subsystem
 */
function useDiagnosticsRPC() {
  const zmkApp = useContext(ZMKAppContext);
  const subsystem = zmkApp?.findSubsystem(SUBSYSTEM_IDENTIFIER);

  const sendRequest = useCallback(async (request: Partial<Request>): Promise<Response | null> => {
    if (!zmkApp?.state.connection || !subsystem) return null;

    try {
      const service = new ZMKCustomSubsystem(
        zmkApp.state.connection,
        subsystem.index
      );

      const req = Request.create(request);
      const payload = Request.encode(req).finish();
      const responsePayload = await service.callRPC(payload);

      if (responsePayload) {
        return Response.decode(responsePayload);
      }
    } catch (error) {
      console.error("RPC call failed:", error);
    }
    return null;
  }, [zmkApp, subsystem]);

  return { sendRequest, isAvailable: !!subsystem };
}

/**
 * Main diagnostics panel
 */
export function DiagnosticsPanel() {
  const zmkApp = useContext(ZMKAppContext);
  const { sendRequest, isAvailable } = useDiagnosticsRPC();
  
  const [kscanConfig, setKscanConfig] = useState<KscanConfig | null>(null);
  const [keyMatrix, setKeyMatrix] = useState<GetKeyMatrixResponse | null>(null);
  const [events, setEvents] = useState<KeyEvent[]>([]);
  const [chatteringAlerts, setChatteringAlerts] = useState<ChatteringAlert[]>([]);
  const [isMonitoring, setIsMonitoring] = useState(false);
  const [totalEvents, setTotalEvents] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [activeTab, setActiveTab] = useState<'matrix' | 'events' | 'gpio'>('matrix');

  // Load initial config
  useEffect(() => {
    if (!isAvailable) return;
    
    const loadConfig = async () => {
      const resp = await sendRequest({ getKscanConfig: {} });
      if (resp?.kscanConfig) {
        setKscanConfig(resp.kscanConfig);
      } else if (resp?.error) {
        setError(resp.error.message);
      }
    };
    
    loadConfig();
  }, [isAvailable, sendRequest]);

  // Poll for events while monitoring
  useEffect(() => {
    if (!isMonitoring || !isAvailable) return;

    const pollEvents = async () => {
      // Get events
      const eventsResp = await sendRequest({ getEvents: { clearBuffer: true } });
      if (eventsResp?.events?.events) {
        setEvents(prev => [...prev, ...eventsResp.events!.events].slice(-100));
        setTotalEvents(eventsResp.events.totalEvents);
      }

      // Get key matrix state
      const matrixResp = await sendRequest({ getKeyMatrix: {} });
      if (matrixResp?.keyMatrix) {
        setKeyMatrix(matrixResp.keyMatrix);
      }

      // Get chattering alerts
      const alertsResp = await sendRequest({ getChatteringAlerts: { clearAlerts: false } });
      if (alertsResp?.chatteringAlerts?.alerts) {
        setChatteringAlerts(alertsResp.chatteringAlerts.alerts);
      }
    };

    const intervalId = setInterval(pollEvents, POLL_INTERVAL);
    pollEvents(); // Initial poll

    return () => clearInterval(intervalId);
  }, [isMonitoring, isAvailable, sendRequest]);

  const handleStartMonitoring = async () => {
    const resp = await sendRequest({ startMonitoring: { maxEvents: 0 } });
    if (resp?.startMonitoring?.success) {
      setIsMonitoring(true);
      setEvents([]);
      setTotalEvents(0);
      setChatteringAlerts([]);
      setError(null);
    } else if (resp?.startMonitoring?.message) {
      setError(resp.startMonitoring.message);
    }
  };

  const handleStopMonitoring = async () => {
    await sendRequest({ stopMonitoring: {} });
    setIsMonitoring(false);
  };

  if (!zmkApp) return null;

  if (!isAvailable) {
    return (
      <section className="card">
        <div className="warning-message">
          <p>
            ⚠️ Subsystem "{SUBSYSTEM_IDENTIFIER}" not found. Make sure your
            firmware includes the keyscan diagnostics module with:
          </p>
          <ul>
            <li><code>CONFIG_ZMK_KEYSCAN_DIAGNOSTICS=y</code></li>
            <li><code>CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_STUDIO_RPC=y</code></li>
          </ul>
        </div>
      </section>
    );
  }

  return (
    <>
      {error && (
        <section className="card error-card">
          <div className="error-message">
            <p>🚨 {error}</p>
            <button className="btn btn-secondary" onClick={() => setError(null)}>
              Dismiss
            </button>
          </div>
        </section>
      )}

      {/* Keyscan Type Info */}
      {kscanConfig && (
        <section className="card">
          <h2>⌨️ Keyscan Configuration</h2>
          <KscanConfigDisplay config={kscanConfig} />
        </section>
      )}

      {/* Chattering Alerts */}
      {chatteringAlerts.length > 0 && (
        <section className="card alert-card">
          <h2>⚠️ Chattering Detected!</h2>
          <ChatteringAlertsDisplay alerts={chatteringAlerts} config={kscanConfig} />
        </section>
      )}

      {/* Monitoring Controls */}
      <section className="card">
        <h2>📊 Key Monitoring</h2>
        <div className="monitoring-controls">
          {!isMonitoring ? (
            <button className="btn btn-primary" onClick={handleStartMonitoring}>
              ▶️ Start Monitoring
            </button>
          ) : (
            <button className="btn btn-danger" onClick={handleStopMonitoring}>
              ⏹️ Stop Monitoring
            </button>
          )}
          <span className="event-count">
            Total Events: <strong>{totalEvents}</strong>
          </span>
        </div>

        {/* Tab Navigation */}
        <div className="tab-nav">
          <button 
            className={`tab-btn ${activeTab === 'matrix' ? 'active' : ''}`}
            onClick={() => setActiveTab('matrix')}
          >
            Key Matrix
          </button>
          <button 
            className={`tab-btn ${activeTab === 'events' ? 'active' : ''}`}
            onClick={() => setActiveTab('events')}
          >
            Event Log
          </button>
          <button 
            className={`tab-btn ${activeTab === 'gpio' ? 'active' : ''}`}
            onClick={() => setActiveTab('gpio')}
          >
            GPIO Pins
          </button>
        </div>

        {/* Tab Content */}
        {activeTab === 'matrix' && keyMatrix && (
          <KeyMatrixDisplay 
            matrix={keyMatrix} 
            events={events}
            chatteringAlerts={chatteringAlerts}
          />
        )}
        {activeTab === 'events' && (
          <EventLogDisplay events={events} config={kscanConfig} />
        )}
        {activeTab === 'gpio' && kscanConfig?.charlieplex && (
          <GpioPinDisplay gpios={kscanConfig.charlieplex.gpios} />
        )}
      </section>

      {/* Help Section */}
      <section className="card">
        <h2>💡 Troubleshooting Guide</h2>
        <TroubleshootingGuide />
      </section>
    </>
  );
}

/**
 * Display keyscan configuration
 */
function KscanConfigDisplay({ config }: { config: KscanConfig }) {
  const getTypeName = (type: KscanType) => {
    switch (type) {
      case KscanType.KSCAN_TYPE_CHARLIEPLEX: return "Charlieplex Matrix";
      case KscanType.KSCAN_TYPE_MATRIX: return "GPIO Matrix";
      case KscanType.KSCAN_TYPE_DIRECT: return "Direct GPIO";
      default: return "Unknown";
    }
  };

  return (
    <div className="config-display">
      <div className="config-item">
        <span className="label">Type:</span>
        <span className="value">{getTypeName(config.type)}</span>
      </div>
      {config.charlieplex && (
        <>
          <div className="config-item">
            <span className="label">GPIO Count:</span>
            <span className="value">{config.charlieplex.gpios.length}</span>
          </div>
          <div className="config-item">
            <span className="label">Max Keys:</span>
            <span className="value">
              {config.charlieplex.gpios.length * (config.charlieplex.gpios.length - 1)}
            </span>
          </div>
          <div className="config-item">
            <span className="label">Debounce (Press):</span>
            <span className="value">{config.charlieplex.debouncePressMs} ms</span>
          </div>
          <div className="config-item">
            <span className="label">Debounce (Release):</span>
            <span className="value">{config.charlieplex.debounceReleaseMs} ms</span>
          </div>
          <div className="config-item">
            <span className="label">Mode:</span>
            <span className="value">
              {config.charlieplex.useInterrupt ? "Interrupt" : "Polling"}
            </span>
          </div>
        </>
      )}
    </div>
  );
}

/**
 * Display chattering alerts with diagnosis info
 */
function ChatteringAlertsDisplay({ 
  alerts, 
  config 
}: { 
  alerts: ChatteringAlert[];
  config: KscanConfig | null;
}) {
  return (
    <div className="chattering-alerts">
      <p className="alert-description">
        These keys are showing rapid state changes, which typically indicates:
      </p>
      <ul className="alert-causes">
        <li>🔧 <strong>Insufficient soldering</strong> on the hot-swap socket</li>
        <li>⚡ <strong>Cold solder joint</strong> causing intermittent contact</li>
        <li>🔌 <strong>Loose switch</strong> not fully seated in socket</li>
      </ul>
      <table className="alerts-table">
        <thead>
          <tr>
            <th>Position</th>
            <th>GPIO (Out→In)</th>
            <th>Events</th>
            <th>Duration</th>
          </tr>
        </thead>
        <tbody>
          {alerts.map((alert, idx) => (
            <tr key={idx} className="alert-row">
              <td>Row {alert.row}, Col {alert.col}</td>
              <td>
                {config?.charlieplex ? (
                  <>
                    Pin {config.charlieplex.gpios[alert.row]?.pin} → 
                    Pin {config.charlieplex.gpios[alert.col]?.pin}
                  </>
                ) : (
                  `${alert.row} → ${alert.col}`
                )}
              </td>
              <td className="event-count-cell">{alert.eventCount}</td>
              <td>{alert.lastEventMs - alert.firstEventMs} ms</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

/**
 * Visual key matrix display
 */
function KeyMatrixDisplay({ 
  matrix, 
  events,
  chatteringAlerts 
}: { 
  matrix: GetKeyMatrixResponse;
  events: KeyEvent[];
  chatteringAlerts: ChatteringAlert[];
}) {
  // Create a map for quick lookup
  const keyMap = new Map<string, KeyInfo>();
  matrix.keys.forEach(key => {
    keyMap.set(`${key.row}-${key.col}`, key);
  });

  // Get recent events for highlighting
  const recentEvents = new Set(
    events.slice(-20).map(e => `${e.row}-${e.col}`)
  );

  // Get chattering keys
  const chatteringKeys = new Set(
    chatteringAlerts.map(a => `${a.row}-${a.col}`)
  );

  return (
    <div className="key-matrix">
      <div className="matrix-legend">
        <span className="legend-item"><span className="key-cell pressed"></span> Pressed</span>
        <span className="legend-item"><span className="key-cell recent"></span> Recent Activity</span>
        <span className="legend-item"><span className="key-cell chattering"></span> Chattering</span>
        <span className="legend-item"><span className="key-cell"></span> Idle</span>
      </div>
      <div className="matrix-grid" style={{ 
        gridTemplateColumns: `repeat(${matrix.cols}, 1fr)`,
      }}>
        {Array.from({ length: matrix.rows }).map((_, row) =>
          Array.from({ length: matrix.cols }).map((_, col) => {
            if (row === col) {
              // Diagonal cells are invalid in charlieplex
              return <div key={`${row}-${col}`} className="key-cell disabled">—</div>;
            }
            
            const key = keyMap.get(`${row}-${col}`);
            const isRecent = recentEvents.has(`${row}-${col}`);
            const isChattering = chatteringKeys.has(`${row}-${col}`);
            
            let className = "key-cell";
            if (key?.currentState) className += " pressed";
            else if (isChattering) className += " chattering";
            else if (isRecent) className += " recent";
            
            return (
              <div 
                key={`${row}-${col}`} 
                className={className}
                title={`Row ${row}, Col ${col}\nPress: ${key?.pressCount || 0}\nRelease: ${key?.releaseCount || 0}`}
              >
                <span className="key-pos">{row},{col}</span>
                {key && (
                  <span className="key-count">
                    {key.pressCount}/{key.releaseCount}
                  </span>
                )}
              </div>
            );
          })
        )}
      </div>
      <p className="matrix-hint">
        Numbers show press/release counts. Hover for details.
      </p>
    </div>
  );
}

/**
 * Event log display
 */
function EventLogDisplay({ 
  events, 
  config 
}: { 
  events: KeyEvent[];
  config: KscanConfig | null;
}) {
  const getGpioInfo = (row: number, col: number) => {
    if (!config?.charlieplex) return null;
    const outPin = config.charlieplex.gpios[row];
    const inPin = config.charlieplex.gpios[col];
    return { outPin, inPin };
  };

  return (
    <div className="event-log">
      {events.length === 0 ? (
        <p className="no-events">No events recorded yet. Press some keys to see activity.</p>
      ) : (
        <table className="events-table">
          <thead>
            <tr>
              <th>Time (ms)</th>
              <th>Position</th>
              <th>Action</th>
              <th>GPIO</th>
            </tr>
          </thead>
          <tbody>
            {[...events].reverse().map((event, idx) => {
              const gpioInfo = getGpioInfo(event.row, event.col);
              return (
                <tr key={idx} className={event.pressed ? "press-event" : "release-event"}>
                  <td>{event.timestampMs}</td>
                  <td>({event.row}, {event.col})</td>
                  <td>{event.pressed ? "⬇️ Press" : "⬆️ Release"}</td>
                  <td>
                    {gpioInfo ? (
                      <>Pin {gpioInfo.outPin?.pin} → Pin {gpioInfo.inPin?.pin}</>
                    ) : (
                      "—"
                    )}
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      )}
    </div>
  );
}

/**
 * GPIO pin display with Xiao board visualization
 */
function GpioPinDisplay({ gpios }: { gpios: GpioPin[] }) {
  // Xiao nRF52840 pin mapping (approximate)
  const xiaoPinLabels: Record<number, string> = {
    2: "D0", 3: "D1", 4: "D2", 5: "D3", 28: "D4", 29: "D5",
    30: "D6", 31: "D7", 26: "D8", 6: "D9", 27: "D10",
  };

  return (
    <div className="gpio-display">
      <h3>GPIO Pin Configuration</h3>
      <div className="gpio-list">
        {gpios.map((gpio, idx) => (
          <div key={idx} className="gpio-item">
            <span className="gpio-index">GPIO {idx}</span>
            <span className="gpio-pin">
              {gpio.portName}:{gpio.pin}
              {xiaoPinLabels[gpio.pin] && (
                <span className="xiao-label"> ({xiaoPinLabels[gpio.pin]})</span>
              )}
            </span>
            <span className={`gpio-polarity ${gpio.activeLow ? 'active-low' : 'active-high'}`}>
              {gpio.activeLow ? "Active Low" : "Active High"}
            </span>
          </div>
        ))}
      </div>
      
      {/* Xiao Board Visualization */}
      <div className="xiao-board">
        <h4>Seeed Xiao nRF52840 Pinout</h4>
        <div className="board-visual">
          <div className="pin-row left">
            {["D0", "D1", "D2", "D3", "D4", "D5"].map(pin => {
              const isUsed = gpios.some(g => xiaoPinLabels[g.pin] === pin);
              return (
                <div key={pin} className={`board-pin ${isUsed ? 'used' : ''}`}>
                  {pin}
                </div>
              );
            })}
          </div>
          <div className="board-center">
            <div className="usb-port">USB-C</div>
          </div>
          <div className="pin-row right">
            {["D10", "D9", "D8", "D7", "D6", "3V3"].map(pin => {
              const isUsed = gpios.some(g => xiaoPinLabels[g.pin] === pin);
              return (
                <div key={pin} className={`board-pin ${isUsed ? 'used' : ''}`}>
                  {pin}
                </div>
              );
            })}
          </div>
        </div>
        <p className="board-hint">Highlighted pins are used for key scanning</p>
      </div>
    </div>
  );
}

/**
 * Troubleshooting guide
 */
function TroubleshootingGuide() {
  return (
    <div className="troubleshooting-guide">
      <div className="trouble-item">
        <h4>🔴 Key doesn't register at all</h4>
        <ul>
          <li>Check if the hot-swap socket is properly soldered</li>
          <li>Verify the switch is fully inserted into the socket</li>
          <li>Check for cold solder joints (dull, grainy appearance)</li>
          <li>Verify the GPIO pins in the matrix view match your PCB design</li>
        </ul>
      </div>
      
      <div className="trouble-item">
        <h4>🟡 Key shows chattering (rapid on/off)</h4>
        <ul>
          <li>Reflow solder on the hot-swap socket pads</li>
          <li>Check for debris in the switch or socket</li>
          <li>Try a different switch to rule out switch defects</li>
          <li>Consider increasing debounce time in firmware config</li>
        </ul>
      </div>
      
      <div className="trouble-item">
        <h4>🟢 Multiple keys affected in same row/column</h4>
        <ul>
          <li>Check the shared GPIO pin connection</li>
          <li>Look for solder bridges between adjacent pads</li>
          <li>Verify continuity of traces on the PCB</li>
        </ul>
      </div>

      <div className="trouble-item">
        <h4>📊 Understanding the Matrix</h4>
        <p>
          In a charlieplex matrix, each key is identified by two GPIO pins:
        </p>
        <ul>
          <li><strong>Row (Output):</strong> The pin that drives the signal</li>
          <li><strong>Column (Input):</strong> The pin that reads the signal</li>
          <li>When a key is pressed, it connects these two pins</li>
          <li>The diagonal (row = column) is invalid since a pin can't drive itself</li>
        </ul>
      </div>
    </div>
  );
}

export default App;
