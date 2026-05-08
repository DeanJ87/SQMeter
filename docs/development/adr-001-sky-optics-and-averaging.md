# ADR-001: Sky Optics, Averaging, and Calibration

## Status

Accepted.

## Context

The TSL2591 is a broad optical sensor. Used bare under a dark sky, it can collect light from zenith, horizon glow, nearby buildings, car parks, ground reflection, and internal enclosure reflections. At dark-sky levels the signal is also very small: an SQM value around 21.8 mag/arcsec² is only a few visible counts at MAX gain and 600 ms integration.

Single-sample SQM conversion is therefore unstable and too sensitive to stray light.

## Decision

SQMeter uses a 20 degree LED collimator lens with an extended matte-black baffle as the reference optical design.

The baffle must extend beyond the lens rim. The lens defines the main collection angle; the baffle blocks low-angle stray light before it reaches the lens.

Recommended first build:

| Property | Recommendation |
| --- | --- |
| Lens | 20 degree PMMA/acrylic LED collimator |
| Lens diameter | 20 mm |
| Lens depth | 11 mm, or per selected lens |
| Lens position | Recessed in the enclosure, not exposed proud of the case |
| Baffle extension beyond lens rim | 30-60 mm |
| Internal finish | Matte black; no shiny PCB, screw, or solder-mask surfaces visible |
| Case material | Opaque black material such as black PETG-CF |

For a purely geometric baffle cutoff:

```text
half_angle = atan((aperture_diameter / 2) / baffle_extension)
full_angle = 2 * half_angle
```

Rearranged:

```text
baffle_extension = (aperture_diameter / 2) / tan(full_angle / 2)
```

For a 20 mm aperture:

```text
20 degree full angle -> 56.7 mm extension
30 mm extension -> about 36.9 degree geometric shield
40 mm extension -> about 28.1 degree geometric shield
```

The 20 degree lens remains the primary optical limiter. A 30-40 mm baffle is still useful because it rejects grazing light and reflections before they reach the lens.

## Alternatives Considered

Bare TSL2591 was rejected because it measures too wide a field and is easily contaminated by local stray light.

5-8 degree lenses were rejected for the reference design because they are more pointing-sensitive, more affected by star-field and Milky Way structure, and less comparable to SQM-L-style zenith readings.

Lens-free baffles remain valid for experimentation, but the documented reference build is the 20 degree lens plus baffle.

## Firmware Consequences

The firmware samples the TSL2591 repeatedly and averages raw counts before converting to lux/SQM:

```text
600 ms sample
rolling raw-count average over skyAveraging.windowSeconds
subtract dark visible offset
convert corrected visible count to lux
convert lux to SQM
apply SQM offset if enabled
```

The firmware does not use a separate confidence score. It exposes raw and processed values so integrations can decide what to use.

Daylight and twilight use auto-ranging to avoid saturation. Calibrated SQM readings are based on the night mode:

```text
TSL2591_GAIN_MAX
600 ms integration
same lens/baffle build
dark offset applied
```

## Calibration

Dark calibration is performed with the aperture covered by an opaque cap after the device has collected enough rolling samples. The current rolling visible count is saved as the dark visible offset.

Absolute SQM calibration starts with a simple offset:

```text
calibrated_sqm = raw_sqm + sqmOffset
```

Changing the lens, lens distance, baffle length, aperture, or internal finish requires a new dark calibration and a new SQM offset comparison.
