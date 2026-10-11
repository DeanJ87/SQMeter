// @vitest-environment node
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import Ajv from 'ajv';
import { demoDevice } from '../device';
import { statusDocument } from '../handlers';

// The OpenAPI document's examples (specs/028-openapi FR-006) come from the
// demo's emulated device - the firmware's own code - so they're true answers.
// This checks the committed examples still match the contract schemas;
// OPENAPI_EXAMPLES=write refreshes them from the demo, then run
// `python3 tools/api/openapi.py` to rebuild docs/api/openapi.json.

const root = join(dirname(fileURLToPath(import.meta.url)), '../../../..');
const examplesDir = join(root, 'tools/api/examples');
const schemaDir = join(root, 'specs/016-demo-device-emulation/contracts/schemas');
const ajv = new Ajv({ allErrors: true, strict: false });

// example name -> contract schema it must satisfy
const EXAMPLES: Record<string, string> = {
  readings: 'readings',
  status: 'status',
  safety: 'safety',
  'alerts-armed': 'alerts-armed',
  'config-change': 'action-result',
  'alert-test': 'action-result',
};

// The demo marks simulated actions `demo: true`; a device never sends it.
const asDevice = (document: Record<string, unknown>) => {
  const { demo: _demo, ...rest } = document;
  return rest;
};

const fromDemo = (): Record<string, unknown> => ({
  readings: JSON.parse(demoDevice.readings()),
  status: statusDocument(),
  safety: JSON.parse(demoDevice.safety()),
  'alerts-armed': JSON.parse(demoDevice.armedDocument()),
  'config-change': asDevice(JSON.parse(demoDevice.applyConfig(JSON.stringify({ deviceName: 'backyard-sqm' })).body)),
  'alert-test': asDevice(JSON.parse(demoDevice.testAlert({ channel: 'all' }).body)),
});

describe('OpenAPI examples come from the demo and match the contract', () => {
  beforeAll(async () => {
    await demoDevice.start();
    await new Promise((resolve) => setTimeout(resolve, 2100)); // a couple of ticks
    if (process.env.OPENAPI_EXAMPLES === 'write') {
      mkdirSync(examplesDir, { recursive: true });
      for (const [name, document] of Object.entries(fromDemo())) {
        writeFileSync(join(examplesDir, `${name}.json`), JSON.stringify(document, null, 2) + '\n');
      }
    }
  });
  afterAll(() => demoDevice.stop());

  for (const [name, schemaName] of Object.entries(EXAMPLES)) {
    it(`${name} matches ${schemaName}`, () => {
      const file = join(examplesDir, `${name}.json`);
      expect(existsSync(file), `${file} missing - run OPENAPI_EXAMPLES=write npx vitest run openapi-examples`).toBe(true);
      const schema = JSON.parse(readFileSync(join(schemaDir, `${schemaName}.schema.json`), 'utf8'));
      const validate = ajv.compile(schema);
      const ok = validate(JSON.parse(readFileSync(file, 'utf8')));
      expect(ok, `${name}: ${ajv.errorsText(validate.errors, { separator: '\n' })}`).toBe(true);
    });
  }

  it('the demo still answers each example with the same shape', () => {
    const live = fromDemo();
    for (const [name, schemaName] of Object.entries(EXAMPLES)) {
      const schema = JSON.parse(readFileSync(join(schemaDir, `${schemaName}.schema.json`), 'utf8'));
      expect(ajv.validate(schema, live[name]), `${name} from the demo`).toBe(true);
    }
  });
});
