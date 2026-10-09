export const DEFAULT_ORDER = ['status', 'safety', 'sky', 'sunmoon', 'cloud', 'environment', 'gps', 'light', 'device', 'ir', 'wind', 'rain'];
const ORDER_KEY = 'sqm.dashboard.order';

// Card order is a per-browser preference.
export const loadOrder = (): string[] => {
  try {
    const value = JSON.parse(localStorage.getItem(ORDER_KEY) ?? '[]');
    return Array.isArray(value) ? value.filter((id) => typeof id === 'string') : [];
  } catch {
    return [];
  }
};
export const saveOrder = (order: string[]) => {
  try {
    if (order.length) localStorage.setItem(ORDER_KEY, JSON.stringify(order));
    else localStorage.removeItem(ORDER_KEY);
  } catch {
    // storage unavailable: the order just isn't remembered
  }
};
