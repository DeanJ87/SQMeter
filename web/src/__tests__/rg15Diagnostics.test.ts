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
      }).success
    ).toBe(true);
  });

  it("exposes RG-15 diagnostics in the sensor mock", () => {
    const sensor = generateSensorData();
    expect(sensor.rainSensor).toBeDefined();
    expect(sensor.rainSensor?.initialized).toBe(true);
    expect(sensor.rainSensor?.online).toBe(true);
    expect(sensor.rainSensor?.stale).toBe(false);
    expect(sensor.rainSensor?.uart.last_raw_response).toContain("Acc");
    expect(sensor.rainSensor?.uart.software_version).toBe("1.000");
    expect(sensor.rainSensor?.uart.stale_timeout_ms).toBe(4500000);
    expect(sensor.rainSensor?.uart.successful_reads).toBeGreaterThan(0);
  });

  it("exposes RG-15 diagnostics in the status mock", () => {
    expect(mockStatus.sensors.rg15).toBeDefined();
    expect(mockStatus.sensors.rg15?.initialized).toBe(true);
    expect(mockStatus.sensors.rg15?.online).toBe(true);
    expect(mockStatus.sensors.rg15?.uart?.last_command).toBe("R");
    expect(mockStatus.sensors.rg15?.uart?.software_version).toBe("1.000");
    expect(mockStatus.sensors.rg15?.uart?.timeouts).toBe(0);
  });
});
