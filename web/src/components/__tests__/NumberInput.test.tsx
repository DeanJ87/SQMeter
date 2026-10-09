import { afterEach, describe, expect, it, vi } from 'vitest';
import { fireEvent, render, screen } from '@testing-library/preact';
import { Field, NumberInput } from '../settings/controls';
import { setLanguage, type Messages } from '../../i18n';

// Typed numbers follow the active language and are never truncated (spec 023 FR-017).

const renderInput = (value: number, onChange = vi.fn()) => {
  render(
    <Field label="Threshold">
      <NumberInput value={value} onChange={onChange} />
    </Field>,
  );
  return { input: screen.getByRole('textbox', { name: 'Threshold' }) as HTMLInputElement, onChange };
};

afterEach(() => setLanguage('en', null));

describe('NumberInput', () => {
  it('shows and reads German decimals', () => {
    setLanguage('de', {} as Messages);
    const { input, onChange } = renderInput(-13.5);
    expect(input.value).toBe('-13,5');
    fireEvent.change(input, { target: { value: '21,5' } });
    expect(onChange).toHaveBeenCalledWith(21.5);
  });

  it('keeps text it cannot read, with a message, instead of truncating', () => {
    setLanguage('de', {} as Messages);
    const { input, onChange } = renderInput(1);
    fireEvent.input(input, { target: { value: '1.234' } });
    fireEvent.change(input, { target: { value: '1.234' } });
    expect(onChange).not.toHaveBeenCalled();
    expect(input.value).toBe('1.234');
    expect(input).toHaveAttribute('aria-invalid', 'true');
    expect(input).toHaveAccessibleDescription(/1234,5/);
  });

  it('passes an empty field on as missing', () => {
    const { input, onChange } = renderInput(5);
    fireEvent.change(input, { target: { value: '' } });
    expect(onChange).toHaveBeenCalledWith(Number.NaN);
  });
});
