import { describe, it, expect, vi, beforeAll } from 'vitest';
import { fireEvent, render, screen } from '@testing-library/preact';
import Masonry, { mergeOrder, moveInOrder } from '../Masonry';

beforeAll(() => {
  if (!('ResizeObserver' in globalThis)) {
    (globalThis as any).ResizeObserver = class {
      observe() {}
      disconnect() {}
    };
  }
});

describe('card order', () => {
  it('slots cards the saved order has never seen in at their default place', () => {
    expect(mergeOrder(['rain', 'safety'], ['safety', 'sky', 'rain'])).toEqual(['rain', 'safety', 'sky']);
    expect(mergeOrder(['gone', 'sky'], ['safety', 'sky'])).toEqual(['safety', 'sky']);
  });

  it('moves among visible cards and leaves hidden ones in place', () => {
    // wind is hidden (sensor off); moving rain to the front keeps wind third
    const order = ['safety', 'sky', 'wind', 'rain'];
    expect(moveInOrder(order, ['safety', 'sky', 'rain'], 'rain', 0)).toEqual(['rain', 'safety', 'wind', 'sky']);
  });
});

describe('Masonry', () => {
  const items = [
    { id: 'a', title: 'Alpha', node: <p>A</p> },
    { id: 'b', title: 'Beta', node: <p>B</p> },
  ];

  it('only shows arrange controls while editing', () => {
    const { rerender } = render(<Masonry items={items} />);
    expect(screen.queryByLabelText('Drag Alpha')).toBeNull();
    rerender(<Masonry items={items} editing />);
    expect(screen.getByLabelText('Drag Alpha')).toBeInTheDocument();
  });

  it('moves a card one place with the arrow buttons', () => {
    const onMove = vi.fn();
    render(<Masonry items={items} editing onMove={onMove} />);
    expect(screen.getByLabelText('Move Alpha earlier')).toBeDisabled();
    fireEvent.click(screen.getByLabelText('Move Alpha later'));
    expect(onMove).toHaveBeenCalledWith('a', 1);
  });
});
