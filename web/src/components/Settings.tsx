import { FunctionalComponent } from 'preact';
import { useEffect, useMemo, useRef, useState } from 'preact/hooks';
import type { Config, SystemStatus } from '../types';
import {
  getConfigValidationErrors,
  getConfigValidationMessage,
  hasConfigValidationErrors,
  type ValidationErrors,
} from '../validation/configSchema';
import AlertsTab from './settings/AlertsTab';
import type { ConfigPath, SettingsTabProps } from './settings/context';
import DeviceTab from './settings/DeviceTab';
import { deriveHardware } from './settings/hardware';
import NetworkTab from './settings/NetworkTab';
import { fieldErrorAliases, toConfigPayload } from './settings/payload';
import SafetyTab from './settings/SafetyTab';
import SensorsTab from './settings/SensorsTab';
import { SETTINGS_TABS, locationQuery, tabForErrorPath, tabFromLocation, type SettingsTabId } from './settings/tabs';
import TimeTab from './settings/TimeTab';
import { listReasons, restartReasons } from './settings/restart';
import { showToast } from './toast';
import { Button } from './ui';
import { nextTabIndex, scrollBehavior } from '../lib/a11y';

const STATUS_REFRESH_MS = 10000;

// Sets value at path, copying every object along the way so state is never
// mutated in place.
const setPath = (target: Config, path: ConfigPath, value: unknown): Config => {
  const root: any = { ...target };
  let current = root;
  for (let i = 0; i < path.length - 1; i++) {
    current[path[i]] = { ...current[path[i]] };
    current = current[path[i]];
  }
  current[path[path.length - 1]] = value;
  return root;
};

const Settings: FunctionalComponent = () => {
  const initial = tabFromLocation(typeof window !== 'undefined' ? locationQuery(window.location) : '');
  const [tab, setTab] = useState<SettingsTabId>(initial.tab);
  const [config, setConfig] = useState<Config | null>(null);
  const [saved, setSaved] = useState<Config | null>(null);
  const [status, setStatus] = useState<SystemStatus | null>(null);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [validationErrors, setValidationErrors] = useState<ValidationErrors>({});
  const [originalWifiSsid, setOriginalWifiSsid] = useState<string | null>(null);
  const pendingAnchor = useRef<string | undefined>(initial.anchor);

  const loadStatus = () =>
    fetch('/api/status')
      .then((response) => (response.ok ? response.json() : null))
      .then((data) => data && setStatus(data))
      .catch(() => undefined);

  useEffect(() => {
    (async () => {
      try {
        const response = await fetch('/api/config');
        const data = await response.json();
        const normalized = toConfigPayload(data);
        setConfig(normalized);
        setSaved(normalized);
        setOriginalWifiSsid(data.wifi?.ssid ?? '');
      } catch {
        // Rendered below as the load-failure state.
      } finally {
        setLoading(false);
      }
    })();
    loadStatus();
    const timer = setInterval(loadStatus, STATUS_REFRESH_MS);
    return () => clearInterval(timer);
  }, []);

  // Scroll to a requested section once its tab has rendered.
  useEffect(() => {
    if (!config || !pendingAnchor.current) return;
    const anchor = pendingAnchor.current;
    pendingAnchor.current = undefined;
    setTimeout(() => document.getElementById(anchor)?.scrollIntoView({ block: 'start' }), 0);
  }, [config === null, tab]);

  const goTo = (next: SettingsTabId, anchor?: string) => {
    pendingAnchor.current = anchor;
    setTab(next);
    const url = new URL(window.location.href);
    if (url.hash.startsWith('#/')) {
      // Hash routing (demo): the query lives in the hash.
      url.hash = `#/settings?tab=${next}`;
    } else {
      url.searchParams.set('tab', next);
      url.searchParams.delete('section');
    }
    window.history.replaceState(window.history.state, '', `${url.pathname}${url.search}${url.hash}`);
    if (!anchor) window.scrollTo?.({ top: 0 });
  };

  const applyChanges = (changes: [ConfigPath, unknown][]) => {
    setConfig((current) => {
      if (!current) return current;
      const next = toConfigPayload(changes.reduce((acc, [path, value]) => setPath(acc, path, value), current));
      setValidationErrors(getConfigValidationErrors(next));
      return next;
    });
  };

  const applyStored = (changes: [ConfigPath, unknown][]) => {
    applyChanges(changes);
    setSaved((current) => (current ? toConfigPayload(changes.reduce((acc, [path, value]) => setPath(acc, path, value), current)) : current));
  };

  const dirty = useMemo(
    () => Boolean(config && saved && JSON.stringify(config) !== JSON.stringify(saved)),
    [config, saved]
  );

  const errorsByTab = useMemo(() => {
    const counts: Partial<Record<SettingsTabId, number>> = {};
    Object.keys(validationErrors).forEach((path) => {
      const owner = tabForErrorPath(path);
      counts[owner] = (counts[owner] ?? 0) + 1;
    });
    return counts;
  }, [validationErrors]);

  const focusFirstError = (errors: ValidationErrors) => {
    const firstField = Object.keys(errors)[0];
    if (!firstField) return;
    const owner = tabForErrorPath(firstField);
    if (owner !== tab) goTo(owner);
    setTimeout(() => {
      const alias = Object.entries(fieldErrorAliases).find(([, path]) => path === firstField)?.[0];
      const target = document.querySelector<HTMLElement>(`[data-field="${firstField}"]`)
        ?? (alias ? document.querySelector<HTMLElement>(`[data-field="${alias}"]`) : null);
      target?.scrollIntoView?.({ behavior: scrollBehavior(), block: 'center' });
      if (target instanceof HTMLInputElement || target instanceof HTMLSelectElement) target.focus({ preventScroll: true });
    }, 60);
  };

  const restart = async () => {
    try {
      await fetch('/api/restart', { method: 'POST' });
      showToast({ message: 'Restarting...' });
    } catch {
      showToast({ message: 'Could not reach the device', tone: 'bad' });
    }
  };

  const save = async () => {
    if (!config || !saved) return;
    const payload = toConfigPayload(config);
    const errors = getConfigValidationErrors(payload);
    setValidationErrors(errors);
    if (hasConfigValidationErrors(errors)) {
      showToast({ message: getConfigValidationMessage(errors), tone: 'bad' });
      focusFirstError(errors);
      return;
    }

    setSaving(true);
    try {
      const response = await fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      });
      const body = (response.headers.get('content-type') || '').includes('application/json') ? await response.json() : null;
      if (response.ok && body?.success !== false) {
        const reasons = restartReasons(saved, payload);
        showToast(
          reasons.length > 0
            ? { message: `Saved. Restart to apply ${listReasons(reasons)}.`, tone: 'warn', durationMs: 0, action: { label: 'Restart', onClick: restart } }
            : { message: 'Saved.' }
        );
        setSaved(payload);
        setConfig(payload);
        setValidationErrors({});
        loadStatus();
      } else {
        showToast({ message: body?.error || 'Failed to save settings', tone: 'bad' });
      }
    } catch {
      showToast({ message: 'Could not reach the device', tone: 'bad' });
    } finally {
      setSaving(false);
    }
  };

  const discard = () => {
    setConfig(saved);
    setValidationErrors({});
  };

  if (loading) {
    return (
      <div class="empty-state">
        <h2>Loading settings...</h2>
      </div>
    );
  }

  if (!config) {
    return <div class="empty-state tone-red">Failed to load configuration</div>;
  }

  const onTabKey = (event: KeyboardEvent) => {
    const index = SETTINGS_TABS.findIndex(({ id }) => id === tab);
    const next = nextTabIndex(event.key, index, SETTINGS_TABS.length);
    if (next === null) return;
    event.preventDefault();
    const nextId = SETTINGS_TABS[next].id;
    goTo(nextId);
    document.getElementById(`settings-tab-${nextId}`)?.focus();
  };

  const props: SettingsTabProps = {
    config,
    update: (path, value) => applyChanges([[path, value]]),
    updateMany: applyChanges,
    applyStored,
    error: (key) => validationErrors[fieldErrorAliases[key] ?? key],
    hw: deriveHardware(config, status),
    status,
    dirty,
    goTo,
  };

  return (
    <div class="panel-page settings-page page-enter">
      {/* ARIA tabs: arrows, Home and End move between tabs (spec 022). */}
      <div class="settings-tabs" role="tablist" aria-label="Settings sections" onKeyDown={onTabKey}>
        {SETTINGS_TABS.map(({ id, label }) => (
          <button
            key={id}
            id={`settings-tab-${id}`}
            type="button"
            role="tab"
            aria-selected={tab === id}
            aria-controls="settings-tab-panel"
            aria-describedby={errorsByTab[id] ? `settings-tab-${id}-errors` : undefined}
            tabIndex={tab === id ? 0 : -1}
            class={`settings-tab ${tab === id ? 'is-active' : ''}`}
            onClick={() => goTo(id)}
          >
            {label}
            {errorsByTab[id] ? <span class="settings-tab-error" title={`${errorsByTab[id]} to fix`} aria-hidden="true" /> : null}
          </button>
        ))}
        {/* The red dot in words, as each tab's description. */}
        {SETTINGS_TABS.map(({ id }) =>
          errorsByTab[id] ? (
            <span key={id} id={`settings-tab-${id}-errors`} hidden>
              {errorsByTab[id]} to fix
            </span>
          ) : null
        )}
      </div>

      <div class="settings-tab-panel" role="tabpanel" id="settings-tab-panel" aria-labelledby={`settings-tab-${tab}`} tabIndex={0}>
        {tab === 'device' && <DeviceTab {...props} />}
        {tab === 'network' && <NetworkTab {...props} originalWifiSsid={originalWifiSsid} />}
        {tab === 'time' && <TimeTab {...props} />}
        {tab === 'sensors' && <SensorsTab {...props} />}
        {tab === 'safety' && <SafetyTab {...props} />}
        {tab === 'alerts' && <AlertsTab {...props} />}
      </div>

      {dirty && (
        <div class="save-bar">
          <span class="save-bar-state">Unsaved changes</span>
          <Button variant="ghost" onClick={discard}>
            Discard
          </Button>
          <Button variant="primary" onClick={save} busy={saving} busyLabel="Saving...">
            Save
          </Button>
        </div>
      )}
    </div>
  );
};

export default Settings;
