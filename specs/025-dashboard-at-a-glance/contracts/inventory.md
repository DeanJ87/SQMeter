# Contract: dashboard inventory and its check

## `tools/dashboard/check.py`

```
python3 tools/dashboard/check.py          # check (quality gate DASH-02, build)
python3 tools/dashboard/check.py --json   # [{file, line, message}] for tools/quality
python3 tools/dashboard/check.py --list   # every path and what maps it
```

Fails, naming the item and the fix, when:

1. a property path of the status, readings, safety or alerts-armed schema, or a settings-dependency id, is mapped by no inventory source and no not-shown entry;
2. an inventory source or not-shown match matches nothing (stale);
3. a shown entry has no test: `web/tests/dashboard.spec.ts` contains no `inventory: <id>`;
4. a label key is not in `web/src/i18n/en.json`;
5. ids are not unique, or a not-shown entry has no reason.

## `readings.rain.clearInSeconds`

Integer ≥ 0. Present while the rain hold is in effect after rain has stopped; absent otherwise. Same in `/api/sensors`, `/ws/sensors`, MQTT state (as part of the readings document) and the demo.
