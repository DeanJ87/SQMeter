import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import type { AlertEventKey } from '../../types';
import { COMMON_VARS, EVENT_VARS } from './alertVariables';
import { defaultText, varHelp, type AlertsView } from './alertsShared';
import { Field, ResultNote } from './controls';
import { Button, Note } from '../ui';
import { t } from '../../i18n';

// The result of a test alert, under the button that sent it.
export const AlertTestNote: FunctionalComponent<{ view: AlertsView; target: string }> = ({ view, target }) => {
  const result = view.tester.testResult;
  if (result?.target !== target) return null;
  return result.type === 'pending' ? <Note>{result.text}</Note> : <ResultNote result={{ type: result.type, text: result.text }} />;
};

const insertVar = (view: AlertsView, key: AlertEventKey, name: string) => {
  const target = view.lastField.current?.key === key ? view.lastField.current : null;
  const field = target?.field ?? 'message';
  const current = view.alerts.events[key][field] ?? '';
  const start = target?.el.selectionStart ?? current.length;
  const end = target?.el.selectionEnd ?? current.length;
  const token = `{${name}}`;
  view.set(['events', key, field], current.slice(0, start) + token + current.slice(end));
  if (target) {
    requestAnimationFrame(() => {
      target.el.focus();
      target.el.setSelectionRange(start + token.length, start + token.length);
    });
  }
};

// Characters as people count them (an emoji or a CJK character is one), the
// same way the device counts them.
const charCount = (text: string) => Array.from(text).length;

// Back to the default wording: a quiet link, then a confirm step in place,
// so it can't be mistaken for Save (the viewer has no confirm() dialog).
const ResetWording: FunctionalComponent<{ onReset: () => void }> = ({ onReset }) => {
  const [asking, setAsking] = useState(false);
  if (!asking) {
    return (
      <Button variant="link" small onClick={() => setAsking(true)}>
        {t('settings.alerts.resetWording')}
      </Button>
    );
  }
  return (
    <div class="inline-confirm" role="group" aria-label={t('settings.alerts.resetWordingConfirm')}>
      <span>{t('settings.alerts.resetWordingConfirm')}</span>
      <Button
        variant="danger"
        small
        onClick={() => {
          setAsking(false);
          onReset();
        }}
      >
        {t('settings.alerts.resetWording')}
      </Button>
      <Button variant="ghost" small onClick={() => setAsking(false)}>
        {t('settings.alerts.cancel')}
      </Button>
    </div>
  );
};

// Your own title and message for one event, with {variable} chips.
export const AlertTemplateEditor: FunctionalComponent<{ view: AlertsView; eventKey: AlertEventKey }> = ({ view, eventKey: key }) => {
  const { alerts, set, updateMany } = view;
  const event = alerts.events[key];
  const title = event.title ?? '';
  const message = event.message ?? '';
  const placeholder = defaultText(key, alerts.skyNightOnly);
  const track = (field: 'title' | 'message') => (e: Event) => {
    view.lastField.current = { key, field, el: e.currentTarget as HTMLInputElement | HTMLTextAreaElement };
  };
  return (
    <div class="template-editor" data-template={key}>
      <Field label={t('settings.alerts.title')} error={charCount(title) > 80 ? t('settings.alerts.upTo80Characters') : undefined}>
        <input
          class="input"
          aria-label={t('settings.alerts.alertTitle')}
          value={title}
          placeholder={placeholder.title}
          maxLength={80}
          onFocus={track('title')}
          onKeyUp={track('title')}
          onClick={track('title')}
          onInput={(e) => set(['events', key, 'title'], (e.target as HTMLInputElement).value)}
        />
      </Field>
      <Field label={t('settings.alerts.message')} error={charCount(message) > 240 ? t('settings.alerts.upTo240Characters') : undefined}>
        <textarea
          class="input"
          aria-label={t('settings.alerts.alertMessage')}
          value={message}
          placeholder={placeholder.message}
          maxLength={240}
          onFocus={track('message')}
          onKeyUp={track('message')}
          onClick={track('message')}
          onInput={(e) => set(['events', key, 'message'], (e.target as HTMLTextAreaElement).value)}
        />
      </Field>
      <div class="var-chips" aria-label={t('settings.alerts.insertAValue')}>
        {[...(EVENT_VARS[key] ?? []), ...COMMON_VARS].map((name) => (
          <button
            key={name}
            type="button"
            class="var-chip"
            title={varHelp(key, name)}
            onMouseDown={(e) => e.preventDefault()}
            onClick={() => insertVar(view, key, name)}
          >
            {`{${name}}`}
          </button>
        ))}
      </div>
      {(event.title || event.message) && (
        <ResetWording
          onReset={() =>
            updateMany([
              [['alerts', 'events', key, 'title'], ''],
              [['alerts', 'events', key, 'message'], ''],
            ])
          }
        />
      )}
    </div>
  );
};
