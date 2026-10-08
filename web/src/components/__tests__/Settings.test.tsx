import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, waitFor } from '@testing-library/preact';
import Settings from '../Settings';

describe('Settings', () => {
  afterEach(() => {
    window.history.replaceState(null, '', '/');
  });

  it('scrolls to the section named in ?section= once config loads (Alpaca setup deep link)', async () => {
    const scrollIntoView = vi.fn();
    Element.prototype.scrollIntoView = scrollIntoView;
    window.history.replaceState(null, '', '/settings?section=alpaca');

    const { container } = render(<Settings />);

    await waitFor(() => expect(scrollIntoView).toHaveBeenCalled());
    expect(scrollIntoView.mock.contexts[0]).toBe(container.querySelector('#alpaca'));
  });
});
