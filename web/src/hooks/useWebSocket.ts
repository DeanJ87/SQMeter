import { useEffect, useRef, useState, useCallback } from 'preact/hooks';
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

export const useWebSocket = <T>(url: string) => {
  const [data, setData] = useState<T | null>(null);
  const [connected, setConnected] = useState(false);
  const [lastMessageAt, setLastMessageAt] = useState<number | null>(null);
  const connectionRef = useRef<Connection>({
    socket: null,
    reconnectTimeout: null,
    retryCount: 0,
    shouldReconnect: true,
    open: false,
    lost: false,
  });

  const connect = useCallback(() => {
    const connection = connectionRef.current;
    clearReconnectTimeout(connection);

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}${url}`;

    const socket = new WebSocket(wsUrl);
    connection.socket = socket;
    wireSocket<T>(socket, url, connection, { setData, setConnected, setLastMessageAt, reconnect: connect });
  }, [url]);

  useEffect(() => {
    const connection = connectionRef.current;
    connection.shouldReconnect = true;
    connect();

    return () => {
      connection.shouldReconnect = false;
      clearReconnectTimeout(connection);
      connection.socket?.close();
      connection.socket = null;
    };
  }, [connect]);

  return { data, connected, lastMessageAt };
};
