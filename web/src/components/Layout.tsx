import { FunctionalComponent } from 'preact';
import { useState, useEffect, useRef } from 'preact/hooks';
import { route } from 'preact-router';

interface LayoutProps {
  currentPath?: string;
}

const Layout: FunctionalComponent<LayoutProps> = ({ children, currentPath }) => {
  const [menuOpen, setMenuOpen] = useState(false);
  const menuRef = useRef<HTMLDivElement>(null);

  const navItems = [
    { path: '/', label: 'Dashboard', icon: '📊' },
    { path: '/system', label: 'System', icon: '💻' },
    { path: '/settings', label: 'Settings', icon: '⚙️' },
    { path: '/updates', label: 'Updates', icon: '📦' },
  ];

  const navigate = (path: string) => {
    route(path);
    setMenuOpen(false);
  };

  // Close menu when clicking outside
  useEffect(() => {
    if (!menuOpen) return;
    const handleClickOutside = (e: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(e.target as Node)) {
        setMenuOpen(false);
      }
    };
    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, [menuOpen]);

  return (
    <div class="min-h-screen bg-gray-900">
      {/* Header */}
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

            {/* Hamburger button — mobile only */}
            <button
              class="sm:hidden flex flex-col justify-center items-center w-10 h-10 space-y-1.5 text-gray-300 hover:text-white focus:outline-none"
              onClick={() => setMenuOpen((o) => !o)}
              aria-label="Toggle navigation menu"
              aria-expanded={menuOpen}
            >
              <div class="w-6 h-0.5 bg-current transition-all" />
              <div class="w-6 h-0.5 bg-current transition-all" />
              <div class="w-6 h-0.5 bg-current transition-all" />
            </button>
          </div>
        </div>
      </header>

      {/* Mobile dropdown menu */}
      {menuOpen && (
        <div ref={menuRef} class="sm:hidden absolute z-50 left-0 right-0 bg-gray-800 border-b border-gray-700 shadow-xl">
          {navItems.map((item) => (
            <button
              key={item.path}
              onClick={() => navigate(item.path)}
              class={`w-full flex items-center px-6 py-4 text-sm font-medium transition-colors border-b border-gray-700 last:border-b-0 ${
                currentPath === item.path
                  ? 'text-white bg-gray-700 border-l-4 border-l-blue-500'
                  : 'text-gray-400 hover:text-white hover:bg-gray-700'
              }`}
            >
              <span class="mr-3 text-lg">{item.icon}</span>
              {item.label}
            </button>
          ))}
        </div>
      )}

      {/* Desktop navigation — hidden on mobile */}
      <nav class="hidden sm:block bg-gray-800 border-b border-gray-700">
        <div class="container mx-auto px-4">
          <div class="flex space-x-1">
            {navItems.map((item) => (
              <button
                key={item.path}
                onClick={() => navigate(item.path)}
                class={`px-4 py-3 text-sm font-medium transition-colors ${
                  currentPath === item.path
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

      {/* Main Content */}
      <main class="container mx-auto px-4 py-6">
        {children}
      </main>

      {/* Footer */}
      <footer class="bg-gray-800 border-t border-gray-700 mt-12">
        <div class="container mx-auto px-4 py-4 text-center text-sm text-gray-400">
          <p>ESP32 Dark Sky Monitoring System • Built with Preact + TypeScript</p>
        </div>
      </footer>
    </div>
  );
};

export default Layout;
