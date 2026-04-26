import { FunctionalComponent } from 'preact';
import Router from 'preact-router';
import type { CustomHistory } from 'preact-router';
import Layout from './components/Layout';
import Dashboard from './components/Dashboard';
import Settings from './components/Settings';
import System from './components/System';
import Updates from './components/Updates';

const isDemo = import.meta.env.VITE_DEMO_MODE === 'true';

const hashHistory: CustomHistory | undefined = isDemo ? {
  get location() {
    return { pathname: window.location.hash.replace(/^#/, '') || '/' } as Location;
  },
  push(path: string) { window.location.hash = path; },
  replace(path: string) { window.location.hash = path; },
  listen(cb: (loc: Location) => void) {
    const h = () => cb({ pathname: window.location.hash.replace(/^#/, '') || '/' } as Location);
    window.addEventListener('hashchange', h);
    return () => window.removeEventListener('hashchange', h);
  },
} : undefined;

const App: FunctionalComponent = () => (
  <Router history={hashHistory}>
    <Layout path="/">
      <Dashboard />
    </Layout>
    <Layout path="/settings">
      <Settings />
    </Layout>
    <Layout path="/system">
      <System />
    </Layout>
    <Layout path="/updates">
      <Updates />
    </Layout>
  </Router>
);

export default App;
