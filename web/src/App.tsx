import { FunctionalComponent } from 'preact';
import { useState, useEffect } from 'preact/hooks';
import Router from 'preact-router';
import Layout from './components/Layout';
import Dashboard from './components/Dashboard';
import Settings from './components/Settings';
import System from './components/System';
import Updates from './components/Updates';

// Demo is hosted on GitHub Pages which has no server-side routing support.
// Hash routing means the hash fragment is never sent to the server, so hard
// refreshing /SQMeter/demo/#/settings always loads /SQMeter/demo/ (which
// exists), then the client reads the hash and renders the correct page.
//
// Production (real ESP32) serves the app at the device root so plain path
// routing works fine there — no base-path stripping needed.
const isDemo = import.meta.env.VITE_DEMO_MODE === 'true';

const getUrl = (): string => {
  if (isDemo) {
    return window.location.hash.replace(/^#/, '') || '/';
  }
  return window.location.pathname || '/';
};

const App: FunctionalComponent = () => {
  const [url, setUrl] = useState(getUrl);

  useEffect(() => {
    if (!isDemo) return;
    const onHashChange = () => {
      const next = getUrl();
      setUrl((prev) => (prev === next ? prev : next));
    };
    window.addEventListener('hashchange', onHashChange);
    return () => window.removeEventListener('hashchange', onHashChange);
  }, []);

  const handleRoute = ({ url: next }: { url: string }) => {
    setUrl((prev) => {
      if (prev === next) return prev;
      if (!isDemo) history.replaceState(null, '', next);
      return next;
    });
  };

  return (
    <Router url={url} onChange={handleRoute}>
      <Layout path="/" currentPath={url}>
        <Dashboard />
      </Layout>
      <Layout path="/settings" currentPath={url}>
        <Settings />
      </Layout>
      <Layout path="/system" currentPath={url}>
        <System />
      </Layout>
      <Layout path="/updates" currentPath={url}>
        <Updates />
      </Layout>
    </Router>
  );
};

export default App;
