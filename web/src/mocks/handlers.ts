import { http, HttpResponse, ws } from "msw";
import {
  generateSensorData,
  mockStatus,
  mockConfig,
  mockWifiNetworks,
} from "./data";

// WebSocket handler — matches any host so it works on both localhost and Pages
const sensorSocket = ws.link("*/ws/sensors");

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

  // REST — config
  http.get("/api/config", () => HttpResponse.json(mockConfig)),
  http.post("/api/config", () => HttpResponse.json({ ok: true })),

  // REST — wifi
  http.get("/api/wifi/scan", () => HttpResponse.json(mockWifiNetworks)),
  http.post("/api/wifi/connect", () => HttpResponse.json({ ok: true })),

  // REST — control
  http.post("/api/restart", () => HttpResponse.json({ ok: true })),
  http.post("/api/update", () => HttpResponse.json({ ok: true })),

  // WebSocket — push sensor data every second, simulating live readings
  sensorSocket.addEventListener("connection", ({ client }) => {
    client.send(JSON.stringify(generateSensorData()));

    const interval = setInterval(() => {
      client.send(JSON.stringify(generateSensorData()));
    }, 1000);

    client.addEventListener("close", () => clearInterval(interval));
  }),
];
