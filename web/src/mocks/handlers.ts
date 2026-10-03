import { http, HttpResponse, ws } from "msw";
import {
  generateSensorData,
  mockStatus,
  mockConfig,
  mockWifiNetworks,
  mockGithubReleases,
  mockAlpacaDevices,
  mockRecentAlerts,
} from "./data";

const alpacaEnvelope = <T,>(Value: T) => ({
  Value,
  ClientTransactionID: 0,
  ServerTransactionID: 1,
  ErrorNumber: 0,
  ErrorMessage: "",
});

// WebSocket handlers — wildcard host works on both localhost and GitHub Pages
const sensorSocket = ws.link("*/ws/sensors");
const statusSocket = ws.link("*/ws/status");

export const handlers = [
  // REST — sensors snapshot
  http.get("/api/sensors", () => HttpResponse.json(generateSensorData())),

  // REST — system status (refresh uptime each call)
  http.get("/api/status", () =>
    HttpResponse.json({
      ...mockStatus,
      uptime: mockStatus.uptime + Math.floor(Math.random() * 10),
      time: { iso: new Date().toISOString(), timezone: "GMT0" },
    })
  ),

  // ASCOM Alpaca — management + device state
  http.get("/management/v1/configureddevices", () =>
    HttpResponse.json(alpacaEnvelope(mockAlpacaDevices))
  ),
  http.get("/management/v1/description", () =>
    HttpResponse.json(alpacaEnvelope({ ServerName: "SQMeter", Manufacturer: "SQMeter", ManufacturerVersion: "demo", Location: "Demo" }))
  ),
  http.get("/api/v1/safetymonitor/0/devicestate", () =>
    HttpResponse.json(alpacaEnvelope([{ Name: "IsSafe", Value: true }, { Name: "TimeStamp", Value: new Date().toISOString() }]))
  ),
  http.get("/api/v1/observingconditions/0/devicestate", () => {
    const data = generateSensorData();
    return HttpResponse.json(alpacaEnvelope([
      { Name: "CloudCover", Value: data.cloudConditions?.cloudCoverPercent ?? 0 },
      { Name: "DewPoint", Value: data.environment?.dewpoint ?? 0 },
      { Name: "Humidity", Value: data.environment?.humidity ?? 0 },
      { Name: "SkyQuality", Value: data.skyQuality?.sqm ?? 0 },
      { Name: "Temperature", Value: data.environment?.temperature ?? 0 },
      { Name: "TimeStamp", Value: new Date().toISOString() },
    ]));
  }),

  // REST — SafetyMonitor verdict
  http.get("/api/safety", () => HttpResponse.json(generateSensorData().safety)),

  // REST — alerts
  http.get("/api/alerts/recent", () => HttpResponse.json(mockRecentAlerts)),
  http.post("/api/alerts/test", () =>
    HttpResponse.json({ success: true, message: "Test notification queued" }, { status: 202 })
  ),

  // REST — config
  http.get("/api/config", () => HttpResponse.json(mockConfig)),
  http.post("/api/config", () => HttpResponse.json({ success: true })),
  http.put("/api/config", () => HttpResponse.json({ success: true })),

  // REST — wifi (returns { networks: [...] } to match firmware API shape)
  http.get("/api/wifi/scan", () =>
    HttpResponse.json({ networks: mockWifiNetworks })
  ),
  http.post("/api/wifi/connect", () => HttpResponse.json({ ok: true })),

  // REST — MQTT test
  http.post("/api/mqtt/test", () =>
    HttpResponse.json({ success: true, message: "Connection successful (demo)" })
  ),

  // REST — RG-15 communication test
  http.post("/api/sensors/rg15/test", () =>
    HttpResponse.json({
      ok: true,
      command: "R",
      bytes_written: 2,
      elapsed_ms: 42,
      raw_response: "Acc 0.00 mm, EventAcc 0.00 mm, TotalAcc 1.24 mm, RInt 0.00 mm/h",
      ack: "m",
      acknowledged: true,
      parsed: true,
      online: true,
      stale: false,
      error: null,
      hint: "Check RG-15 Serial OUT -> ESP32 RX, Serial IN -> ESP32 TX, common ground, baud rate, and voltage level.",
    })
  ),
  http.post("/api/sensors/rg15/reset-total", () =>
    HttpResponse.json({ ok: true, command: "O", message: "RG-15 total accumulation reset command sent" })
  ),
  http.post("/api/sensors/rg15/reboot", () =>
    HttpResponse.json({ ok: true, command: "K", message: "RG-15 reboot command sent" })
  ),

  // REST — control
  http.post("/api/restart", () => HttpResponse.json({ ok: true })),
  http.post("/api/update", () => HttpResponse.json({ success: true })),
  http.post("/api/update/fs", () => HttpResponse.json({ success: true })),

  // REST — GitHub release updates
  http.get("/api/updates/check", ({ request }) => {
    const track = new URL(request.url).searchParams.get("track") === "beta" ? "beta" : "stable";
    return HttpResponse.json(mockGithubReleases.filter((r) => r.prerelease === (track === "beta")));
  }),
  http.post("/api/updates/apply", () =>
    HttpResponse.json({ success: true, message: "Update started" })
  ),

  // WebSocket — push sensor data every second
  sensorSocket.addEventListener("connection", ({ client }) => {
    let lastSensorData = generateSensorData();
    client.send(JSON.stringify(lastSensorData));

    const interval = setInterval(() => {
      if (Date.now() - (lastSensorData.dataTimestamp ?? 0) >= mockConfig.sensor.readIntervalMs) {
        lastSensorData = generateSensorData();
      }
      client.send(JSON.stringify(lastSensorData));
    }, 1000);

    client.addEventListener("close", () => clearInterval(interval));
  }),

  // WebSocket — push system status every 5 seconds
  statusSocket.addEventListener("connection", ({ client }) => {
    const send = () =>
      client.send(
        JSON.stringify({
          ...mockStatus,
          uptime: mockStatus.uptime + Math.floor(Date.now() / 1000),
          time: { iso: new Date().toISOString(), timezone: "GMT0" },
        })
      );

    send();
    const interval = setInterval(send, 5000);
    client.addEventListener("close", () => clearInterval(interval));
  }),
];
