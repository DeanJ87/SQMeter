// The device's documents as the web UI reads them, by area.
export * from './readings';
export * from './status';
export * from './config';
export * from './alerts';

export interface WiFiNetwork {
  ssid: string;
  rssi: number;
  encryption: 'open' | 'secured';
}

export interface GithubRelease {
  tag: string;
  name: string;
  prerelease: boolean;
  publishedAt: string;
  firmwareAssetUrl: string;
  firmwareAssetSize: number;
  fsAssetUrl: string;
  fsAssetSize: number;
}

// ASCOM Alpaca response envelope (https://ascom-standards.org/api/)
export interface AlpacaResponse<T> {
  Value: T;
  ClientTransactionID: number;
  ServerTransactionID: number;
  ErrorNumber: number;
  ErrorMessage: string;
}

export interface AlpacaConfiguredDevice {
  DeviceName: string;
  DeviceType: string;
  DeviceNumber: number;
  UniqueID: string;
}

export interface AlpacaDeviceStateItem {
  Name: string;
  Value: number | boolean | string;
}
