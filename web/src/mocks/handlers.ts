import { http, HttpResponse, ws } from "msw";
import {
  generateSensorData,
  mockStatus,
  mockConfig,
  mockWifiNetworks,
} from "./data";

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

  // REST — config
  http.get("/api/config", () => HttpResponse.json(mockConfig)),
  http.post("/api/config", () => HttpResponse.json({ ok: true })),
  http.put("/api/config", () => HttpResponse.json({ ok: true })),

  // REST — wifi (returns { networks: [...] } to match firmware API shape)
  http.get("/api/wifi/scan", () =>
    HttpResponse.json({ networks: mockWifiNetworks })
  ),
  http.post("/api/wifi/connect", () => HttpResponse.json({ ok: true })),

  // REST — MQTT test
  http.post("/api/mqtt/test", () =>
    HttpResponse.json({ success: true, message: "Connection successful (demo)" })
  ),

  // REST — control
  http.post("/api/restart", () => HttpResponse.json({ ok: true })),
  http.post("/api/update", () => HttpResponse.json({ success: true })),
  http.post("/api/update/fs", () => HttpResponse.json({ success: true })),

  // WebSocket — push sensor data every second
  sensorSocket.addEventListener("connection", ({ client }) => {
    client.send(JSON.stringify(generateSensorData()));

    const interval = setInterval(() => {
      client.send(JSON.stringify(generateSensorData()));
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
