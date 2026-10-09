import { afterEach, describe, expect, it } from 'vitest';
import { isolateLatin, setLanguage, t, type Messages } from '../index';

const FSI = '⁨';
const PDI = '⁩';

describe('right-to-left isolation', () => {
  afterEach(() => setLanguage('en', null));

  it('isolates product names with edge punctuation', () => {
    expect(isolateLatin('غير مشترك مع N.I.N.A. (Alpaca متوقف)')).toBe(`غير مشترك مع ${FSI}N.I.N.A.${PDI} (${FSI}Alpaca${PDI} متوقف)`);
  });

  it('keeps a multi-word Latin run and its numbers together', () => {
    expect(isolateLatin('عطل في المستشعر: MLX90614 IR')).toBe(`عطل في المستشعر: ${FSI}MLX90614 IR${PDI}`);
  });

  it('does not isolate a run twice', () => {
    expect(isolateLatin(isolateLatin('منذ 16 ث'))).toBe(`منذ ${FSI}16${PDI} ث`);
  });

  it('isolates in Arabic but leaves left-to-right languages untouched', () => {
    const messages = { 'safety.unsafe': 'غير آمن - N.I.N.A.' } as Messages;
    setLanguage('ar', messages);
    expect(t('safety.unsafe' as never)).toBe(`غير آمن - ${FSI}N.I.N.A.${PDI}`);
    setLanguage('es', { 'safety.unsafe': 'Inseguro - N.I.N.A.' } as Messages);
    expect(t('safety.unsafe' as never)).toBe('Inseguro - N.I.N.A.');
  });
});
