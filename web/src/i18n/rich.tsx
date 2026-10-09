import type { ComponentChildren } from 'preact';
import { t, type MessageKey } from './index';

// A whole translated sentence with markup inside it, e.g. "SQMeter joined
// <b>{ssid}</b> and is restarting." - translators move {ssid} wherever their
// grammar needs it, instead of the sentence being split around the markup.
export function tRich(key: MessageKey, parts: Record<string, ComponentChildren>) {
  const placeholders = Object.fromEntries(Object.keys(parts).map((name) => [name, `\u0000${name}\u0000`]));
  return t(key, placeholders)
    .split('\u0000')
    .map((piece, i) => (i % 2 === 1 && piece in parts ? parts[piece] : piece));
}
