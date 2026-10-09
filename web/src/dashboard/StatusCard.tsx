import { FunctionalComponent } from 'preact';
import { t } from '../i18n';
import { useAnnounceChange } from '../lib/a11y';
import { Button, Card, InfoTip, Pill } from '../components/ui';
import { spoken, toCheck, type GlanceItem, type Severity } from './glance';

// The Status card (specs/025, 026 FR-001..FR-003, DS-01..DS-08): a card like
// the others whose job is the imaging app - is N.I.N.A. (or another Alpaca
// client) watching each device - and whether alerts go out. Problems no other
// card shows are one row each - name, pill, "?" - and the row goes to where
// it's fixed. Nothing another card already shows (DS-08).

type Action = 'resume' | 'acknowledge';

const TONE: Record<Severity, string> = { ok: 'pill-green', idle: 'pill-dim', note: 'pill-amber', problem: 'pill-red' };
// Tiles: the imaging app (one per Alpaca device), then whether anyone will be told.
const TILE_ORDER = ['imaging-app', 'alerts-state'];
const TILES = new Set(TILE_ORDER);

// A tile is its label, "?" and a pill (DS-08): the why is in "?", and only an
// action (Resume) gets a button. `sub` only says which one, for repeated tiles.
const Tile: FunctionalComponent<{ item: GlanceItem; onAction: (action: Action) => void }> = ({ item, onAction }) => {
  const fix = item.fix;
  return (
    <div class="metric-tile left status-tile" data-inventory={item.id} data-severity={item.severity}>
      <div class="metric-label">
        {item.label} {item.detail && <InfoTip text={item.detail} />}
      </div>
      <Pill tone={TONE[item.severity]}>{item.state}</Pill>
      {item.sub && <div class="metric-sub">{item.sub}</div>}
      {item.checked && <div class="metric-sub">{item.checked}</div>}
      {fix && 'action' in fix && (
        <Button small onClick={() => onAction(fix.action)}>
          {fix.label}
        </Button>
      )}
    </div>
  );
};

const RowBody: FunctionalComponent<{ item: GlanceItem }> = ({ item }) => (
  <>
    <span class="status-row-label">{item.label}</span>
    <Pill tone={TONE[item.severity]}>{item.state}</Pill>
  </>
);

// The whole row is the link to the fix; "?" sits beside it, not inside it.
const Row: FunctionalComponent<{ item: GlanceItem; onAction: (action: Action) => void }> = ({ item, onAction }) => {
  const fix = item.fix;
  return (
    <li class="status-row" data-inventory={item.id} data-severity={item.severity}>
      {fix && 'href' in fix ? (
        <a class="status-row-main" href={fix.href} title={fix.label}>
          <RowBody item={item} />
        </a>
      ) : (
        <span class="status-row-main">
          <RowBody item={item} />
        </span>
      )}
      {item.detail && <InfoTip text={item.detail} />}
      {fix && 'action' in fix && (
        <Button small onClick={() => onAction(fix.action)}>
          {fix.label}
        </Button>
      )}
    </li>
  );
};

const StatusCard: FunctionalComponent<{ items: GlanceItem[]; onAction: (action: Action) => void }> = ({ items, onAction }) => {
  const tiles = items.filter((entry) => TILES.has(entry.id)).sort((a, b) => TILE_ORDER.indexOf(a.id) - TILE_ORDER.indexOf(b.id));
  const rows = items.filter((entry) => !TILES.has(entry.id) && (entry.severity === 'note' || entry.severity === 'problem'));
  const open = toCheck(items);
  const worst: Severity = open.some((entry) => entry.severity === 'problem') ? 'problem' : open.length ? 'note' : 'ok';
  // Announced once when something stops working (spec 022): not every update.
  const urgent = items.filter((entry) => entry.severity === 'problem').map(spoken);
  useAnnounceChange(urgent.join(' | ') || undefined, (text) => t('glance.announce', { text }));
  const pill = open.length ? t('status.toCheck', { count: open.length }) : t('status.allGood');
  return (
    <div class="status-card" data-inventory="status-card">
      <Card title={t('status.title')} icon="eye" tone="cyan" hint={t('status.hint')} actions={<Pill tone={TONE[worst]}>{pill}</Pill>}>
        <div class="tile-grid two">
          {tiles.map((entry, index) => (
            <Tile key={`${entry.id}-${index}`} item={entry} onAction={onAction} />
          ))}
        </div>
        {rows.length > 0 && (
          <ul class="status-rows">
            {rows.map((entry, index) => (
              <Row key={`${entry.id}-${index}`} item={entry} onAction={onAction} />
            ))}
          </ul>
        )}
      </Card>
    </div>
  );
};

export default StatusCard;
