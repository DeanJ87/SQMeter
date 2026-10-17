import { FunctionalComponent } from 'preact';
import { DISPLAY_BUTTON_UNSET, DISPLAY_PAGES, type DisplayConfig, type DisplayPage } from '../../types';
import type { SettingsTabProps } from './context';
import type { Hardware } from './hardware';
import type { DeviceActions } from '../../hooks/useDeviceActions';
import { Button, Note } from '../ui';
import {
  ActionButton,
  DepNote,
  DepToggle,
  Field,
  NumberInput,
  ResultNote,
  SelectInput,
  SettingsCard,
  StatusBadge,
  Toggle,
} from './controls';
import { defaultDisplayConfig } from './defaults';
import { t } from '../../i18n';

// Settings -> Device -> Display: the optional OLED (spec 031, contracts/ui.md).

const PAGE_LABEL: Record<DisplayPage, () => string> = {
  sky: () => t('settings.display.pageSky'),
  environment: () => t('settings.display.pageEnvironment'),
  cloud: () => t('settings.display.pageCloud'),
  safety: () => t('settings.display.pageSafety'),
  gps: () => t('settings.display.pageGps'),
  battery: () => t('settings.display.pageBattery'),
  network: () => t('settings.display.pageNetwork'),
};

const TIMEOUTS = [10, 30, 60, 120, 300, 600, 1800, 0];
const timeoutLabel = (seconds: number) =>
  seconds === 0
    ? t('settings.display.never')
    : seconds < 60
      ? t('settings.display.secondsShort', { value: seconds })
      : t('settings.display.minutesShort', { value: seconds / 60 });

// A page whose source the device doesn't have is skipped on the screen; unknown never counts as absent.
const pageHasNoSource = (page: DisplayPage, hw: Hardware, hasBattery: boolean) => {
  if (page === 'sky') return hw.skyLight.detected === false;
  if (page === 'environment') return hw.environment.detected === false;
  if (page === 'cloud') return hw.irSky.detected === false;
  if (page === 'gps') return !hw.gps.enabled;
  if (page === 'battery') return hw.statusLoaded && !hasBattery;
  return false;
};

// Chosen pages in their order, then the rest.
const pageRows = (pages: DisplayPage[]) => [...pages, ...DISPLAY_PAGES.filter((page) => !pages.includes(page))];

const move = (pages: DisplayPage[], index: number, by: number) => {
  const next = [...pages];
  [next[index], next[index + by]] = [next[index + by], next[index]];
  return next;
};

const badgeFor = (enabled: boolean, running?: { enabled: boolean; detected: boolean }) => {
  if (!enabled) return <StatusBadge tone="off" label={t('settings.display.off')} />;
  if (!running?.enabled) return undefined;
  return running.detected ? (
    <StatusBadge tone="ok" label={t('settings.display.detected')} />
  ) : (
    <StatusBadge tone="off" label={t('settings.display.notDetected')} />
  );
};

const PageRows: FunctionalComponent<{
  display: DisplayConfig;
  locked: boolean;
  hw: Hardware;
  hasBattery: boolean;
  setPages: (pages: DisplayPage[]) => void;
}> = ({ display, locked, hw, hasBattery, setPages }) => {
  const { pages } = display;
  return (
    <Field label={t('settings.display.pages')}>
      {pageRows(pages).map((page) => {
        const index = pages.indexOf(page);
        const chosen = index >= 0;
        const label = PAGE_LABEL[page]();
        return (
          <div key={page} class="display-page input-row">
            <Toggle
              label={label}
              checked={chosen}
              disabled={locked}
              hint={pageHasNoSource(page, hw, hasBattery) ? t('settings.display.noDataSource') : undefined}
              onChange={(on) => setPages(on ? [...pages, page] : pages.filter((p) => p !== page))}
            />
            {chosen && (
              <span class="btn-row">
                <Button
                  small
                  ariaLabel={t('settings.display.moveUp', { page: label })}
                  disabled={locked || index === 0}
                  onClick={() => setPages(move(pages, index, -1))}
                >
                  ↑
                </Button>
                <Button
                  small
                  ariaLabel={t('settings.display.moveDown', { page: label })}
                  disabled={locked || index === pages.length - 1}
                  onClick={() => setPages(move(pages, index, 1))}
                >
                  ↓
                </Button>
              </span>
            )}
          </div>
        );
      })}
    </Field>
  );
};

const DisplayCard: FunctionalComponent<SettingsTabProps & { actions: DeviceActions }> = ({
  config,
  update,
  error,
  hw,
  status,
  deps,
  fix,
  actions,
}) => {
  const display = { ...defaultDisplayConfig, ...config.display };
  const running = status?.display;
  const missing = Boolean(running?.enabled && !running.detected);
  const locked = !display.enabled || missing;
  const set = (key: keyof DisplayConfig, value: unknown) => update(['display', key], value);
  const pagesError = error('display.pages');

  return (
    <SettingsCard id="display" title={t('settings.display.title')} badge={badgeFor(display.enabled, running)}>
      <Toggle label={t('settings.display.showOnDisplay')} checked={display.enabled} onChange={(v) => set('enabled', v)} />
      {missing && display.enabled && (
        <div class="indent">
          <DepNote entry={deps.get('display.pages')} onFix={fix} />
        </div>
      )}

      <PageRows
        display={display}
        locked={locked}
        hw={hw}
        hasBattery={Boolean(status && 'battery' in status)}
        setPages={(p) => set('pages', p)}
      />
      {pagesError && <Note tone="bad">{pagesError}</Note>}

      <div class="form-grid">
        <Field label={t('settings.display.changePageEvery')} error={error('display.cycleSeconds')}>
          <NumberInput
            dataField="display.cycleSeconds"
            integer
            min={2}
            max={60}
            unit="s"
            value={display.cycleSeconds}
            disabled={locked || display.pages.length < 2}
            error={error('display.cycleSeconds')}
            onChange={(v) => set('cycleSeconds', v)}
          />
        </Field>
        <Field label={t('settings.display.brightness')} hint={t('settings.display.brightnessHint')}>
          <SelectInput
            dataField="display.brightness"
            value={display.brightness}
            disabled={locked}
            onChange={(v) => set('brightness', v)}
            options={[
              { value: 'low', label: t('settings.display.low') },
              { value: 'medium', label: t('settings.display.medium') },
              { value: 'high', label: t('settings.display.high') },
            ]}
          />
        </Field>
        <Field label={t('settings.display.turnOffAfter')} error={error('display.timeoutSeconds')}>
          <SelectInput
            dataField="display.timeoutSeconds"
            value={String(display.timeoutSeconds)}
            disabled={locked}
            onChange={(v) => set('timeoutSeconds', Number(v))}
            options={(TIMEOUTS.includes(display.timeoutSeconds) ? TIMEOUTS : [...TIMEOUTS, display.timeoutSeconds]).map((s) => ({
              value: String(s),
              label: timeoutLabel(s),
            }))}
          />
        </Field>
      </div>

      <Toggle
        label={t('settings.display.lightUpAtStartUp')}
        checked={display.wakeOnBoot}
        disabled={locked}
        onChange={(v) => set('wakeOnBoot', v)}
      />
      <DepToggle
        entry={deps.get('display.stayOnAtNight')}
        onFix={fix}
        label={t('settings.display.stayOnAtNight')}
        checked={display.stayOnAtNight}
        disabled={locked}
        onChange={(v) => set('stayOnAtNight', v)}
      />
      {display.stayOnAtNight && (
        <div class="indent">
          <Note tone="warn">{t('settings.display.usesLightAtNight')}</Note>
        </div>
      )}
      <Toggle label={t('settings.display.rotate180')} checked={display.rotate180} disabled={locked} onChange={(v) => set('rotate180', v)} />

      <div class="form-grid">
        <Field
          label={t('settings.display.wakeButtonPin')}
          hint={t('settings.display.wakeButtonPinHint')}
          error={error('display.buttonPin')}
        >
          <NumberInput
            dataField="display.buttonPin"
            integer
            min={0}
            max={39}
            value={display.buttonPin === DISPLAY_BUTTON_UNSET ? Number.NaN : display.buttonPin}
            disabled={locked}
            error={error('display.buttonPin')}
            onChange={(v) => set('buttonPin', Number.isFinite(v) ? v : DISPLAY_BUTTON_UNSET)}
          />
        </Field>
      </div>

      <div class="btn-row">
        <ActionButton disabled={locked} onClick={() => actions.act('/api/display/wake', t('settings.display.displayOn'))}>
          {t('settings.display.wake')}
        </ActionButton>
        <ResultNote result={actions.actionResult} />
      </div>
    </SettingsCard>
  );
};

export default DisplayCard;
