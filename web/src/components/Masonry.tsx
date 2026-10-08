import { ComponentChildren, FunctionalComponent } from 'preact';
import { useLayoutEffect, useRef, useState } from 'preact/hooks';

export interface MasonryItem {
  id: string;
  title: string;
  node: ComponentChildren;
}

// Pinterest-style layout: items keep their order, each going into the
// shortest column. Items are absolutely positioned inside one container, so
// moving between columns animates and never remounts a card.
//
// In `editing` mode each item gets a bar to drag it (mouse or touch) or move
// it one place earlier/later; `onMove(id, toIndex)` reports the new place.
const Masonry: FunctionalComponent<{
  items: MasonryItem[];
  minColumnWidth?: number;
  gap?: number;
  editing?: boolean;
  onMove?: (id: string, toIndex: number) => void;
}> = ({ items, minColumnWidth = 320, gap = 16, editing = false, onMove }) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const [width, setWidth] = useState(0);
  const [heights, setHeights] = useState<Record<string, number>>({});
  const [dragging, setDragging] = useState<string | null>(null);
  // The item last swapped with; ignored until the pointer leaves it, so two
  // cards of different heights don't keep trading places.
  const lastSwap = useRef<string | null>(null);

  useLayoutEffect(() => {
    const container = containerRef.current;
    if (!container) return;
    const observer = new ResizeObserver(() => setWidth(container.clientWidth));
    observer.observe(container);
    setWidth(container.clientWidth);
    return () => observer.disconnect();
  }, []);

  // Re-measure whenever any item's content changes height.
  useLayoutEffect(() => {
    const container = containerRef.current;
    if (!container) return;
    const measure = () => {
      const next: Record<string, number> = {};
      container.querySelectorAll<HTMLElement>('[data-masonry-id]').forEach((el) => {
        next[el.dataset.masonryId!] = el.offsetHeight;
      });
      setHeights((previous) => {
        const keys = Object.keys(next);
        const same = keys.length === Object.keys(previous).length && keys.every((key) => previous[key] === next[key]);
        return same ? previous : next;
      });
    };
    // Measure on the next frame: re-laying out inside the observer callback
    // would trigger "ResizeObserver loop" warnings.
    let frame = 0;
    const observer = new ResizeObserver(() => {
      cancelAnimationFrame(frame);
      frame = requestAnimationFrame(measure);
    });
    container.querySelectorAll('[data-masonry-id]').forEach((el) => observer.observe(el));
    measure();
    return () => {
      cancelAnimationFrame(frame);
      observer.disconnect();
    };
  }, [items.map((item) => item.id).join(','), editing]);

  const columns = Math.max(1, Math.floor((width + gap) / (minColumnWidth + gap)));
  const columnWidth = columns > 0 ? (width - gap * (columns - 1)) / columns : width;
  const columnHeights = new Array(columns).fill(0);
  const positions: Record<string, { x: number; y: number }> = {};
  for (const item of items) {
    let column = 0;
    for (let c = 1; c < columns; c++) if (columnHeights[c] < columnHeights[column] - 1) column = c;
    positions[item.id] = { x: column * (columnWidth + gap), y: columnHeights[column] };
    columnHeights[column] += (heights[item.id] ?? 0) + gap;
  }
  const measured = width > 0 && items.every((item) => heights[item.id] !== undefined);

  // Dragging: while the pointer is over another item, take its place.
  const startDrag = (id: string, event: PointerEvent) => {
    event.preventDefault();
    (event.currentTarget as HTMLElement).setPointerCapture(event.pointerId);
    lastSwap.current = null;
    setDragging(id);
  };
  const drag = (id: string, event: PointerEvent) => {
    if (dragging !== id || !containerRef.current) return;
    const box = containerRef.current.getBoundingClientRect();
    const x = event.clientX - box.left;
    const y = event.clientY - box.top;
    const target = items.findIndex((item) => {
      const p = positions[item.id];
      return item.id !== id && x >= p.x && x < p.x + columnWidth && y >= p.y && y < p.y + (heights[item.id] ?? 0);
    });
    const targetId = target >= 0 ? items[target].id : null;
    if (targetId !== null && targetId !== lastSwap.current) onMove?.(id, target);
    lastSwap.current = targetId;
  };

  return (
    <div
      ref={containerRef}
      class={`masonry${measured ? ' is-measured' : ''}${editing ? ' is-editing' : ''}`}
      style={{ height: `${Math.max(0, Math.max(...columnHeights) - gap)}px` }}
    >
      {items.map((item, index) => (
        <div
          key={item.id}
          data-masonry-id={item.id}
          class={`masonry-item${dragging === item.id ? ' is-dragging' : ''}`}
          style={{ width: `${columnWidth}px`, transform: `translate(${positions[item.id].x}px, ${positions[item.id].y}px)` }}
        >
          {editing && (
            <div class="arrange-bar">
              <button
                type="button"
                class="arrange-handle"
                aria-label={`Drag ${item.title}`}
                onPointerDown={(e) => startDrag(item.id, e)}
                onPointerMove={(e) => drag(item.id, e)}
                onPointerUp={() => setDragging(null)}
                onPointerCancel={() => setDragging(null)}
              >
                <span aria-hidden="true">⠿</span> {item.title}
              </button>
              <button
                type="button"
                class="btn btn-ghost btn-sm"
                aria-label={`Move ${item.title} earlier`}
                disabled={index === 0}
                onClick={() => onMove?.(item.id, index - 1)}
              >
                ←
              </button>
              <button
                type="button"
                class="btn btn-ghost btn-sm"
                aria-label={`Move ${item.title} later`}
                disabled={index === items.length - 1}
                onClick={() => onMove?.(item.id, index + 1)}
              >
                →
              </button>
            </div>
          )}
          {item.node}
        </div>
      ))}
    </div>
  );
};

export default Masonry;

// The saved order, with ids it doesn't know yet (new cards) placed right
// after the card that precedes them by default.
export const mergeOrder = (saved: string[], defaults: string[]) => {
  const order = saved.filter((id) => defaults.includes(id));
  defaults.forEach((id, index) => {
    if (order.includes(id)) return;
    const before = defaults
      .slice(0, index)
      .reverse()
      .find((other) => order.includes(other));
    order.splice(before === undefined ? 0 : order.indexOf(before) + 1, 0, id);
  });
  return order;
};

// Move `id` to `toIndex` among the visible ids, keeping hidden ids (cards
// for sensors that are off) where they were in the full order.
export const moveInOrder = (order: string[], visible: string[], id: string, toIndex: number) => {
  const shown = visible.filter((v) => v !== id);
  shown.splice(Math.max(0, Math.min(toIndex, shown.length)), 0, id);
  const slots = new Set(visible);
  let next = 0;
  return order.map((v) => (slots.has(v) ? shown[next++] : v));
};
