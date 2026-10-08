import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { Button, Note } from '../components/ui';
import { demoDevice } from './device';
import { SCENARIOS, scenarioActive, scenarioRemainingMs } from './simulator';

// The demo's own controls: weather and fault scenarios, demo speed, reset.
// Only in the demo build (mounted from main.tsx).

const formatRemaining = (ms: number) => {
  const s = Math.ceil(ms / 1000);
  return s >= 60 ? `${Math.floor(s / 60)}m ${s % 60}s` : `${s}s`;
};

const DemoPanel: FunctionalComponent = () => {
  const [open, setOpen] = useState(false);
  const [, setTick] = useState(0);

  useEffect(() => demoDevice.onChange(() => setTick((n) => n + 1)), []);

  const active = scenarioActive(demoDevice.scenario, demoDevice.nowMs) ? demoDevice.scenario : null;
  const remaining = scenarioRemainingMs(active, demoDevice.nowMs);

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
          <Note>
            A simulated SQMeter running the real firmware's logic in your browser. Settings work; nothing is sent anywhere.
          </Note>
          <div class="demo-scenarios">
            {SCENARIOS.map((scenario) => (
              <Button
                key={scenario.id}
                small
                variant={active?.id === scenario.id ? 'primary' : 'default'}
                title={scenario.hint}
                onClick={() => demoDevice.startScenario(scenario.id)}
              >
                {scenario.label}
              </Button>
            ))}
          </div>
          {active && (
            <Note tone="ok">
              {SCENARIOS.find((s) => s.id === active.id)?.label} - {formatRemaining(remaining)} left
            </Note>
          )}
          <label class="demo-speed">
            <input
              type="checkbox"
              checked={demoDevice.timeMultiplier === 10}
              onChange={(e) => demoDevice.setTimeMultiplier((e.target as HTMLInputElement).checked ? 10 : 1)}
            />
            Run the device clock 10× faster (rain clear delay, safe delay, cooldowns)
          </label>
          <div class="btn-row">
            <Button small variant="danger" onClick={() => demoDevice.reset()}>
              Reset demo
            </Button>
          </div>
        </section>
      )}
      <button type="button" class="demo-panel-toggle" aria-expanded={open} onClick={() => setOpen(!open)}>
        <span aria-hidden="true">✦</span> Demo{active ? ` · ${SCENARIOS.find((s) => s.id === active.id)?.label}` : ''}
      </button>
    </div>
  );
};

export default DemoPanel;
