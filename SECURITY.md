# Security Policy

## Supported versions

SQMeter is currently in alpha. Only the latest release is supported.

| Version | Supported |
|---------|-----------|
| latest  | ✓ |
| older   | ✗ |

## Scope

SQMeter is a local-network device. It is designed to run on your home or observatory network and is not intended to be directly exposed to the public internet. There is currently no authentication on the web interface or REST API — treat network access as equivalent to physical access.

Known non-issues (by design):

- No authentication on the web UI or REST API
- No HTTPS (local network only)
- Firmware OTA update endpoint is unauthenticated

If you intend to expose SQMeter to untrusted networks, place it behind a reverse proxy with authentication.

## Reporting a vulnerability

Please **do not** open a public GitHub issue for security vulnerabilities.

Report vulnerabilities via Discord: **.dean0**

Include:

- A description of the vulnerability
- Steps to reproduce
- Potential impact
- Any suggested fix if you have one

You can expect an acknowledgement within 7 days and a resolution timeline within 30 days for confirmed issues.
