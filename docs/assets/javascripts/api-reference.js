// Renders docs/api/openapi.json on the API reference page (spec 028 FR-005)
// with the self-hosted Scalar build (assets/javascripts/vendor/scalar.js,
// copied in by web/scripts/vendor-scalar.mjs). Read-only: "Try it" lives on
// the demo site's explorer, where the emulated device can answer (FR-010).
(function () {
  var mount = document.getElementById('sqm-api-reference');
  if (!mount || !window.Scalar) return;

  // The docs site's palette and fonts (stylesheets/sqmeter.css).
  var theme = [
    '.dark-mode {',
    '  --scalar-background-1: #080d14; --scalar-background-2: #0d141d; --scalar-background-3: #111b27;',
    '  --scalar-background-accent: rgba(85, 199, 242, 0.1);',
    '  --scalar-color-1: #e7edf3; --scalar-color-2: #8b9baa; --scalar-color-3: #768798;',
    '  --scalar-color-accent: #55c7f2; --scalar-color-green: #5ee0a0; --scalar-color-red: #f26b62;',
    '  --scalar-color-yellow: #f4c15d; --scalar-color-blue: #55c7f2; --scalar-color-orange: #f4c15d;',
    '  --scalar-color-purple: #a78bfa; --scalar-border-color: #1b2a38;',
    '}',
    ':root { --scalar-font: "Inter SQM", ui-sans-serif, system-ui, sans-serif;',
    '  --scalar-font-code: "JetBrains Mono SQM", ui-monospace, monospace; }',
    'li[data-sidebar-id] .group\\/group-button > button[aria-expanded] { min-width: 24px; min-height: 24px; }',
    '.dark-mode { --scalar-sidebar-search-background: #0d141d; --scalar-sidebar-search-color: #8b9baa;',
    '  --scalar-sidebar-search-border-color: #1b2a38; }',
    '.sidebar-search, .sidebar-search-placeholder { background: #0d141d; color: #8b9baa; }',
    '.scalar-mcp-layer-link { display: none !important; }',
  ].join('\n');

  window.Scalar.createApiReference(mount, {
    // Relative to this page, so the reference keeps working if the docs move.
    url: new URL(mount.dataset.url, window.location.href).href,
    layout: 'modern',
    showSidebar: false,
    hideTestRequestButton: true,
    hideClientButton: true,
    hideDarkModeToggle: true,
    forceDarkModeState: 'dark',
    theme: 'none',
    customCss: theme,
    withDefaultFonts: false,
    telemetry: false,
    agent: { disabled: true },
    mcp: { disabled: true },
    showDeveloperTools: 'never',
    documentDownloadType: 'json',
    defaultHttpClient: { targetKey: 'shell', clientKey: 'curl' },
  });

  // Scalar's icon-only buttons with no accessible name (WCAG 4.1.2).
  var name = function () {
    mount.querySelectorAll('button:not([aria-label])').forEach(function (button) {
      if (button.innerText.trim()) return;
      if (button.classList.contains('scalar-code-copy')) button.setAttribute('aria-label', 'Copy code');
      else if (button.hasAttribute('aria-expanded')) button.setAttribute('aria-label', 'Show or hide');
    });
  };
  new MutationObserver(name).observe(mount, { childList: true, subtree: true });
  name();
})();
