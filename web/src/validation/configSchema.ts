import { z } from "zod";

// Valid ESP32 GPIO pins
const validGPIOs = [
  0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32,
  33, 34, 35, 36, 39,
];

export const wifiConfigSchema = z.object({
  ssid: z.string().min(1, "WiFi SSID is required"),
  password: z.string(),
  hostname: z.string(),
  autoReconnect: z.boolean(),
  reconnectDelayMs: z.number().int().positive(),
  maxReconnectDelayMs: z.number().int().positive(),
});

export const mqttConfigSchema = z
  .object({
    enabled: z.boolean(),
    broker: z.string(),
    port: z
      .number()
      .int()
      .min(1, "Port must be at least 1")
      .max(65535, "Port must be at most 65535"),
    topic: z.string(),
    username: z.string(),
    password: z.string(),
    publishIntervalMs: z
      .number()
      .int()
      .min(1000, "Publish interval must be at least 1 second"),
  })
  .refine(
    (data) =>
      !data.enabled ||
      (data.broker.length > 0 &&
        data.topic.length > 0 &&
        data.topic.match(/^[a-zA-Z0-9/_-]+$/)),
    {
      message: "MQTT broker and valid topic are required when MQTT is enabled",
      path: ["broker"],
    },
  );

export const ntpConfigSchema = z.object({
  enabled: z.boolean(),
  server1: z.string(),
  server2: z.string(),
  timezone: z.string(),
  gmtOffsetSec: z.number().int(),
  daylightOffsetSec: z.number().int(),
  syncIntervalMs: z
    .number()
    .int()
    .min(600000, "Sync interval must be at least 10 minutes"),
});

export const sensorConfigSchema = z
  .object({
    readIntervalMs: z
      .number()
      .int()
      .min(100, "Read interval must be at least 100ms")
      .max(3600000, "Read interval cannot exceed 1 hour"),
    i2cSDA: z
      .number()
      .int()
      .refine((val) => validGPIOs.includes(val), {
        message: `SDA pin must be a valid GPIO: ${validGPIOs.join(", ")}`,
      }),
    i2cSCL: z
      .number()
      .int()
      .refine((val) => validGPIOs.includes(val), {
        message: `SCL pin must be a valid GPIO: ${validGPIOs.join(", ")}`,
      }),
    i2cFrequency: z
      .number()
      .int()
      .min(10000, "I2C frequency must be at least 10kHz")
      .max(400000, "I2C frequency must be at most 400kHz"),
  })
  .refine((data) => data.i2cSDA !== data.i2cSCL, {
    message: "SDA and SCL pins must be different",
    path: ["i2cSDA"],
  });

export const configSchema = z.object({
  deviceName: z.string().min(1, "Device name is required"),
  wifi: wifiConfigSchema,
  mqtt: mqttConfigSchema,
  ntp: ntpConfigSchema,
  sensor: sensorConfigSchema,
  timezone: z.string(),
});

export type ConfigSchema = z.infer<typeof configSchema>;
