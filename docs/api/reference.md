# API Reference

Every `/api/*` route, generated from the firmware's route registry and its contract schemas, so it
matches the device. Download it as [`openapi.json`](openapi.json) for Postman, client generators or
your own tools; each release also has `sqmeter-openapi-<version>.json` on its
[release page](https://github.com/DeanJ87/SQMeter/releases).

[Try it in the API explorer :material-arrow-right:](https://demo.sqmeter.dev/explorer/){ .md-button .md-button--primary }

The explorer runs on the demo site, where the demo's emulated SQMeter answers each request in your
browser - no hardware, and nothing is sent anywhere. Each operation also has a `curl` line for your
own device. Behaviour, the password and limits are in the [REST API guide](rest.md); ASCOM Alpaca
follows the [ASCOM Alpaca standard](https://ascom-standards.org/api/) and isn't repeated here.

<div id="sqm-api-reference" data-url="../openapi.json"></div>
