import { useEffect, useState } from 'preact/hooks';
import { announce } from '../lib/a11y';
import { t } from '../i18n';

const INITIAL_RECONNECT_DELAY_MS = 5000;
const MAX_RECONNECT_DELAY_MS = 30000;
const RECONNECT_JITTER_MS = 1000;

// Exponential backoff with jitter, capped.
const reconnectDelay = (retryCount: number) =>
  Math.min(MAX_RECONNECT_DELAY_MS, INITIAL_RECONNECT_DELAY_MS * 2 ** retryCount) + Math.random() * RECONNECT_JITTER_MS;

// What one hook instance remembers across sockets and reconnects.
type Connection = {
  socket: WebSocket | null;
  reconnectTimeout: number | null;
  retryCount: number;
  shouldReconnect: boolean;
  // Connection loss and recovery are announced once each, not per retry
  // (spec 022 FR-009). Two sockets dropping together say it once (dedupe).
  open: boolean;
  lost: boolean;
};

type Listeners<T> = {
  setData: (data: T) => void;
  setConnected: (connected: boolean) => void;
  setLastMessageAt: (at: number) => void;
  reconnect: () => void;
};

const wireSocket = <T>(socket: WebSocket, url: string, connection: Connection, listeners: Listeners<T>) => {
  socket.onopen = () => {
    console.log(`WebSocket connected: ${url}`);
    connection.retryCount = 0;
    connection.open = true;
    if (connection.lost) announce(t('webSocket.reconnectedToTheDevice'));
    connection.lost = false;
    listeners.setConnected(true);
  };

  socket.onmessage = (event) => {
    try {
      const parsed = JSON.parse(event.data);
      listeners.setData(parsed);
      listeners.setLastMessageAt(Date.now());
    } catch (error) {
      console.error('Failed to parse WebSocket message:', error);
    }
  };

  socket.onerror = (error) => {
    console.error('WebSocket error:', error);
    listeners.setConnected(false);
  };

  socket.onclose = () => {
    console.log(`WebSocket disconnected: ${url}`);
    if (connection.socket !== socket) {
      return;
    }

    connection.socket = null;
    listeners.setConnected(false);

    if (!connection.shouldReconnect) {
      return;
    }

    if (connection.open) {
      connection.open = false;
      connection.lost = true;
      announce(t('webSocket.connectionToTheDeviceLost'));
    }

    connection.reconnectTimeout = window.setTimeout(listeners.reconnect, reconnectDelay(connection.retryCount++));
  };
};

const clearReconnectTimeout = (connection: Connection) => {
  if (connection.reconnectTimeout !== null) {
    window.clearTimeout(connection.reconnectTimeout);
    connection.reconnectTimeout = null;
  }
};

// One socket per URL, shared by every component that asks for it: the
// status socket is used by the page and the alerts bell at the same time,
// and the device has few connections to spare (16, see troubleshooting).
type Subscriber = {
  setData: (data: unknown) => void;
  setConnected: (connected: boolean) => void;
  setLastMessageAt: (at: number) => void;
};

type Shared = {
  url: string;
  connection: Connection;
  subscribers: Set<Subscriber>;
  data: unknown;
  connected: boolean;
  lastMessageAt: number | null;
};

const sharedSockets = new Map<string, Shared>();

const fanOut = (entry: Shared): Listeners<unknown> => ({
  setData: (data) => {
    entry.data = data;
    entry.subscribers.forEach((s) => s.setData(data));
  },
  setConnected: (connected) => {
    entry.connected = connected;
    entry.subscribers.forEach((s) => s.setConnected(connected));
  },
  setLastMessageAt: (at) => {
    entry.lastMessageAt = at;
    entry.subscribers.forEach((s) => s.setLastMessageAt(at));
  },
  // Only the socket still registered for its URL reconnects.
  reconnect: () => {
    if (sharedSockets.get(entry.url) === entry) openSocket(entry);
  },
});

const openSocket = (entry: Shared) => {
  const connection = entry.connection;
  clearReconnectTimeout(connection);
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const socket = new WebSocket(`${protocol}//${window.location.host}${entry.url}`);
  connection.socket = socket;
  wireSocket<unknown>(socket, entry.url, connection, fanOut(entry));
};

const subscribe = (url: string, subscriber: Subscriber) => {
  let entry = sharedSockets.get(url);
  if (!entry) {
    entry = {
      url,
      connection: { socket: null, reconnectTimeout: null, retryCount: 0, shouldReconnect: true, open: false, lost: false },
      subscribers: new Set(),
      data: null,
      connected: false,
      lastMessageAt: null,
    };
    sharedSockets.set(url, entry);
    entry.subscribers.add(subscriber);
    openSocket(entry);
  } else {
    entry.subscribers.add(subscriber);
  }
  return entry;
};

const unsubscribe = (url: string, subscriber: Subscriber) => {
  const entry = sharedSockets.get(url);
  if (!entry) return;
  entry.subscribers.delete(subscriber);
  if (entry.subscribers.size > 0) return;
  sharedSockets.delete(url);
  entry.connection.shouldReconnect = false;
  clearReconnectTimeout(entry.connection);
  entry.connection.socket?.close();
  entry.connection.socket = null;
};

export const useWebSocket = <T>(url: string) => {
  const existing = sharedSockets.get(url);
  const [data, setData] = useState<T | null>((existing?.data as T | null) ?? null);
  const [connected, setConnected] = useState(existing?.connected ?? false);
  const [lastMessageAt, setLastMessageAt] = useState<number | null>(existing?.lastMessageAt ?? null);

  useEffect(() => {
    const subscriber: Subscriber = { setData: (d) => setData(d as T), setConnected, setLastMessageAt };
    const entry = subscribe(url, subscriber);
    // A component joining an open socket starts from its latest message.
    if (entry.data !== null) setData(entry.data as T);
    setConnected(entry.connected);
    setLastMessageAt(entry.lastMessageAt);
    return () => unsubscribe(url, subscriber);
  }, [url]);

  return { data, connected, lastMessageAt };
};
