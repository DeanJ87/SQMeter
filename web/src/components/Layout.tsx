import { FunctionalComponent } from 'preact';
import { route, useRouter } from 'preact-router';

interface LayoutProps {
  path?: string;
  default?: boolean;
}

const navItems = [
  { path: '/', label: 'Dashboard', icon: 'chart' },
  { path: '/system', label: 'System', icon: 'cpu' },
  { path: '/settings', label: 'Settings', icon: 'gear' },
  { path: '/updates', label: 'Updates', icon: 'upload' },
];

const TinyIcon: FunctionalComponent<{ name: string }> = ({ name }) => {
  const common = {
    stroke: 'currentColor',
    strokeWidth: 1.7,
    fill: 'none',
    strokeLinecap: 'round' as const,
    strokeLinejoin: 'round' as const,
  };
  const paths = {
    chart: <path {...common} d="M4 17 9 11l4 4 7-8M15 7h5v5" />,
    cpu: <><rect {...common} x="5" y="5" width="14" height="14" rx="2" /><path {...common} d="M9 1v4M15 1v4M9 19v4M15 19v4M1 9h4M1 15h4M19 9h4M19 15h4" /></>,
    gear: <><circle {...common} cx="12" cy="12" r="3" /><path {...common} d="M19 12a7 7 0 0 0-.1-1.2l2-1.5-2-3.5-2.4 1a7.8 7.8 0 0 0-2-1.2L14.2 3h-4.4l-.4 2.6a7.8 7.8 0 0 0-2 1.2l-2.4-1-2 3.5 2 1.5a7 7 0 0 0 0 2.4l-2 1.5 2 3.5 2.4-1a7.8 7.8 0 0 0 2 1.2l.4 2.6h4.4l.4-2.6a7.8 7.8 0 0 0 2-1.2l2.4 1 2-3.5-2-1.5A7 7 0 0 0 19 12Z" /></>,
    upload: <><path {...common} d="M12 16V4M7 9l5-5 5 5" /><path {...common} d="M5 18v2h14v-2" /></>,
  };

  return (
    <svg class="nav-icon-svg" width="15" height="15" viewBox="0 0 24 24" aria-hidden="true">
      {paths[name as keyof typeof paths]}
    </svg>
  );
};

const Layout: FunctionalComponent<LayoutProps> = ({ children }) => {
  const [router] = useRouter();

  return (
    <div class="app-shell">
      <header class="app-header">
        <div class="app-header-inner">
          <div class="brand">
            <div class="brand-mark" aria-hidden="true">✦</div>
            <div>
              <h1>SQMeter</h1>
              <p>Dark Sky Monitor</p>
            </div>
          </div>

          <nav class="top-nav" aria-label="Primary">
            {navItems.map((item) => (
              <button
                key={item.path}
                type="button"
                onClick={() => route(item.path)}
                class={`nav-button ${router.url === item.path ? 'is-active' : ''}`}
              >
                <TinyIcon name={item.icon} />
                <span>{item.label}</span>
              </button>
            ))}
          </nav>
        </div>
      </header>

      <main class="app-main">
        {children}
      </main>
    </div>
  );
};

export default Layout;
