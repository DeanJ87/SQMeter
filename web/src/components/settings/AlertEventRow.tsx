import { ComponentChildren, FunctionalComponent } from 'preact';
import type { AlertEventKey, AlertEventSetting } from '../../types';
import type { DepEntry } from '../../lib/settingsDeps';
import { LEVEL_OPTIONS, isPending, soundOptions, type AlertsView } from './alertsShared';
import { AlertTemplateEditor, AlertTestNote } from './AlertTemplateEditor';
import { ActionButton, DepNote, SelectInput } from './controls';
import { InfoTip } from '../ui';
import { t } from '../../i18n';

export interface AlertEventRowProps {
  view: AlertsView;
  eventKey: AlertEventKey;
  label: string;
  hint?: string;
  threshold?: ComponentChildren;
  editing: boolean;
  onEdit: () => void;
}

const testTitle = (event: AlertEventSetting, entry: DepEntry, label: string) => {
  if (event.level === 0) return t('settings.alerts.off');
  if (entry.state === 'inactive') return entry.text;
  return t('settings.alerts.sendASampleLabelAlert', { label });
};

const SoundSelect: FunctionalComponent<{ view: AlertsView; eventKey: AlertEventKey; label: string }> = ({ view, eventKey: key, label }) => {
  const event = view.alerts.events[key];
  if (event.level < 2) return <span />;
  return (
    <SelectInput
      value={event.sound}
      ariaLabel={`${label}: sound`}
      options={soundOptions(event.sound, t('settings.alerts.default'))}
      disabled={view.off}
      onChange={(v) => view.set(['events', key, 'sound'], v)}
    />
  );
};

// One "Notify me when" row: level, Pushover sound, Test and the wording.
export const AlertEventRow: FunctionalComponent<AlertEventRowProps> = ({
  view,
  eventKey: key,
  label,
  hint,
  threshold,
  editing,
  onEdit,
}) => {
  const { off, set } = view;
  const event = view.alerts.events[key];
  const entry = view.dep(`alerts.events.${key}.level`);
  // Can't be raised from Off while what it needs is missing (FR-005).
  const locked = event.level === 0 && entry.blockedBy !== undefined;
  const testDisabled = off || event.level === 0 || entry.state === 'inactive' || isPending(view.tester.testResult);
  return (
    <>
      <div class="event-row" data-event={key}>
        <span class="event-label">
          <span>
            {label}
            {hint && <InfoTip text={hint} />}
          </span>
          {threshold}
        </span>
        <SelectInput
          value={String(event.level)}
          ariaLabel={`${label}: level`}
          options={LEVEL_OPTIONS}
          disabled={off || locked}
          onChange={(v) => set(['events', key, 'level'], Number(v))}
        />
        {view.pushoverOn && <SoundSelect view={view} eventKey={key} label={label} />}
        <ActionButton
          onClick={() => view.tester.sendEventTest(key, event, view.channelCount)}
          disabled={testDisabled}
          title={testTitle(event, entry, label)}
        >
          {t('settings.alerts.test')}
        </ActionButton>
        <ActionButton onClick={onEdit} disabled={off} title={t('settings.alerts.writeYourOwnTitleAnd')}>
          {event.title || event.message ? t('settings.alerts.text') : t('settings.alerts.text2')}
        </ActionButton>
      </div>
      <AlertTestNote view={view} target={key} />
      {editing && <AlertTemplateEditor view={view} eventKey={key} />}
      <DepNote entry={entry} onFix={view.fix} />
    </>
  );
};
