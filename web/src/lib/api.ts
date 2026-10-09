// The device's HTTP API as the web UI uses it (coding standard STRUCT-04):
// components ask here, or a hook in hooks/, instead of calling fetch.
// Each helper is a thin, behaviour-preserving wrapper: callers keep their own
// error handling, so a network failure still rejects.

/** GET (or `init`) `path` and return the raw response. */
export const request = (path: string, init?: RequestInit): Promise<Response> => fetch(path, init);

/** The parsed JSON body when the device answers 2xx, otherwise null. Rejects when unreachable. */
export const getJson = async <T>(path: string, init?: RequestInit): Promise<T | null> => {
  const response = await fetch(path, init);
  return response.ok ? ((await response.json()) as T) : null;
};

/** POST with no body. */
export const post = (path: string): Promise<Response> => fetch(path, { method: 'POST' });

/** POST `body` as JSON. */
export const postJson = (path: string, body: unknown): Promise<Response> =>
  fetch(path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });

/** A response body as JSON, or {} when it isn't JSON (error bodies, empty replies). */
export const bodyOf = (response: Response): Promise<Record<string, unknown>> => response.json().catch(() => ({}));
