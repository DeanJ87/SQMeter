import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import { t } from '../i18n';
import { useAnnounceChange } from '../lib/a11y';
import { demoInfo } from '../lib/demoInfo';
import { Button } from '../components/ui';
import { healthyLine, type GlanceItem } from './glance';

// The area at the top of the dashboard (specs/025 FR-005..FR-007): one calm
// line when all is well; otherwise each problem with its consequence and a
// fix. On a phone the first three show, the rest behind "Show all".

const PHONE_LIMIT = 3;

const FixControl: FunctionalComponent<{ item: GlanceItem; onAction: (action: 'resume' | 'acknowledge') => void }> = ({
  item,
  onAction,
}) => {
  const fix = item.fix;
  if (!fix) return null;
  if ('href' in fix)
    return (
      <a class="glance-fix" href={fix.href}>
        {fix.label}
      </a>
    );
  return (
    <Button small variant="primary" onClick={() => onAction(fix.action)}>
      {fix.label}
    </Button>
  );
};

const Item: FunctionalComponent<{ item: GlanceItem; onAction: (action: 'resume' | 'acknowledge') => void }> = ({ item, onAction }) => (
  <li class={`glance-item glance-${item.severity}`} data-inventory={item.id}>
    <span class="glance-mark" aria-hidden="true">
      {item.severity === 'problem' ? '!' : 'i'}
    </span>
    <span class="glance-body">
      <strong>{item.text}</strong>
      {item.detail && <span class="glance-detail">{item.detail}</span>}
    </span>
    <FixControl item={item} onAction={onAction} />
  </li>
);

const AtAGlance: FunctionalComponent<{ items: GlanceItem[]; onAction: (action: 'resume' | 'acknowledge') => void }> = ({
  items,
  onAction,
}) => {
  const [showAll, setShowAll] = useState(false);
  const problems = items.filter((item) => item.severity !== 'ok');
  const healthy = healthyLine(items);
  const urgent = problems.filter((item) => item.severity === 'problem');
  // Announced once when something stops working (spec 022): alerts stopped,
  // the imaging app went quiet, the connection dropped - not every update.
  useAnnounceChange(urgent.map((item) => item.text).join(' | ') || undefined, (text) => t('glance.announce', { text }));
  const demo = demoInfo();
  const hidden = showAll ? 0 : Math.max(0, problems.length - PHONE_LIMIT);
  return (
    <section class="glance" aria-label={t('glance.title')}>
      <p class="glance-line" data-inventory="healthy">
        {healthy}
        {demo && (
          <a class="glance-demo" href="#demo" data-inventory="demo-marker" onClick={demo.openPanel}>
            {demo.clockShifted ? t('glance.demoTime') : t('glance.demo')}
          </a>
        )}
      </p>
      {problems.length > 0 && (
        <ul class={`glance-list${hidden ? ' is-limited' : ''}`}>
          {problems.map((item, index) => (
            <Item key={`${item.id}-${index}`} item={item} onAction={onAction} />
          ))}
        </ul>
      )}
      {problems.length > PHONE_LIMIT && (
        <button type="button" class="glance-more" aria-expanded={showAll} onClick={() => setShowAll(!showAll)}>
          {showAll ? t('glance.showFewer') : t('glance.showAll', { count: problems.length })}
        </button>
      )}
    </section>
  );
};

export default AtAGlance;
