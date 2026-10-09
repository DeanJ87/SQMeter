import { describe, expect, it, vi } from 'vitest';
import { fireEvent, render, screen } from '@testing-library/preact';
import AtAGlance from '../AtAGlance';
import type { GlanceItem } from '../glance';

// The at-a-glance area as rendered (specs/025 FR-005..FR-007, FR-020).

const ok = (id: string, text: string): GlanceItem => ({ id, severity: 'ok', priority: 1, text });
const problem = (id: string, text: string, extra: Partial<GlanceItem> = {}): GlanceItem => ({
  id,
  severity: 'problem',
  priority: 5,
  text,
  ...extra,
});

describe('AtAGlance', () => {
  it('is one calm line when nothing is wrong', () => {
    render(<AtAGlance items={[ok('freshness', 'Live'), ok('safety-verdict', 'Safe')]} onAction={() => undefined} />);
    expect(screen.getByText('Live · Safe')).toBeTruthy();
    expect(screen.queryByRole('listitem')).toBeNull();
  });

  // inventory: phone-alarm - the demo is the standard build, so the ringing
  // alarm (Bluetooth build) is checked here: shown with Acknowledge, which acts.
  it('shows a ringing phone alarm with Acknowledge', () => {
    const onAction = vi.fn();
    render(
      <AtAGlance
        items={[
          ok('freshness', 'Live'),
          problem('phone-alarm', 'Phone alarm ringing', { fix: { label: 'Acknowledge', action: 'acknowledge' } }),
        ]}
        onAction={onAction}
      />,
    );
    expect(screen.getByText('Phone alarm ringing').closest('[data-inventory="phone-alarm"]')).toBeTruthy();
    fireEvent.click(screen.getByRole('button', { name: 'Acknowledge' }));
    expect(onAction).toHaveBeenCalledWith('acknowledge');
  });

  it('hides the phone alarm when none is ringing', () => {
    render(<AtAGlance items={[ok('freshness', 'Live')]} onAction={() => undefined} />);
    expect(document.querySelector('[data-inventory="phone-alarm"]')).toBeNull();
  });

  it('offers "Show all" beyond three problems', () => {
    const items = [1, 2, 3, 4].map((n) => problem(`p${n}`, `Problem ${n}`));
    render(<AtAGlance items={items} onAction={() => undefined} />);
    const more = screen.getByRole('button', { name: 'Show all (4)' });
    expect(more.getAttribute('aria-expanded')).toBe('false');
    fireEvent.click(more);
    expect(screen.getByRole('button', { name: 'Show fewer' })).toBeTruthy();
  });
});
