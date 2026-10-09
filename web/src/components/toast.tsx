import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { t } from '../i18n';

export interface ToastOptions {
  message: string;
  tone?: 'ok' | 'warn' | 'bad';
  action?: { label: string; onClick: () => void };
  durationMs?: number; // 0 = stays until dismissed
}

interface ToastEntry extends ToastOptions {
  id: number;
}

let nextId = 1;
let toasts: ToastEntry[] = [];
const listeners = new Set<(entries: ToastEntry[]) => void>();
const publish = () => listeners.forEach((listener) => listener(toasts));

export const dismissToast = (id: number) => {
  toasts = toasts.filter((toast) => toast.id !== id);
  publish();
};

export const showToast = (options: ToastOptions): number => {
  const id = nextId++;
  toasts = [...toasts.slice(-2), { id, tone: 'ok', durationMs: 4000, ...options }];
  publish();
  const duration = options.durationMs ?? 4000;
  if (duration > 0) setTimeout(() => dismissToast(id), duration);
  return id;
};

export const Toaster: FunctionalComponent = () => {
  const [entries, setEntries] = useState<ToastEntry[]>(toasts);

  useEffect(() => {
    listeners.add(setEntries);
    return () => {
      listeners.delete(setEntries);
    };
  }, []);

  if (entries.length === 0) return null;
  return (
    <div class="toaster" role="status" aria-live="polite">
      {entries.map((toast) => (
        <div key={toast.id} class={`toast toast-${toast.tone}`}>
          <span>{toast.message}</span>
          {toast.action && (
            <button
              type="button"
              class="toast-action"
              onClick={() => {
                toast.action!.onClick();
                dismissToast(toast.id);
              }}
            >
              {toast.action.label}
            </button>
          )}
          <button type="button" class="toast-close" aria-label={t('toast.dismiss')} onClick={() => dismissToast(toast.id)}>
            ×
          </button>
        </div>
      ))}
    </div>
  );
};
