import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import { Button, Note } from '../components/ui';
import { imagingApp } from './imagingApp';

// Demo panel section: a simulated imaging app to try the "imaging app
// stopped checking" alerts and "Only while an imaging app is connected".
// Self-contained so any Demo panel layout can host it.

const STATE_TEXT = {
  off: 'Not connected.',
  checking: 'Connected - checking the safety monitor every 3 s and the weather every minute.',
  silent: 'Connected but silent - as if it crashed. The device notices after the "Silent for" time.',
};

const DemoImagingApp: FunctionalComponent = () => {
  const [, setTick] = useState(0);
  const act = (action: () => void) => () => {
    action();
    setTick((n) => n + 1);
  };
  const state = imagingApp.state;
  return (
    <div class="card-group" aria-label="Imaging app">
      <h3 class="card-group-title">Imaging app</h3>
      <Note>{STATE_TEXT[state]}</Note>
      <div class="btn-row">
        {state === 'off' ? (
          <Button small onClick={act(() => imagingApp.connect())}>
            Connect
          </Button>
        ) : (
          <>
            {state === 'checking' ? (
              <Button small onClick={act(() => imagingApp.goSilent())}>
                Go silent
              </Button>
            ) : (
              <Button small onClick={act(() => imagingApp.resume())}>
                Resume checking
              </Button>
            )}
            <Button small onClick={act(() => imagingApp.disconnect())}>
              Disconnect
            </Button>
          </>
        )}
      </div>
    </div>
  );
};

export default DemoImagingApp;
