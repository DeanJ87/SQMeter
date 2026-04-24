import { render } from "preact";
import App from "./App";
import "./index.css";

async function init() {
  if (import.meta.env.VITE_DEMO_MODE === "true") {
    try {
      const { worker } = await import("./mocks/browser");
      await worker.start({
        serviceWorker: {
          url: import.meta.env.BASE_URL + "mockServiceWorker.js",
        },
        onUnhandledRequest: "bypass",
      });
    } catch (e) {
      console.warn("[Demo] MSW failed to start:", e);
    }
  }

  render(<App />, document.getElementById("app")!);
}

init();
