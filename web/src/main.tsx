import { render } from 'preact';
import { loadLanguage } from './i18n/loader';
import './index.css';

// The app is imported after the language is installed, so labels built when
// a module loads are translated too (specs/023-i18n research D2).
async function init() {
  if (import.meta.env.VITE_DEMO_MODE === 'true') {
    try {
      // The emulated device (the firmware's own logic in WebAssembly) answers
      // the UI's requests; see specs/016-demo-device-emulation.
      const { demoDevice } = await import('./demo/device');
      await demoDevice.start();
      const { worker } = await import('./demo/browser');
      await worker.start({
        serviceWorker: {
          url: import.meta.env.BASE_URL + 'mockServiceWorker.js',
        },
        onUnhandledRequest: 'bypass',
      });
    } catch (e) {
      console.warn('[Demo] MSW failed to start:', e);
    }
  }

  // Device URLs opened directly in the demo (served by 404.html): show what
  // the emulated device answers instead of the host's Not Found page.
  if (import.meta.env.VITE_DEMO_MODE === 'true') {
    const { deviceUrlView } = await import('./demo/ApiView');
    const view = deviceUrlView(window.location);
    if (view) {
      render(view, document.getElementById('app')!);
      return;
    }
  }

  await loadLanguage();
  const { default: App } = await import('./App');

  if (import.meta.env.VITE_DEMO_MODE === 'true') {
    const { default: DemoPanel } = await import('./demo/DemoPanel');
    const { default: Tour } = await import('./demo/tour/Tour');
    // ?panel=hidden leaves the Demo button out (screenshots for the docs).
    const showPanel = new URLSearchParams(window.location.search).get('panel') !== 'hidden';
    render(
      <>
        <App />
        {showPanel && <DemoPanel />}
        {showPanel && <Tour />}
      </>,
      document.getElementById('app')!,
    );
    return;
  }

  render(<App />, document.getElementById('app')!);
}

init();
