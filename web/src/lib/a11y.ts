import { useEffect, useRef } from 'preact/hooks';
import type { RefObject } from 'preact';
import { t } from '../i18n';

// Accessibility helpers shared by the UI (spec 022). Small on purpose: the
// device serves the UI from limited flash, so no focus-trap or dialog library.

// --- Announcements (FR-009) --------------------------------------------------
// One polite live region (rendered by Layout). Live readings never announce
// themselves; only a verdict change, a new alert and connection loss/recovery
// call this, and the same text within 2 s is dropped.

export const LIVE_REGION_ID = 'sqm-live';
const DEDUPE_MS = 2000;
let last = { text: '', at: 0 };

export const announce = (text: string, now: number = Date.now()) => {
  if (!text || (text === last.text && now - last.at < DEDUPE_MS)) return false;
  last = { text, at: now };
  const region = typeof document === 'undefined' ? null : document.getElementById(LIVE_REGION_ID);
  if (region) {
    // Clear first so a repeated message is read again.
    region.textContent = '';
    setTimeout(() => {
      region.textContent = text;
    }, 50);
  }
  return true;
};

export const resetAnnouncements = () => {
  last = { text: '', at: 0 };
};

// Announce `message(value)` when `value` changes, but not for the first value
// (the page loading isn't news).
export const useAnnounceChange = <T>(value: T | undefined, message: (value: T) => string | null) => {
  const previous = useRef<T | undefined>(undefined);
  // The latest wording, without re-running the effect on every render.
  const wording = useRef(message);
  wording.current = message;
  useEffect(() => {
    if (value === undefined) return;
    if (previous.current !== undefined && previous.current !== value) {
      const text = wording.current(value);
      if (text) announce(text);
    }
    previous.current = value;
  }, [value]);
};

// --- Dialog focus (FR-012) -----------------------------------------------------
// Non-modal dialogs and flyouts: focus moves in on open (to an element marked
// data-autofocus, else the first focusable one, else the container) and back
// to the trigger on close if it was inside.

const FOCUSABLE =
  // i18n-ignore: a CSS selector, not text
  'a[href], button:not([disabled]), input:not([disabled]), select:not([disabled]), textarea:not([disabled]), [tabindex]:not([tabindex="-1"])';

export const useDialogFocus = (open: boolean, container: RefObject<HTMLElement>, trigger: RefObject<HTMLElement>) => {
  useEffect(() => {
    if (!open) return undefined;
    const box = container.current;
    const opener = trigger.current;
    const target = box?.querySelector<HTMLElement>('[data-autofocus]') ?? box?.querySelector<HTMLElement>(FOCUSABLE) ?? box;
    target?.focus();
    return () => {
      const active = document.activeElement;
      if (!active || active === document.body || box?.contains(active)) opener?.focus();
    };
  }, [open, container, trigger]);
};

// --- Tabs (FR-013) ---------------------------------------------------------------
// ARIA tabs keyboard: Left/Right wrap, Home/End jump. Null for other keys.
export const nextTabIndex = (key: string, index: number, count: number): number | null => {
  if (count <= 0) return null;
  switch (key) {
    case 'ArrowRight':
      return (index + 1) % count;
    case 'ArrowLeft':
      return (index - 1 + count) % count;
    case 'Home':
      return 0;
    case 'End':
      return count - 1;
    default:
      return null;
  }
};

// --- Motion (FR-014) -------------------------------------------------------------
export const prefersReducedMotion = () =>
  typeof window !== 'undefined' && typeof window.matchMedia === 'function' && window.matchMedia('(prefers-reduced-motion: reduce)').matches;

export const scrollBehavior = (): ScrollBehavior => (prefersReducedMotion() ? 'auto' : 'smooth');

// --- Charts (FR-011) ---------------------------------------------------------------
// A sparkline's meaning in words: direction and range over the window shown.
export const summariseSeries = (label: string, values: number[], format: (value: number) => string = (v) => String(v)) => {
  const finite = values.filter((v) => Number.isFinite(v));
  if (finite.length < 2) return t('a11y.trendNoData', { label });
  const first = finite[0];
  const lastValue = finite[finite.length - 1];
  const min = Math.min(...finite);
  const max = Math.max(...finite);
  const span = max - min;
  const change = lastValue - first;
  const direction = span === 0 || Math.abs(change) < span * 0.2 ? t('a11y.steady') : change > 0 ? t('a11y.rising') : t('a11y.falling');
  return t('a11y.trend', {
    label,
    direction,
    from: format(first),
    to: format(lastValue),
    min: format(min),
    max: format(max),
    count: finite.length,
  });
};
