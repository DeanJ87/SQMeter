// The Updates page's last check, kept for this browser session so the
// dashboard can say "Update available" without contacting GitHub itself
// (specs/025 FR-018; the device keeps no result of its own).

const KEY = 'sqm.updates.latest';

export const saveUpdateCheck = (latestTag: string | null) => {
  try {
    if (latestTag) sessionStorage.setItem(KEY, latestTag);
    else sessionStorage.removeItem(KEY);
  } catch {
    // storage unavailable: the dashboard just won't say it
  }
};

/** The newest release the last check found this session, or null. */
export const lastUpdateCheck = (): string | null => {
  try {
    return sessionStorage.getItem(KEY);
  } catch {
    return null;
  }
};
