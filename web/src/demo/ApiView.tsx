import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';

// Device URLs opened directly in the demo - /management/v1/description,
// /api/v1/safetymonitor/0/issafe, /api/sensors, ... - arrive at 404.html (the
// host has no such files). Show what the emulated device answers, like a
// browser pointed at a real SQMeter would.

const base = import.meta.env.BASE_URL.replace(/\/$/, '');

const devicePath = (location: Location) => {
  const path = location.pathname.startsWith(base) ? location.pathname.slice(base.length) || '/' : location.pathname;
  return path;
};

const ApiView: FunctionalComponent<{ path: string; search: string }> = ({ path, search }) => {
  const [result, setResult] = useState<{ status: number; type: string; body: string } | null>(null);

  useEffect(() => {
    fetch(`${base}${path}${search}`)
      .then(async (response) => {
        const text = await response.text();
        const type = response.headers.get('Content-Type') ?? '';
        let body = text;
        try {
          if (type.includes('json')) body = JSON.stringify(JSON.parse(text), null, 2);
        } catch {
          // not JSON after all: show as is
        }
        setResult({ status: response.status, type, body });
      })
      .catch(() => setResult({ status: 0, type: 'text/plain', body: 'The demo device did not answer.' }));
  }, [path, search]);

  return (
    <div class="api-view">
      <p class="api-view-head">
        <span class="pill pill-cyan">Demo</span>{' '}
        <code>
          GET {path}
          {search}
        </code>
        {result && <span class="muted"> · HTTP {result.status}</span>}
      </p>
      <pre>{result ? result.body : 'Asking the demo device...'}</pre>
      <p>
        <a href={`${base}/`}>Back to the demo</a>
      </p>
    </div>
  );
};

/** The view for a device URL opened directly, or null for the normal app. */
export function deviceUrlView(location: Location) {
  const path = devicePath(location);
  if (path === '/setup' || path.startsWith('/setup/')) {
    // The device sends Alpaca "Setup" links to its Safety settings.
    location.replace(`${base}/#/settings?tab=safety`);
    return <div class="api-view">Opening the Alpaca settings...</div>;
  }
  if (path.startsWith('/api/') || path.startsWith('/management/')) return <ApiView path={path} search={location.search} />;
  return null;
}
