import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import Router from 'preact-router';
import Layout from './components/Layout';
import Dashboard from './components/Dashboard';
import Settings from './components/Settings';
import System from './components/System';
import Updates from './components/Updates';

// Strip the Vite base URL prefix from pathname so preact-router receives
// bare paths like "/" rather than "/SQMeter/demo/" when deployed to a subpath.
const base = (import.meta.env.BASE_URL ?? '/').replace(/\/$/, '');
const getPath = () => window.location.pathname.slice(base.length) || '/';

const App: FunctionalComponent = () => {
  const [url, setUrl] = useState(getPath);

  const handleRoute = ({ url: next }: { url: string }) => {
    setUrl(next);
    history.replaceState(null, '', base + next.replace(/^\//, '/'));
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
