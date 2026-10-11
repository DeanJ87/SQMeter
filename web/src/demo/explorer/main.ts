import { createApiReference } from '@scalar/api-reference';
import '@scalar/api-reference/style.css';
import './explorer.css';
import openapi from '../../../../docs/api/openapi.json';
import { API_REFERENCE_URL } from '../links';

// The demo site's API explorer (specs/028-openapi FR-010): the REST API's
// OpenAPI description in Scalar, where "Try it" is answered by the demo's
// emulated SQMeter - the firmware's own code in this browser - through the
// same mock service worker the demo app uses. Nothing leaves the browser
// (spec 016 FR-006): no telemetry, no proxy, no hosted fonts, and the demo's
// Content-Security-Policy still applies. Demo build only: never on a device
// (FR-011).

async function startEmulatedDevice() {
  const { demoDevice } = await import('../device');
  await demoDevice.start();
  const { worker } = await import('../browser');
  await worker.start({
    serviceWorker: { url: import.meta.env.BASE_URL + 'mockServiceWorker.js' },
    onUnhandledRequest: 'bypass',
    quiet: true,
  });
}

const theme = `
  .dark-mode {
    --scalar-background-1: #080d14;
    --scalar-background-2: #0d141d;
    --scalar-background-3: #111b27;
    --scalar-background-accent: rgba(85, 199, 242, 0.1);
    --scalar-color-1: #e7edf3;
    --scalar-color-2: #8b9baa;
    --scalar-color-3: #768798;
    --scalar-color-accent: #55c7f2;
    --scalar-color-green: #5ee0a0;
    --scalar-color-red: #f26b62;
    --scalar-color-yellow: #f4c15d;
    --scalar-color-blue: #55c7f2;
    --scalar-color-orange: #f4c15d;
    --scalar-color-purple: #a78bfa;
    --scalar-border-color: #1b2a38;
    --scalar-button-1: #55c7f2;
    --scalar-button-1-color: #080d14;
    --scalar-button-1-hover: #7fd6f6;
  }
  .dark-mode .t-doc__sidebar, .dark-mode .sidebar {
    --scalar-sidebar-background-1: #0d141d;
    --scalar-sidebar-color-1: #e7edf3;
    --scalar-sidebar-color-2: #8b9baa;
    --scalar-sidebar-border-color: #1b2a38;
    --scalar-sidebar-item-hover-background: #111b27;
    --scalar-sidebar-item-active-background: rgba(85, 199, 242, 0.1);
    --scalar-sidebar-color-active: #55c7f2;
    --scalar-sidebar-search-background: #080d14;
    --scalar-sidebar-search-border-color: #1b2a38;
  }
  /* WCAG 2.5.8: the sidebar's group toggles are 22 px in Scalar. */
  li[data-sidebar-id] .group\\/group-button > button[aria-expanded] {
    min-width: 24px;
    min-height: 24px;
  }
  :root {
    --scalar-font: 'Inter SQM', ui-sans-serif, system-ui, sans-serif;
    --scalar-font-code: 'JetBrains Mono SQM', ui-monospace, monospace;
    --scalar-radius: 8px;
    --scalar-radius-lg: 12px;
  }
`;

// Scalar's icon-only buttons with no accessible name (WCAG 4.1.2): its
// copy-code buttons and group toggles. Named as they appear.
function nameIconButtons(root: HTMLElement) {
  const name = () => {
    root.querySelectorAll<HTMLButtonElement>('button:not([aria-label])').forEach((button) => {
      if (button.innerText.trim()) return; // visible text only: Scalar's icons carry hidden titles
      if (button.classList.contains('scalar-code-copy')) button.setAttribute('aria-label', 'Copy code');
      else if (button.hasAttribute('aria-expanded')) button.setAttribute('aria-label', 'Show or hide');
    });
  };
  new MutationObserver(name).observe(root, { childList: true, subtree: true });
  name();
}

async function init() {
  const docsLink = document.getElementById('explorer-docs') as HTMLAnchorElement | null;
  if (docsLink) docsLink.href = API_REFERENCE_URL;
  try {
    await startEmulatedDevice();
  } catch (e) {
    const note = document.getElementById('explorer-note');
    if (note) note.textContent = 'The emulated SQMeter could not start, so "Try it" has nothing to answer it.';
    console.warn('[Explorer] emulated device failed to start:', e);
  }
  createApiReference('#explorer', {
    // Scalar's hosted AI chat and MCP install links talk to scalar.com: off.
    agent: { disabled: true },
    mcp: { disabled: true },
    content: openapi,
    servers: [{ url: window.location.origin, description: 'This demo: the emulated SQMeter in your browser' }, ...openapi.servers],
    theme: 'none',
    customCss: theme,
    forceDarkModeState: 'dark',
    hideDarkModeToggle: true,
    withDefaultFonts: false,
    telemetry: false,
    hideClientButton: true,
    showDeveloperTools: 'never',
    documentDownloadType: 'json',
    defaultHttpClient: { targetKey: 'shell', clientKey: 'curl' },
    persistAuth: false,
    metaData: { title: 'SQMeter API explorer' },
  });
  nameIconButtons(document.body);
}

void init();
