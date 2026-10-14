import { defineConfig } from 'vite';
import preact from '@preact/preset-vite';
import { copyFileSync, readFileSync } from 'fs';
import { resolve } from 'path';
import { latestReleasedVersion } from './src/lib/releaseVersion';

// The demo device reports the newest release in CHANGELOG.md.
const demoVersion = latestReleasedVersion(readFileSync(resolve(__dirname, '../CHANGELOG.md'), 'utf8'));

export default defineConfig({
  plugins: [
    preact(),
    // Static hosts serve 404.html for unknown paths: let the app answer
    // device URLs (/api/..., /management/..., /setup/...) opened directly.
    // Nothing may leave the browser (spec 016 FR-006): the demo only talks to
    // its own origin, enforced by the browser too.
    {
      name: 'sqm-demo-csp',
      transformIndexHtml(html: string) {
        return html.replace(
          '<head>',
          `<head>\n    <meta http-equiv="Content-Security-Policy" content="default-src 'self'; connect-src 'self' ws: wss: https://ntfy.sh https://api.pushover.net; script-src 'self' 'wasm-unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self'; worker-src 'self'">`,
        );
      },
    },
    {
      // Link previews and home-screen icon for demo.sqmeter.dev (brand/README.md).
      // Open Graph needs absolute URLs; DEMO_SITE overrides the host.
      name: 'sqm-demo-meta',
      transformIndexHtml(html: string) {
        const site = (process.env.DEMO_SITE ?? 'https://demo.sqmeter.dev').replace(/\/$/, '');
        const description = 'Try SQMeter in your browser: the real firmware logic on an emulated device, no hardware needed.';
        return html
          .replace(/<title>[^<]*<\/title>/, '<title>SQMeter Demo</title>')
          .replace(/<meta name="description" content="[^"]*" \/>/, `<meta name="description" content="${description}" />`)
          .replace(
            '</head>',
            [
              '  <link rel="apple-touch-icon" href="/apple-touch-icon.png" />',
              '    <meta property="og:type" content="website" />',
              '    <meta property="og:site_name" content="SQMeter" />',
              '    <meta property="og:title" content="SQMeter Demo" />',
              `    <meta property="og:description" content="${description}" />`,
              `    <meta property="og:url" content="${site}/" />`,
              `    <meta property="og:image" content="${site}/og-card.png" />`,
              '    <meta property="og:image:width" content="1280" />',
              '    <meta property="og:image:height" content="640" />',
              '    <meta property="og:image:alt" content="SQMeter: open-hardware sky quality meter for the ESP32" />',
              '    <meta name="twitter:card" content="summary_large_image" />',
              '  </head>',
            ].join('\n'),
          );
      },
    },
    {
      name: 'sqm-demo-404',
      closeBundle() {
        copyFileSync(resolve(__dirname, 'dist-demo/index.html'), resolve(__dirname, 'dist-demo/404.html'));
        // Demo-only brand files, kept off the device to save its flash.
        copyFileSync(resolve(__dirname, '../brand/png/apple-touch-icon-180.png'), resolve(__dirname, 'dist-demo/apple-touch-icon.png'));
        copyFileSync(resolve(__dirname, '../brand/png/sqmeter-social-1280x640.png'), resolve(__dirname, 'dist-demo/og-card.png'));
      },
    },
  ],
  // Served at the root of https://demo.sqmeter.dev. Set DEMO_BASE to build it
  // for a sub-path instead (routing is hash-based, so only assets care).
  base: process.env.DEMO_BASE ?? '/',
  define: {
    'import.meta.env.VITE_DEMO_MODE': '"true"',
    'import.meta.env.VITE_DEMO_VERSION': JSON.stringify(demoVersion),
  },
  build: {
    outDir: 'dist-demo',
    assetsDir: 'assets',
    minify: 'terser',
    terserOptions: {
      compress: { drop_console: false }, // keep logs in demo for transparency
    },
    rollupOptions: {
      // The API explorer (specs/028-openapi FR-010) is a second page of the
      // demo site only; the device UI build (vite.config.ts) never has it.
      input: {
        main: resolve(__dirname, 'index.html'),
        explorer: resolve(__dirname, 'explorer/index.html'),
      },
      output: { manualChunks: undefined },
    },
  },
});
