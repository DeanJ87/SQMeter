import { useEffect, useState, useCallback } from "preact/hooks";

export const useWebSocket = <T>(url: string) => {
  const [data, setData] = useState<T | null>(null);
  const [connected, setConnected] = useState(false);
  const [ws, setWs] = useState<WebSocket | null>(null);

  const connect = useCallback(() => {
    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const wsUrl = `${protocol}//${window.location.host}${url}`;

    const socket = new WebSocket(wsUrl);

    socket.onopen = () => {
      console.log(`WebSocket connected: ${url}`);
      setConnected(true);
    };

    socket.onmessage = (event) => {
      try {
        const parsed = JSON.parse(event.data);
        setData(parsed);
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
      setConnected(false);
      // Reconnect after 5 seconds
      setTimeout(connect, 5000);
    };

    setWs(socket);
  }, [url]);

  useEffect(() => {
    connect();

    return () => {
      if (ws) {
        ws.close();
      }
    };
  }, [connect]);

  return { data, connected };
};
