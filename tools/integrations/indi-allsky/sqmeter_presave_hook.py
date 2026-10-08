#!/usr/bin/env python3
# indi-allsky image pre-save hook: turns SQMeter's numbers into words for
# the image label ({custom_1} ... {custom_5}).
#
# indi-allsky passes every user sensor slot as an environment variable
# (SENSOR_USER_10 ... SENSOR_USER_59) and reads custom_1 ... custom_9 back
# from the JSON file named in DATA_JSON. The slot numbers below match the
# suggested layout on https://sqmeter.dev/user-guide/indi-allsky/ - change
# them if you put the SQMeter topics in other slots.

import json
import os
import sys

SLOT_SKYSTATE = 15
SLOT_SAFE = 18
SLOT_IMAGING = 19
SLOT_RAINING = 24
SLOT_ONLINE = 29
SLOT_UNSAFE_FLAGS = 31

# SQMeter's safety reason bits (GET /api/safety -> reasonFlags).
REASONS = {
    0: 'manual override',
    1: 'no data',
    2: 'stale data',
    3: 'sensor fault',
    4: 'cloud',
    5: 'sky brightness',
    6: 'humidity',
    7: 'dew point',
    8: 'humidity sensor fault',
    9: 'rain',
    10: 'rain sensor fault',
    11: 'wind',
    12: 'gusts',
    13: 'wind sensor fault',
}


def slot(number):
    try:
        return float(os.environ['SENSOR_USER_{0:d}'.format(number)])
    except (KeyError, ValueError):
        return None


def unsafe_reasons(flags):
    if flags is None:
        return ''
    bits = int(flags)
    return ', '.join(text for bit, text in REASONS.items() if bits & (1 << bit))


try:
    data_file = os.environ['DATA_JSON']
except KeyError:
    sys.exit(1)

safe = slot(SLOT_SAFE)
reasons = unsafe_reasons(slot(SLOT_UNSAFE_FLAGS))
sky_code = slot(SLOT_SKYSTATE)
sky = {0: 'Clear', 1: 'Cloudy', 2: 'Overcast'}.get(int(sky_code), '') if sky_code is not None else ''

data = {
    'custom_1': 'SAFE' if safe == 1 else 'UNSAFE' + (': ' + reasons if reasons else ''),
    'custom_2': 'Imaging' if slot(SLOT_IMAGING) == 1 else 'Not imaging',
    'custom_3': 'Raining' if slot(SLOT_RAINING) == 1 else 'Dry',
    'custom_4': sky,
    # indi-allsky keeps the last values it received: say so when the device is offline.
    'custom_5': 'SQMeter offline' if slot(SLOT_ONLINE) == 0 else '',
}

with open(data_file, 'w') as f:
    json.dump(data, f)

sys.exit(0)
