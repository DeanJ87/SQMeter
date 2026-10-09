import { describe, expect, it } from 'vitest';
import shared from '../../../../lib/AlertLogic/template-variables.json';
import { COMMON_VARS, EVENT_VARS } from '../settings/alertVariables';

// specs/008 FR-006 / SC-003: the wording editor offers exactly the variables
// the device fills - the same list as lib/AlertLogic/template-variables.json,
// which the device's native tests check from their side.
const list = shared as { common: string[]; events: Record<string, string[]>; sharesWordingWith: Record<string, string> };

describe('alert template variables', () => {
  it('offers the same common variables as the device', () => {
    expect([...COMMON_VARS].sort()).toEqual([...list.common].sort());
  });

  it('offers the same per-event variables as the device', () => {
    const worded = Object.fromEntries(Object.entries(list.events).filter(([event]) => !(event in list.sharesWordingWith)));
    expect(EVENT_VARS).toEqual(worded);
  });

  it('only shares wording with an event that has the same variables', () => {
    for (const [event, wordedBy] of Object.entries(list.sharesWordingWith)) expect(list.events[event]).toEqual(list.events[wordedBy]);
  });
});
