import { t } from '../../i18n';

export const bortleTone = (bortle?: number): string => {
  if (typeof bortle !== 'number') return 'tone-muted';
  if (bortle <= 2) return 'tone-cyan';
  if (bortle <= 4) return 'tone-green';
  if (bortle <= 6) return 'tone-amber';
  return 'tone-red';
};

// The device classifies the sky (with the thresholds in Settings); show its verdict.
const CONDITION_TONE: Record<string, string> = { clear: 'pill-green', cloudy: 'pill-amber', overcast: 'pill-red' };
export const conditionTone = (condition?: string): string => (condition && CONDITION_TONE[condition]) || 'pill-dim';

// Highlight a wind reading against its safety limit: red at or above, amber
// within 80% of it; plain when that limit is off.
export const windTone = (value: number | undefined, enabled: boolean | undefined, limit: number | undefined, base = '') => {
  if (typeof value !== 'number' || !enabled || typeof limit !== 'number' || limit <= 0) return base;
  if (value >= limit) return 'tone-red';
  if (value >= limit * 0.8) return 'tone-amber';
  return base;
};

export const compassPoint = (degrees: number) => t('dashboard.compassPoints').split(' ')[Math.round(degrees / 22.5) % 16];
