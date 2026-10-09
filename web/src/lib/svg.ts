// SVG geometry, not text: path and point coordinates are machine-read, so they
// always use "." and never the language's number format (spec 023 FR-017).

/** A coordinate for an SVG path or points list, to one decimal. */
export const svgNumber = (value: number) => value.toFixed(1);
