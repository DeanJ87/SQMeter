# Security Policy

## Supported Versions

This is a hobbyist, open-source project maintained on a best-effort basis. Only the
latest released version of the SQM v2 firmware is actively supported. No backports
or patches are issued for older versions.

| Version | Supported |
| ------- | --------- |
| Latest  | Yes       |
| Older   | No        |

If you are running an older version, please update to the latest before reporting
an issue, as it may already be resolved.

---

## Reporting a Vulnerability

**Please do not open a public GitHub issue for security vulnerabilities.**

This project uses **GitHub's built-in Private Vulnerability Reporting** feature.
To report a vulnerability:

1. Navigate to the repository on GitHub.
2. Click the **Security** tab.
3. Click **"Report a vulnerability"**.
4. Fill in the form and submit.

Your report will be visible only to the maintainer until the issue is resolved and
a coordinated disclosure is made.

### What to Include

To help investigate and address the issue efficiently, please include as much of
the following as possible:

- **Description** — a clear explanation of the vulnerability and the affected
  component (firmware, web interface, API, etc.).
- **Steps to reproduce** — a minimal, reliable sequence of steps or a proof of
  concept that demonstrates the issue.
- **Potential impact** — what an attacker could achieve by exploiting this
  (e.g., unauthorised access, data exposure, device takeover).
- **Suggested mitigations** — if you have ideas for a fix or workaround, they
  are welcome but not required.

---

## Scope

### In Scope

The following are considered within the security scope of this project:

- **Firmware** (`src/`) — logic running on the ESP32 microcontroller.
- **Web interface** (`web/`) — the browser-based configuration and monitoring UI
  served by the device.
- **API endpoints** — HTTP endpoints exposed by the device for configuration,
  data retrieval, and control.
- **Authentication and configuration handling** — how credentials, tokens, and
  device settings are stored and validated.

### Out of Scope

The following are explicitly out of scope:

- **User's own network infrastructure** — misconfigured routers, firewalls, or
  other equipment in the reporter's local environment.
- **Third-party libraries in their unmodified form** — vulnerabilities in
  upstream Arduino, ESP-IDF, or other dependencies should be reported directly
  to those projects. If this project uses a vulnerable dependency in a way that
  creates additional exposure, that is in scope.
- **Physical device access** — attacks that require the reporter to have
  physical possession of the hardware are out of scope. Once an attacker has
  physical access to an embedded device, the threat model is fundamentally
  different.

---

## Response Expectations

This project is maintained voluntarily in spare time. Please keep that in mind
when setting expectations.

- **Acknowledgement** — you can expect an initial response within **7 days** of
  submission.
- **Coordinated disclosure** — the maintainer will work with you on a fix before
  any public disclosure. Once a fix is available and released, a disclosure
  timeline will be agreed upon. If no response is received within 30 days of
  the initial acknowledgement, you are free to disclose responsibly.
- **Credit** — if you would like to be credited in the release notes or a
  security advisory, please say so in your report.

---

## A Note on Project Nature

SQM v2 is an open-source hobbyist project, not a commercial product. It is
developed and maintained voluntarily by a single contributor. There is no
dedicated security team, no bug bounty programme, and no SLA. That said,
security matters even in hobbyist firmware — especially for devices that sit on
home networks and expose a web interface — and all credible reports will be
taken seriously and addressed as quickly as circumstances allow.

Thank you for helping keep this project and its users safe.
