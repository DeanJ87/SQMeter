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
        <span aria-hidden="true">✦</span> Demo
        <ClockMoved />
      </button>
    </div>
  );
};

export default DemoPanel;
