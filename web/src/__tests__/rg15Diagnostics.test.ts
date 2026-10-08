import { describe, it, expect } from "vitest";
import { configSchema, rainSensorConfigSchema } from "../validation/configSchema";
import { generateSensorData, mockConfig, mockStatus } from "../mocks/data";

describe("RG-15 diagnostics contracts", () => {
  it("includes the RG-15 UART debug toggle in config", () => {
    expect(mockConfig.rain?.debugUart).toBe(false);
    expect(configSchema.safeParse(mockConfig).success).toBe(true);
  });

  it("accepts RG-15 configs with the UART debug toggle", () => {
    expect(
      rainSensorConfigSchema.safeParse({
        enabled: true,
        rxPin: 18,
        txPin: 19,
        baudRate: 9600,
        debugUart: false,
        mode: "polling",
        resolution: "high",
        units: "metric",
        pollIntervalMs: 5000,
        rainClearDelayMs: 900000,
        dailyResetEnabled: true,
        dailyResetHour: 0,
        dailyResetMinute: 0,
      }).success
    ).toBe(true);
  });

  it("reports rain readings in the sensor mock, without diagnostics", () => {
    const sensor = generateSensorData();
    expect(sensor.rain?.status).toBe("ok");
    expect(sensor.rain?.raining).toBe(true);
    expect(sensor.rain?.intensity).toBeGreaterThan(0);
    expect(sensor.rain).not.toHaveProperty("uart");
  });

  it("exposes RG-15 diagnostics in the status mock", () => {
    expect(mockStatus.sensors.rain?.status).toBe("ok");
    expect(mockStatus.diagnostics?.rain?.lastCommand).toBe("R");
    expect(mockStatus.diagnostics?.rain?.lastResponse).toContain("Acc");
    expect(mockStatus.diagnostics?.rain?.softwareVersion).toBe("1.000");
    expect(mockStatus.diagnostics?.rain?.timeouts).toBe(0);
  });
});
