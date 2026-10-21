import { describe, it, expect } from 'vitest';
import { render } from '@testing-library/preact';
import { TextInput } from '../settings/controls';

// Secret fields must not be autofilled: a browser's saved password would replace the device's mask
// and the next unrelated save would store it.

describe('TextInput', () => {
  it('asks browsers and password managers not to fill secret fields', () => {
    const { container } = render(<TextInput type="password" value="********" onInput={() => {}} />);
    const input = container.querySelector('input')!;
    expect(input).toHaveAttribute('autocomplete', 'new-password');
    expect(input).toHaveAttribute('data-1p-ignore');
    expect(input).toHaveAttribute('data-lpignore', 'true');
  });

  it('leaves ordinary text fields alone', () => {
    const { container } = render(<TextInput value="broker.local" onInput={() => {}} />);
    expect(container.querySelector('input')).not.toHaveAttribute('autocomplete');
  });
});
