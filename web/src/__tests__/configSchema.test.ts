import { describe, it, expect } from "vitest";
import {
  configSchema,
  authConfigSchema,
  alpacaConfigSchema,
  getConfigValidationErrors,
  getConfigValidationMessage,
  hasConfigValidationErrors,
} from "../validation/configSchema";
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
  cloudDetection: { clearSkyThreshold: -13.0, cloudyThreshold: -3.0, humidityCorrection: 0.75 },
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

describe("frontend settings validation", () => {
  const setPath = (source: Config, path: string, value: unknown): Config => {
    const candidate = structuredClone(source) as Config;
    const parts = path.split(".");
    let target: Record<string, unknown> = candidate as unknown as Record<string, unknown>;

    for (const part of parts.slice(0, -1)) {
      target = target[part] as Record<string, unknown>;
    }
    target[parts.at(-1)!] = value;
    return candidate;
  };

  const validSettings: Array<[path: string, value: unknown]> = [
    ["deviceName", "Back Garden SQMeter"],
    ["wifi.ssid", "Observatory"],
    ["wifi.password", "wifi-secret"],
    ["wifi.hostname", "sqmeter-garden"],
    ["wifi.autoReconnect", false],
    ["ota.enabled", false],
    ["ota.password", ""],
    ["auth.enabled", false],
    ["auth.username", "operator"],
    ["auth.password", ""],
    ["ntp.enabled", true],
    ["ntp.timezone", "GMT0BST,M3.5.0/1,M10.5.0"],
    ["ntp.server1", "time.cloudflare.com"],
    ["ntp.server2", ""],
    ["ntp.syncIntervalMs", 600000],
    ["gps.enabled", true],
    ["gps.rxPin", 16],
    ["gps.txPin", 17],
    ["gps.baudRate", 115200],
    ["primaryTimeSource", 0],
    ["secondaryTimeSource", 1],
    ["mqtt.enabled", true],
    ["mqtt.broker", "mqtt.example.com"],
    ["mqtt.port", 8883],
    ["mqtt.username", "sqmeter"],
    ["mqtt.password", "mqtt-secret"],
    ["mqtt.topic", "observatory/sqmeter"],
    ["mqtt.publishIntervalMs", 1000],
    ["sensor.readIntervalMs", 100],
    ["sensor.i2cSDA", 21],
    ["sensor.i2cSCL", 22],
    ["sensor.i2cFrequency", 400000],
    ["cloudDetection.clearSkyThreshold", -15],
    ["cloudDetection.cloudyThreshold", -2],
    ["cloudDetection.humidityCorrection", 1],
    ["rain.enabled", true],
    ["rain.rxPin", 18],
    ["rain.txPin", 19],
    ["rain.baudRate", 19200],
    ["rain.pollIntervalMs", 1000],
    ["rain.rainClearDelayMs", 60000],
    ["rain.resolution", "low"],
    ["rain.units", "imperial"],
    ["rain.debugUart", true],
    ["rain.dailyResetEnabled", true],
    ["rain.dailyResetHour", 23],
    ["rain.dailyResetMinute", 59],
  ];

  it.each(validSettings)("accepts the frontend setting %s", (path, value) => {
    const result = configSchema.safeParse(setPath(mockConfig, path, value));
    expect(result.success, result.success ? undefined : JSON.stringify(result.error.issues)).toBe(true);
  });

  it("allows save when a valid configuration has zero validation errors", () => {
    const errors = getConfigValidationErrors(mockConfig);
    expect(errors).toEqual({});
    expect(hasConfigValidationErrors(errors)).toBe(false);
  });

  it("blocks save and identifies the field when validation fails", () => {
    const errors = getConfigValidationErrors(setPath(mockConfig, "deviceName", ""));
    expect(errors.deviceName).toBe("Device name is required");
    expect(hasConfigValidationErrors(errors)).toBe(true);
  });

  it("reports one actionable error when both NTP and GPS are disabled", () => {
    const candidate = structuredClone(mockConfig);
    candidate.ntp.enabled = false;
    candidate.gps.enabled = false;

    const errors = getConfigValidationErrors(candidate);

    expect(errors).toEqual({
      "ntp.enabled": "Enable at least one time source: NTP or GPS",
    });
    expect(hasConfigValidationErrors(errors)).toBe(true);
    expect(getConfigValidationMessage(errors)).toBe(
      "Please fix 1 validation error: Enable at least one time source: NTP or GPS",
    );
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

describe("alpacaConfigSchema", () => {
  const validAlpaca = {
    enabled: false,
    manualOverrideUnsafe: false,
    staleAfterSeconds: 30,
    cloudCoverEnabled: true,
    cloudCoverUnsafePercent: 90,
    sqmMinEnabled: false,
    sqmMinSafe: 0,
    humidityMaxEnabled: false,
    humidityMaxSafe: 100,
    dewpointMarginEnabled: false,
    dewpointMarginMinC: 0,
  };

  it("passes with valid defaults", () => {
    expect(alpacaConfigSchema.safeParse(validAlpaca).success).toBe(true);
  });

  it("fails when staleAfterSeconds is zero", () => {
    expect(alpacaConfigSchema.safeParse({ ...validAlpaca, staleAfterSeconds: 0 }).success).toBe(false);
  });

  it("fails when staleAfterSeconds exceeds 1 hour", () => {
    expect(alpacaConfigSchema.safeParse({ ...validAlpaca, staleAfterSeconds: 3601 }).success).toBe(false);
  });

  it("fails when cloudCoverUnsafePercent is out of range", () => {
    expect(alpacaConfigSchema.safeParse({ ...validAlpaca, cloudCoverUnsafePercent: 101 }).success).toBe(false);
    expect(alpacaConfigSchema.safeParse({ ...validAlpaca, cloudCoverUnsafePercent: -1 }).success).toBe(false);
  });

  it("fails when sqmMinSafe is out of range", () => {
    expect(alpacaConfigSchema.safeParse({ ...validAlpaca, sqmMinSafe: -1 }).success).toBe(false);
    expect(alpacaConfigSchema.safeParse({ ...validAlpaca, sqmMinSafe: 31 }).success).toBe(false);
  });

  it("fails when humidityMaxSafe is out of range", () => {
    expect(alpacaConfigSchema.safeParse({ ...validAlpaca, humidityMaxSafe: 101 }).success).toBe(false);
  });

  it("fails when dewpointMarginMinC is out of range", () => {
    expect(alpacaConfigSchema.safeParse({ ...validAlpaca, dewpointMarginMinC: -1 }).success).toBe(false);
    expect(alpacaConfigSchema.safeParse({ ...validAlpaca, dewpointMarginMinC: 21 }).success).toBe(false);
  });

  it("is accepted as an optional field on the full config schema", () => {
    expect(configSchema.safeParse({ ...validBase, alpaca: validAlpaca }).success).toBe(true);
  });

  it("full config schema still passes when alpaca is omitted", () => {
    expect(configSchema.safeParse(validBase).success).toBe(true);
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
