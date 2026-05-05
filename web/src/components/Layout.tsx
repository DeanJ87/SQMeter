import { FunctionalComponent } from 'preact';
import { route, useRouter } from 'preact-router';

interface LayoutProps {
  path?: string;
}

const Layout: FunctionalComponent<LayoutProps> = ({ children }) => {
  const [router] = useRouter();
  const navItems = [
    { path: '/', label: 'Dashboard', icon: '📊' },
    { path: '/system', label: 'System', icon: '💻' },
    { path: '/settings', label: 'Settings', icon: '⚙️' },
    { path: '/updates', label: 'Updates', icon: '📦' },
  ];

  return (
    <div class="min-h-screen bg-gray-900">
      <header class="bg-gray-800 border-b border-gray-700 shadow-lg">
        <div class="container mx-auto px-4 py-4">
          <div class="flex items-center justify-between">
            <div class="flex items-center space-x-3">
              <div class="text-3xl">🌌</div>
              <div>
                <h1 class="text-2xl font-bold text-white">SQMeter</h1>
                <p class="text-sm text-gray-400">Dark Sky Monitor</p>
              </div>
            </div>
          </div>
        </div>
      </header>

      <nav class="bg-gray-800 border-b border-gray-700">
        <div class="container mx-auto px-4">
          <div class="flex space-x-1">
            {navItems.map((item) => (
              <button
                key={item.path}
                onClick={() => route(item.path)}
                class={`px-4 py-3 text-sm font-medium transition-colors ${
                  router.url === item.path
                    ? 'text-white bg-gray-700 border-b-2 border-blue-500'
                    : 'text-gray-400 hover:text-white hover:bg-gray-750'
                }`}
              >
                <span class="mr-2">{item.icon}</span>
                {item.label}
              </button>
            ))}
          </div>
        </div>
      </nav>

      <main class="container mx-auto px-4 py-6">
        {children}
      </main>

      <footer class="bg-gray-800 border-t border-gray-700 mt-12">
        <div class="container mx-auto px-4 py-4 text-center text-sm text-gray-400">
          <p>ESP32 Dark Sky Monitoring System • Built with Preact + TypeScript</p>
        </div>
      </footer>
    </div>
  );
};

export default Layout;
