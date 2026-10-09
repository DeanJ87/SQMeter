import { setupWorker } from 'msw/browser';
import { demoHandlers } from './handlers';
import { i18nHandlers } from './i18n';

export const worker = setupWorker(...i18nHandlers, ...demoHandlers);
