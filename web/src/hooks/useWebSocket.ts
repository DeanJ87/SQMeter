import { useEffect, useRef, useState, useCallback } from "preact/hooks";

const INITIAL_RECONNECT_DELAY_MS = 5000;
const MAX_RECONNECT_DELAY_MS = 30000;
const RECONNECT_JITTER_MS = 1000;

export const useWebSocket = <T>(url: string) => {
  const [data, setData] = useState<T | null>(null);
  const [connected, setConnected] = useState(false);
  const [lastMessageAt, setLastMessageAt] = useState<number | null>(null);
  const socketRef = useRef<WebSocket | null>(null);
  const reconnectTimeoutRef = useRef<number | null>(null);
  const retryCountRef = useRef(0);
  const shouldReconnectRef = useRef(true);

  const clearReconnectTimeout = () => {
    if (reconnectTimeoutRef.current !== null) {
      window.clearTimeout(reconnectTimeoutRef.current);
      reconnectTimeoutRef.current = null;
    }
  };

  const connect = useCallback(() => {
    clearReconnectTimeout();

    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const wsUrl = `${protocol}//${window.location.host}${url}`;

    const socket = new WebSocket(wsUrl);
    socketRef.current = socket;

    socket.onopen = () => {
      console.log(`WebSocket connected: ${url}`);
      retryCountRef.current = 0;
      setConnected(true);
    };

    socket.onmessage = (event) => {
      try {
        const parsed = JSON.parse(event.data);
        setData(parsed);
        setLastMessageAt(Date.now());
      } catch (error) {
        console.error("Failed to parse WebSocket message:", error);
      }
    };

    socket.onerror = (error) => {
      console.error("WebSocket error:", error);
      setConnected(false);
    };

    socket.onclose = () => {
      console.log(`WebSocket disconnected: ${url}`);
      if (socketRef.current !== socket) {
        return;
      }

      socketRef.current = null;
      setConnected(false);

      if (!shouldReconnectRef.current) {
        return;
      }

      const retryCount = retryCountRef.current++;
      const baseDelay = Math.min(
        MAX_RECONNECT_DELAY_MS,
        INITIAL_RECONNECT_DELAY_MS * 2 ** retryCount,
      );
      const delay = baseDelay + Math.random() * RECONNECT_JITTER_MS;
      reconnectTimeoutRef.current = window.setTimeout(connect, delay);
    };
  }, [url]);

  useEffect(() => {
    shouldReconnectRef.current = true;
    connect();

    return () => {
      shouldReconnectRef.current = false;
      clearReconnectTimeout();
      socketRef.current?.close();
      socketRef.current = null;
    };
  }, [connect]);

  return { data, connected, lastMessageAt };
};
