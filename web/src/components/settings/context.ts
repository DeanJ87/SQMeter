import type { Config, SystemStatus } from '../../types';
import type { Hardware } from './hardware';
import type { SettingsTabId } from './tabs';

export type ConfigPath = string[];

export interface SettingsTabProps {
  config: Config;
  update: (path: ConfigPath, value: unknown) => void;
  // Apply several changes as one state update (avoids lost writes when two
  // fields change together, e.g. hour + minute).
  updateMany: (changes: [ConfigPath, unknown][]) => void;
  // Changes the device has already stored (e.g. dark calibration): applied
  // without marking the form unsaved.
  applyStored: (changes: [ConfigPath, unknown][]) => void;
  error: (key: string) => string | undefined;
  hw: Hardware;
  status: SystemStatus | null;
  dirty: boolean; // unsaved changes exist
  goTo: (tab: SettingsTabId, anchor?: string) => void;
}
