import type { Config, SystemStatus } from '../types';
import { getJson, post, postJson, request } from './api';

// What Settings reads from and writes to the device (STRUCT-04).

/** The stored settings, as the device sends them (normalised by the caller). Rejects when unreachable or not JSON. */
export const fetchConfig = async (): Promise<Config> => (await request('/api/config')).json();

/** The live status, or null when the device answers with an error. */
export const fetchStatus = () => getJson<SystemStatus>('/api/status');

/** Restart the device. */
export const restartDevice = () => post('/api/restart');

/** Save the settings: whether the device accepted them, and its JSON answer if it sent one. */
export const saveConfig = async (payload: Config): Promise<{ ok: boolean; body: { success?: boolean; error?: unknown } | null }> => {
  const response = await postJson('/api/config', payload);
  const body = (response.headers.get('content-type') || '').includes('application/json') ? await response.json() : null;
  return { ok: response.ok, body };
};
