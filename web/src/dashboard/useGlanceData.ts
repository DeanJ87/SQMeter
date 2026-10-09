import { useEffect, useState } from 'preact/hooks';
import { fetchEffectiveReport, type EffectiveReport } from '../lib/settingsDeps';

// Which settings are on but not in effect (specs/025 FR-011, research D9):
// read on load, every minute, when the tab comes back into view, and when the
// device says its settings changed (`configRevision`).

const REFRESH_MS = 60_000;

export const useEffectiveReport = (configRevision?: number): EffectiveReport | null => {
  const [report, setReport] = useState<EffectiveReport | null>(null);
  useEffect(() => {
    let alive = true;
    const load = () => fetchEffectiveReport().then((next) => alive && setReport(next));
    load();
    const timer = setInterval(load, REFRESH_MS);
    const onVisible = () => document.visibilityState === 'visible' && load();
    document.addEventListener('visibilitychange', onVisible);
    return () => {
      alive = false;
      clearInterval(timer);
      document.removeEventListener('visibilitychange', onVisible);
    };
  }, [configRevision]);
  return report;
};
