// Map POSIX timezone strings to friendly names
export const TIMEZONE_MAP: Record<string, string> = {
  UTC0: "UTC",
  "PST8PDT,M3.2.0,M11.1.0": "US/Pacific (PST/PDT)",
  "MST7MDT,M3.2.0,M11.1.0": "US/Mountain (MST/MDT)",
  "CST6CDT,M3.2.0,M11.1.0": "US/Central (CST/CDT)",
  "EST5EDT,M3.2.0,M11.1.0": "US/Eastern (EST/EDT)",
  "GMT0BST,M3.5.0/1,M10.5.0": "Europe/London (GMT/BST)",
  "CET-1CEST,M3.5.0,M10.5.0/3": "Europe/Paris (CET/CEST)",
  "AEST-10AEDT,M10.1.0,M4.1.0/3": "Australia/Sydney (AEST/AEDT)",
  "JST-9": "Asia/Tokyo (JST)",
};

export function getTimezoneFriendlyName(posix: string): string {
  return TIMEZONE_MAP[posix] || posix;
}
