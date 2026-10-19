import type { BackupCore } from './backup';

// What the device core (WebAssembly, tools/demo-core/bridge.cpp) exports.

export interface Core extends BackupCore {
  getConfig(redacted: boolean): string;
  applyConfig(json: string): string;
  loadConfig(json: string): boolean;
  restart(nowMs: number): void;
  tick(nowMs: number, epoch: number, inputs: string, localTime: string, localDate: string): void;
  readings(): string;
  statusParts(): string;
  safety(): string;
  effective(): string;
  safetyHistory(): string;
  recentAlerts(): string;
  clearAlerts(): void;
  isArmed(): boolean;
  armedDocument(): string;
  setArmed(on: boolean, source: string): void;
  testAlert(params: string): string;
  realTestAlert(): void;
  deliveryRequests(id: number, credentials: string): string;
  calibrateDark(sensor: string): string;
  displayRequest(wake: boolean): string;
  alpaca(method: string, path: string, params: string): string;
  saveState(): string;
  loadState(json: string): boolean;
  pending(): string;
}
