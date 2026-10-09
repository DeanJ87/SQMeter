// 0 off, 1 quiet, 2 normal, 3 urgent, 4 wake me
export type AlertLevel = 0 | 1 | 2 | 3 | 4;

export type AlertEventKey =
  | 'unsafe'
  | 'safe'
  | 'rain_started'
  | 'rain_stopped'
  | 'sensor_fault'
  | 'sensor_recovered'
  | 'dew_risk'
  | 'clear_sky'
  | 'clouded_over'
  | 'client_lost'
  | 'client_back'
  | 'client_disconnected';

// sound: Pushover sound name; empty uses the Pushover default.
export interface AlertEventSetting {
  level: AlertLevel;
  sound: string;
  // Custom wording with {variables}; empty or missing uses the default.
  title?: string;
  message?: string;
}

export interface AlertsConfig {
  enabled: boolean;
  events: Record<AlertEventKey, AlertEventSetting>;
  dewRiskMarginC: number;
  clearSkyCloudPercent: number;
  cloudedOverCloudPercent: number;
  skyNightOnly: boolean;
  safetyNightOnly: boolean;
  // When alerts are sent (specs/021); older firmware has only armWithAlpaca.
  sendMode?: AlertSendMode;
  armWithAlpaca?: boolean;
  clientSilentSafetySeconds?: number;
  clientSilentWeatherSeconds?: number;
  nightSunAltitudeDeg: number;
  cooldownSeconds: number;
  pushover: { enabled: boolean; userKey: string; appToken: string; sound: string };
  ntfy: { enabled: boolean; server: string; topic: string; token: string };
  webhook: { enabled: boolean; url: string; authHeader: string; insecureTls: boolean };
  mqtt: { enabled: boolean };
}

export type AlertChannelName = 'mqtt' | 'pushover' | 'ntfy' | 'webhook';

export type AlertSendMode = 'any' | 'whileConnected';

export type AlertScheduleReason =
  'none' | 'user-ui' | 'user-rest' | 'user-mqtt' | 'client-connected' | 'client-disconnected' | 'waiting-for-client' | 'migrated';

// GET /api/alerts/armed, and /api/status "alerts". Older firmware sends only
// armed and armWithAlpaca.
export interface AlertSchedule {
  armed: boolean;
  armWithAlpaca?: boolean;
  mode?: AlertSendMode;
  reason?: AlertScheduleReason;
  since?: string | null; // ISO 8601 UTC, null without a clock
  sinceAgeMs?: number | null; // null: before this boot
  recentRevision?: number; // /api/status only: changes whenever /api/alerts/recent would
}

// One Alpaca device as the imaging app sees it (/api/status "alpaca").
export interface AlpacaClientState {
  connected: boolean;
  watching: boolean;
  silent: boolean;
  lastCheckedAgeMs: number | null;
  clientId: number | null;
}

export interface AlertRecord {
  id: number;
  event: string;
  title: string;
  message: string;
  level: 'quiet' | 'normal' | 'urgent' | 'wake' | 'off';
  ageSeconds: number;
  timestamp?: number;
  channels: Partial<Record<AlertChannelName, { status: 'pending' | 'sent' | 'failed' | 'skipped'; detail: string }>>;
}

export interface AlertsRecent {
  enabled: boolean;
  // Sending (true) or paused; missing from older firmware = sending.
  armed?: boolean;
  alerts: AlertRecord[];
}
