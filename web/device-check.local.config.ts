import { defineConfig } from '@playwright/test';

export default defineConfig({
  testDir: '.',
  testMatch: 'device-check.local.spec.ts',
  reporter: 'list',
  use: { colorScheme: 'dark' },
});
