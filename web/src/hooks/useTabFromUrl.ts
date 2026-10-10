import { useEffect, useState, type MutableRef } from 'preact/hooks';
import { locationQuery, tabFromLocation } from '../components/settings/tabs';
import type { SettingsTabId } from '../components/settings/tabs';

// Settings follows the address: a link, Back/Forward or a pasted address can
// change the tab while the page stays open (the router re-renders it; the
// demo's hash routing fires hashchange). A tab click updates the address
// itself, so this only acts when the address names a different tab.
export const useTabFromUrl = (tab: SettingsTabId, setTab: (next: SettingsTabId) => void, pendingAnchor: MutableRef<string | undefined>) => {
  const [, setTick] = useState(0);
  useEffect(() => {
    const onChange = () => setTick((n) => n + 1);
    window.addEventListener('hashchange', onChange);
    window.addEventListener('popstate', onChange);
    return () => {
      window.removeEventListener('hashchange', onChange);
      window.removeEventListener('popstate', onChange);
    };
  }, []);
  const query = typeof window !== 'undefined' ? locationQuery(window.location) : '';
  const fromUrl = tabFromLocation(query);
  const urlTab = new URLSearchParams(query).get('tab') ? fromUrl.tab : null;
  useEffect(() => {
    if (urlTab && urlTab !== tab) {
      pendingAnchor.current = fromUrl.anchor;
      setTab(urlTab);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps -- reacting to the address, not to tab clicks
  }, [urlTab, fromUrl.anchor]);
};
