# Quickstart: Share passwords with a paired device

## Automated

```bash
~/.platformio/penv/bin/pio test -e native -f test_shared_secrets -f test_config_model -f test_settings_deps
(cd web && npx vitest run src/components/__tests__/SharePasswords.test.tsx src/lib/__tests__/settingsDeps.test.ts)
python3 tools/settings-deps/check.py && python3 tools/api/routes.py && python3 tools/api/openapi.py --check
```

## On a device

1. Settings → Device → Security: with **Password-protect changes** off, **Share passwords** is
   disabled with "Set a device password".
2. Turn password protection on, set a password, turn **Share passwords** on, save.
3. `curl -i http://sqmeter.local/api/secrets` → 401.
4. `curl -i -u admin:<password> http://sqmeter.local/api/secrets` → 200 with the four values and
   `Cache-Control: no-store`; the device log shows "Shared passwords with <address>" and no value.
5. Five requests with a wrong password, then a sixth → 429 with `Retry-After`; after a minute the
   right login works again.
6. Turn **Share passwords** off, save, repeat step 4 → 403 "Password sharing is off in SQMeter".
7. Turn password protection off, sharing on, save → 403 "Set a web password in SQMeter before
   sharing passwords".
