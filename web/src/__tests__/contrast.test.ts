/// <reference types="node" />
import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';

const css = readFileSync(new URL('../index.css', import.meta.url), 'utf8');

// Every text colour token meets 4.5:1 and control edges 3:1 against every
// background token (spec 022 SC-005, A11Y-01). Parsed from index.css so a
// token change can't slip past.

const root = css.slice(css.indexOf(':root'), css.indexOf('}', css.indexOf(':root')));
const tokens = Object.fromEntries([...root.matchAll(/--([a-z0-9-]+):\s*(#[0-9a-f]{6})/gi)].map((m) => [m[1], m[2]]));

const luminance = (hex: string) => {
  const [r, g, b] = [1, 3, 5]
    .map((i) => parseInt(hex.slice(i, i + 2), 16) / 255)
    .map((v) => (v <= 0.03928 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4));
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
};
const ratio = (a: string, b: string) => {
  const [x, y] = [luminance(a), luminance(b)];
  return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05);
};

const BACKGROUNDS = ['bg', 'panel', 'panel-2', 'panel-3'];
const TEXT = ['text', 'muted', 'dim', 'cyan', 'violet', 'green', 'amber', 'red'];

describe('colour tokens', () => {
  it('has every token this test checks', () => {
    for (const name of [...BACKGROUNDS, ...TEXT, 'control-edge']) expect(tokens[name], `--${name}`).toBeDefined();
  });

  for (const fg of TEXT)
    for (const bg of BACKGROUNDS)
      it(`--${fg} text on --${bg} is at least 4.5:1`, () => {
        expect(ratio(tokens[fg], tokens[bg])).toBeGreaterThanOrEqual(4.5);
      });

  for (const bg of BACKGROUNDS)
    it(`--control-edge on --${bg} is at least 3:1`, () => {
      expect(ratio(tokens['control-edge'], tokens[bg])).toBeGreaterThanOrEqual(3);
    });
});
