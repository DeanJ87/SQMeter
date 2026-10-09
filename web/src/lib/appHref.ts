// A link to a page of this app. The device routes by path (/settings?tab=x);
// the demo routes by hash (#/settings?tab=x) so static hosting never 404s.
// A "#/..." href on the device only changes the hash and goes nowhere.
const isDemo = import.meta.env.VITE_DEMO_MODE === 'true';

export const appHref = (path: string) => (isDemo ? `#${path}` : path);
