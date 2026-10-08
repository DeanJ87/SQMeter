import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { Button, Note } from '../components/ui';
import DemoImagingApp from './DemoImagingApp';
import { demoDevice } from './device';
import DeviceReadout from './panel/DeviceReadout';
import SensorInputs from './panel/SensorInputs';
import Shortcuts from './panel/Shortcuts';
import TimePlace from './panel/TimePlace';

// The demo's own controls (spec 019): what each sensor reports, what the
// device makes of it, shortcuts worked out from its settings, and its date,
// time and place. Only in the demo build (mounted from main.tsx).

const DemoPanel: FunctionalComponent = () => {
  const [open, setOpen] = useState(false);
  const [, setTick] = useState(0);

  useEffect(() => demoDevice.onChange(() => setTick((n) => n + 1)), []);
  useEffect(() => {
    document.body.classList.add('has-demo-panel');
    return () => document.body.classList.remove('has-demo-panel');
  }, []);

  return (
    <div class={`demo-panel${open ? ' is-open' : ''}`}>
      {open && (
        <section class="demo-panel-body" aria-label="Demo controls">
          <div class="demo-panel-head">
            <h2>Demo</h2>
            <Button variant="link" onClick={() => setOpen(false)}>
              Close
            </Button>
          </div>
          <Note>Set what each sensor reports; the device's own code works out the rest. Nothing is sent anywhere.</Note>
          <DeviceReadout />
          <Shortcuts />
          <SensorInputs />
          <TimePlace />
          <DemoImagingApp />
          <div class="btn-row">
            <Button small variant="danger" onClick={() => demoDevice.reset()}>
              Reset demo
            </Button>
          </div>
        </section>
      )}
      <button type="button" class="demo-panel-toggle" aria-expanded={open} onClick={() => setOpen(!open)}>
        <span aria-hidden="true">✦</span> Demo
      </button>
    </div>
  );
};

export default DemoPanel;
