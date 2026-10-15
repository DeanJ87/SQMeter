import { FunctionalComponent } from 'preact';
import { useEffect, useRef, useState } from 'preact/hooks';
import { useDialogFocus } from '../lib/a11y';
import { Button, Note } from '../components/ui';
import DemoImagingApp from './DemoImagingApp';
import { demoDevice } from './device';
import DeviceReadout from './panel/DeviceReadout';
import SensorInputs from './panel/SensorInputs';
import Shortcuts from './panel/Shortcuts';
import RealNotifications from './panel/RealNotifications';
import TimePlace from './panel/TimePlace';
import { tour } from './tour/tourState';

// The demo's own controls (spec 019): what each sensor reports, what the
// device makes of it, shortcuts worked out from its settings, and its date,
// time and place. Only in the demo build (mounted from main.tsx).

// The flat SQMeter mark, for 17 to 39 px (brand/README.md), inlined at 18 px.
// Decorative: the button's name stays "Demo". Source: brand/src/logo/sqmeter-mark-flat.svg.
const DemoMark: FunctionalComponent = () => (
  <svg class="demo-panel-mark" width="18" height="18" viewBox="0 0 24 24" aria-hidden="true" focusable="false">
    <defs>
      <clipPath id="sqm-demo-mark-clip">
        <rect x="2.25" y="2.25" width="19.5" height="19.5" rx="2.75" />
      </clipPath>
    </defs>
    <g clip-path="url(#sqm-demo-mark-clip)">
      <rect x="2.25" y="2.25" width="19.5" height="19.5" fill="#1d2f41" />
      <circle cx="5.2" cy="5.6" r=".6" fill="#8b9baa" />
      <circle cx="9.4" cy="4.6" r=".6" fill="#8b9baa" />
      <path d="M12.8 13.1L13.67 3.14A10 10 0 0 1 20.99 7.36Z" fill="#55c7f2" />
      <path d="M2.25 19.7Q7.5 18.75 12 19.35T21.75 18.9V21.75H2.25Z" fill="#13212f" />
      <ellipse cx="7.1" cy="12.45" rx="1.5" ry="1.7" transform="rotate(14 7.1 12.45)" fill="#060b11" />
      <path d="M6.45 13.8H7.55V15.5H6.45Z" fill="#060b11" />
      <path d="M5.25 21.75L4.5 17.1Q4.3 15.5 5.8 15.2H8.2Q9.6 15.4 9.5 16.9L8.75 21.75Z" fill="#060b11" />
      <path
        d="M9 15.9L10.75 16.15L12.15 13.85"
        fill="none"
        stroke="#060b11"
        stroke-width="1.35"
        stroke-linecap="round"
        stroke-linejoin="round"
      />
      <path d="M11.85 14.15L12.8 13.1" fill="none" stroke="#060b11" stroke-width="1.1" stroke-linecap="round" />
    </g>
  </svg>
);

// The demo device's clock isn't the real time (spec 019): say so where the demo is named (spec 026).
const ClockMoved: FunctionalComponent = () =>
  Math.abs(demoDevice.now.valueOf() - Date.now()) > 60_000 ? (
    <span class="demo-clock-moved" data-inventory="demo-marker">
      Clock moved
    </span>
  ) : null;

const DemoPanel: FunctionalComponent = () => {
  const [open, setOpen] = useState(false);
  const [, setTick] = useState(0);
  // Non-modal: focus moves in on open, Escape closes and focus returns to the
  // Demo button (spec 022 FR-012).
  const body = useRef<HTMLElement>(null);
  const toggle = useRef<HTMLButtonElement>(null);
  useDialogFocus(open, body, toggle);

  useEffect(() => demoDevice.onChange(() => setTick((n) => n + 1)), []);
  useEffect(() => {
    document.body.classList.add('has-demo-panel');
    return () => document.body.classList.remove('has-demo-panel');
  }, []);

  return (
    <div class={`demo-panel${open ? ' is-open' : ''}`} onKeyDown={(event) => event.key === 'Escape' && setOpen(false)}>
      {open && (
        <section class="demo-panel-body" aria-label="Demo controls" ref={body}>
          <div class="demo-panel-head">
            <h2 tabIndex={-1} data-autofocus>
              Demo
            </h2>
            <Button variant="link" onClick={() => setOpen(false)}>
              Close
            </Button>
          </div>
          <Note>
            Set what each sensor reports; the device's own code works out the rest. Nothing is sent anywhere unless you turn on Real
            notifications.
          </Note>
          <DeviceReadout />
          <Shortcuts />
          <SensorInputs />
          <TimePlace />
          <DemoImagingApp />
          <RealNotifications />
          <div class="btn-row">
            <Button
              small
              onClick={() => {
                setOpen(false);
                tour.start();
              }}
            >
              Take the tour
            </Button>
            <Button small variant="danger" onClick={() => demoDevice.reset()}>
              Reset demo
            </Button>
          </div>
        </section>
      )}
      <button ref={toggle} type="button" class="demo-panel-toggle" aria-expanded={open} onClick={() => setOpen(!open)}>
        <DemoMark /> Demo
        <ClockMoved />
      </button>
    </div>
  );
};

export default DemoPanel;
