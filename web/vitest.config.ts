import { readFileSync } from 'fs';
import { resolve } from 'path';
import { defineConfig } from 'vitest/config';
import { latestReleasedVersion } from './src/lib/releaseVersion';

export default defineConfig({
  // As in vite.demo.config.ts: the demo device's version comes from CHANGELOG.md.
  define: {
    'import.meta.env.VITE_DEMO_VERSION': JSON.stringify(latestReleasedVersion(readFileSync(resolve(__dirname, '../CHANGELOG.md'), 'utf8'))),
  },
  test: {
    coverage: {
      provider: 'v8',
      reporter: ['text', 'html', 'lcov'],
      reportsDirectory: './coverage',
      include: ['src/**/*.{ts,tsx}'],
      exclude: ['src/mocks/**', 'src/main.tsx', 'src/types/**'],
    },
    projects: [
      {
        extends: true,
        test: {
          name: 'logic',
          environment: 'node',
          include: ['src/**/*.test.ts'],
        },
      },
      {
        extends: true,
        test: {
          name: 'components',
          environment: 'jsdom',
          include: ['src/components/**/*.test.tsx', 'src/dashboard/**/*.test.tsx'],
          setupFiles: ['./src/test/setup.ts'],
        },
      },
    ],
  },
});
