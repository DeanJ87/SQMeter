import { FunctionalComponent } from 'preact';
import Router from 'preact-router';
import Layout from './components/Layout';
import Dashboard from './components/Dashboard';
import Settings from './components/Settings';
import System from './components/System';
import Updates from './components/Updates';

const App: FunctionalComponent = () => {
  return (
    <Router>
      <Layout path="/" currentPath="/">
        <Dashboard />
      </Layout>
      <Layout path="/settings" currentPath="/settings">
        <Settings />
      </Layout>
      <Layout path="/system" currentPath="/system">
        <System />
      </Layout>
      <Layout path="/updates" currentPath="/updates">
        <Updates />
      </Layout>
    </Router>
  );
};

export default App;
