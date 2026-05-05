import { describe, it, expect } from "vitest";
import { configSchema, authConfigSchema } from "../validation/configSchema";
import { mockConfig } from "../mocks/data";
import type { Config } from "../types";

// Minimal valid config satisfying all schema rules
const validBase: Config = {
  deviceName: "test-sqm",
  timezone: "UTC0",
  primaryTimeSource: 0,
  secondaryTimeSource: 1,
  wifi: {
    ssid: "TestNet",
    password: "",
    hostname: "sqm-test",
    autoReconnect: true,
    reconnectDelayMs: 1000,
    maxReconnectDelayMs: 300000,
  },
  mqtt: {
    enabled: false,
    broker: "",
    port: 1883,
    username: "",
    password: "",
    topic: "sqm/data",
    publishIntervalMs: 5000,
  },
  ota: { enabled: false, password: "" },
  auth: { enabled: false, username: "admin", password: "" },
  ntp: {
    enabled: true,
    server1: "pool.ntp.org",
    server2: "time.nist.gov",
    timezone: "UTC0",
    gmtOffsetSec: 0,
    daylightOffsetSec: 0,
    syncIntervalMs: 600000,
  },
  gps: { enabled: false, rxPin: 16, txPin: 17, baudRate: 9600 },
  sensor: { readIntervalMs: 5000, i2cSDA: 21, i2cSCL: 22, i2cFrequency: 100000 },
};

describe("configSchema", () => {
  it("accepts a valid config with auth disabled", () => {
    expect(configSchema.safeParse(validBase).success).toBe(true);
  });

  it("accepts auth enabled with credentials", () => {
    const cfg = { ...validBase, auth: { enabled: true, username: "admin", password: "s3cr3t" } };
    expect(configSchema.safeParse(cfg).success).toBe(true);
  });

  it("rejects auth enabled with empty password", () => {
    const cfg = { ...validBase, auth: { enabled: true, username: "admin", password: "" } };
    const result = configSchema.safeParse(cfg);
    expect(result.success).toBe(false);
    if (!result.success) {
      const paths = result.error.issues.map((i) => i.path.join("."));
      expect(paths).toContain("auth.password");
    }
  });

  it("rejects auth enabled with empty username", () => {
    const cfg = { ...validBase, auth: { enabled: true, username: "", password: "s3cr3t" } };
    const result = configSchema.safeParse(cfg);
    expect(result.success).toBe(false);
    if (!result.success) {
      const paths = result.error.issues.map((i) => i.path.join("."));
      expect(paths).toContain("auth.username");
    }
  });

  it("rejects empty deviceName", () => {
    const cfg = { ...validBase, deviceName: "" };
    expect(configSchema.safeParse(cfg).success).toBe(false);
  });

  it("rejects MQTT enabled with empty broker", () => {
    const cfg = { ...validBase, mqtt: { ...validBase.mqtt, enabled: true, broker: "" } };
    expect(configSchema.safeParse(cfg).success).toBe(false);
  });

  it("rejects OTA enabled with empty password", () => {
    const cfg = { ...validBase, ota: { enabled: true, password: "" } };
    expect(configSchema.safeParse(cfg).success).toBe(false);
  });

  it("rejects I2C SDA and SCL on the same pin", () => {
    const cfg = { ...validBase, sensor: { ...validBase.sensor, i2cSDA: 21, i2cSCL: 21 } };
    expect(configSchema.safeParse(cfg).success).toBe(false);
  });
});

describe("authConfigSchema", () => {
  it("passes when disabled regardless of credentials", () => {
    expect(authConfigSchema.safeParse({ enabled: false, username: "", password: "" }).success).toBe(true);
  });

  it("passes when enabled with both username and password", () => {
    expect(
      authConfigSchema.safeParse({ enabled: true, username: "admin", password: "hunter2" }).success
    ).toBe(true);
  });

  it("fails when enabled with empty password", () => {
    expect(
      authConfigSchema.safeParse({ enabled: true, username: "admin", password: "" }).success
    ).toBe(false);
  });

  it("fails when enabled with empty username", () => {
    expect(
      authConfigSchema.safeParse({ enabled: true, username: "", password: "hunter2" }).success
    ).toBe(false);
  });
});

describe("mockConfig", () => {
  it("has an auth field", () => {
    expect(mockConfig.auth).toBeDefined();
  });

  it("has auth.enabled = false by default", () => {
    expect(mockConfig.auth.enabled).toBe(false);
  });

  it("has a non-empty auth.username", () => {
    expect(mockConfig.auth.username.length).toBeGreaterThan(0);
  });

  it("has an empty auth.password (not a real credential)", () => {
    expect(mockConfig.auth.password).toBe("");
  });

  it("satisfies configSchema", () => {
    expect(configSchema.safeParse(mockConfig).success).toBe(true);
  });

  it("does not expose a real password value in wifi mock", () => {
    // wifi.password should be empty in the demo config
    expect(mockConfig.wifi.password).toBe("");
  });

  it("does not expose a real password value in mqtt mock", () => {
    expect(mockConfig.mqtt.password).toBe("");
  });
});
