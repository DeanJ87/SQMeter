import { FunctionalComponent } from 'preact';
import { route } from 'preact-router';
import { useCallback, useEffect, useLayoutEffect, useRef, useState } from 'preact/hooks';
import { Button } from '../../components/ui';
import { demoDevice } from '../device';
import { LINKS, TOUR_STEPS, captureRules, type TourStep } from './steps';
import { tour } from './tourState';

// The guided tour (specs/018 US1): a non-modal card in the app's own style
// that outlines the control it talks about, waits for the device on action
// steps, and ends on Skip or Escape. Keyboard and reduced-motion friendly;
// docked at the bottom on phones.

const MARGIN = 12;
const CARD_WIDTH = 340;
const PHONE_WIDTH = 600;

interface Box {
  top: number;
  left: number;
  width: number;
  height: number;
}

const targetBox = (step: TourStep): Box | null => {
  const el = step.target ? document.querySelector(step.target) : null;
  if (!el) return null;
  const r = el.getBoundingClientRect();
  return r.width > 0 ? { top: r.top, left: r.left, width: r.width, height: r.height } : null;
};

// Below the target if it fits, else above; always inside the viewport.
const cardPosition = (box: Box | null): Record<string, string> => {
  if (window.innerWidth < PHONE_WIDTH) return {};
  if (!box) return { top: '30%', left: `calc(50% - ${CARD_WIDTH / 2}px)` };
  const left = Math.min(Math.max(MARGIN, box.left), window.innerWidth - CARD_WIDTH - MARGIN);
  const below = box.top + box.height + MARGIN;
  const top = below + 220 < window.innerHeight ? below : Math.max(MARGIN, box.top - 220 - MARGIN);
  return { top: `${top}px`, left: `${left}px` };
};

const useRerender = () => {
  const [, setTick] = useState(0);
  return useCallback(() => setTick((n) => n + 1), []);
};

const useStepEntry = (index: number, step: TourStep) => {
  useEffect(() => {
    if (step.route) route(step.route);
    if (step.id === 'change-rule') captureRules();
    if (step.target) document.querySelector(step.target)?.scrollIntoView({ block: 'center', behavior: 'auto' });
  }, [index, step]);
};

// Re-render as the device ticks and the page moves, so the outline follows.
const useFollow = () => {
  const rerender = useRerender();
  useEffect(() => {
    const off = demoDevice.onChange(rerender);
    window.addEventListener('resize', rerender);
    window.addEventListener('scroll', rerender, true);
    return () => {
      off();
      window.removeEventListener('resize', rerender);
      window.removeEventListener('scroll', rerender, true);
    };
  }, [rerender]);
};

const TourActions: FunctionalComponent<{ index: number; step: TourStep; waiting: boolean }> = ({ index, step, waiting }) => {
  const last = index === TOUR_STEPS.length - 1;
  return (
    <div class="btn-row">
      {waiting && step.doIt && (
        <Button small onClick={step.doIt}>
          {step.doItLabel ?? 'Do it for me'}
        </Button>
      )}
      {index > 0 && (
        <Button small onClick={() => tour.go(index - 1)}>
          Back
        </Button>
      )}
      <Button small variant="primary" disabled={waiting} onClick={() => (last ? tour.end('done') : tour.go(index + 1))}>
        {last ? 'Finish' : 'Next'}
      </Button>
      <Button small variant="link" onClick={() => tour.end('dismissed')}>
        Skip tour
      </Button>
    </div>
  );
};

const TourCard: FunctionalComponent<{ index: number }> = ({ index }) => {
  const step = TOUR_STEPS[index];
  const heading = useRef<HTMLHeadingElement>(null);
  useStepEntry(index, step);
  useFollow();
  useLayoutEffect(() => heading.current?.focus(), [index]);

  const box = targetBox(step);
  const waiting = step.done !== undefined && !step.done();
  const last = index === TOUR_STEPS.length - 1;

  return (
    <>
      {box && <div class="tour-outline" style={{ top: box.top - 4, left: box.left - 4, width: box.width + 8, height: box.height + 8 }} />}
      <section
        class="tour-card card"
        role="dialog"
        aria-modal="false"
        aria-labelledby="tour-title"
        style={cardPosition(box)}
        onKeyDown={(e) => e.key === 'Escape' && tour.end('dismissed')}
      >
        <p class="tour-progress">
          Step {index + 1} of {TOUR_STEPS.length}
        </p>
        <h2 id="tour-title" tabIndex={-1} ref={heading}>
          {step.title}
        </h2>
        <p>{step.body()}</p>
        {last && (
          <p class="tour-links">
            {LINKS.map((link) => (
              <a key={link.href} href={link.href} target="_blank" rel="noopener">
                {link.label}
              </a>
            ))}
          </p>
        )}
        {waiting && (
          <p class="note note-muted" aria-live="polite">
            Waiting for the device...
          </p>
        )}
        <TourActions index={index} step={step} waiting={waiting} />
      </section>
    </>
  );
};

const TourOffer: FunctionalComponent = () => (
  <section class="tour-offer card" aria-label="Tour">
    <p>New here? A short tour shows what SQMeter does - about 2 minutes.</p>
    <div class="btn-row">
      <Button small variant="primary" onClick={() => tour.start()}>
        Take the tour
      </Button>
      <Button small variant="link" onClick={() => tour.end('dismissed')}>
        No thanks
      </Button>
    </div>
  </section>
);

const Tour: FunctionalComponent = () => {
  const rerender = useRerender();
  useEffect(() => tour.onChange(rerender), [rerender]);
  if (tour.step !== null) return <TourCard index={tour.step} />;
  return tour.offered ? <TourOffer /> : null;
};

export default Tour;
