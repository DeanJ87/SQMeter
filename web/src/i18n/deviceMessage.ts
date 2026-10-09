import en from './en.json';
import { t, type MessageKey } from './index';

// Text the device builds (settings errors, safety reasons, alert history, API
// errors) arrives in English. Each is one of the device.* templates in
// en.json, generated from the firmware source by
// tools/i18n/gen_device_catalog.py; the template's key is the message's stable
// ID. Recognising the text here costs no device flash (research.md D4) and
// works with any firmware version: text that matches no template (custom alert
// wording, a newer firmware's message) is shown as sent.

interface Template {
  key: MessageKey;
  pattern: RegExp;
  names: string[];
}

const escape = (text: string) => text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

let templates: Template[] | null = null;

const compile = (): Template[] =>
  Object.entries(en as Record<string, unknown>)
    .filter((entry): entry is [string, string] => entry[0].startsWith('device.') && typeof entry[1] === 'string')
    .map(([key, text]) => {
      const names: string[] = [];
      const source = text
        .split(/(\{\w+\})/)
        .map((part) => {
          const name = /^\{(\w+)\}$/.exec(part)?.[1];
          if (!name) return escape(part);
          names.push(name);
          return '(.+?)';
        })
        .join('');
      return { key: key as MessageKey, pattern: new RegExp(`^${source}$`, 's'), names };
    })
    // Longer templates first, so "Sensor fault: TSL2591 light and MLX90614 IR"
    // wins over a shorter one that could also match.
    .sort((a, b) => b.pattern.source.length - a.pattern.source.length);

/** The device's English text in the UI's language. */
export function deviceText(text: string | null | undefined): string {
  if (!text) return '';
  templates ??= compile();
  for (const template of templates) {
    const match = template.pattern.exec(text);
    if (match) return t(template.key, Object.fromEntries(template.names.map((name, i) => [name, match[i + 1]])));
  }
  return text;
}

/** An error from a device response body ({ error }), translated; `fallback` when there is none. */
export const deviceError = (body: { error?: unknown } | null | undefined, fallback: string) =>
  typeof body?.error === 'string' && body.error ? deviceText(body.error) : fallback;
