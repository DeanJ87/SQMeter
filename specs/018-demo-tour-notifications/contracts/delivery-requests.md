# Contract: `EmulatedDevice.deliveryRequests(recordId, credentialsJson)`

Input `credentialsJson` (any channel may be absent):

```json
{
  "ntfy": { "topic": "my-topic", "token": "" },
  "pushover": { "userKey": "u...", "appToken": "a..." },
  "mqtt": { "topic": "sqmeter" }
}
```

Output: requests for the record's channels (every channel for an alert; only the asked-for one for a
per-channel test) that have credentials, built by `lib/DeviceCore` `AlertDelivery` exactly as the firmware
sends them, with Wake clamped to Urgent. Unknown record: `{}`.

```json
{
  "ntfy": {
    "url": "https://ntfy.sh/my-topic",
    "contentType": "text/plain; charset=utf-8",
    "headers": [["Title", "SQMeter Demo: Rain detected"], ["Priority", "high"], ["Tags", "cloud_with_rain"]],
    "body": "..."
  },
  "pushover": {
    "url": "https://api.pushover.net/1/messages.json",
    "contentType": "application/x-www-form-urlencoded",
    "headers": [],
    "body": "token=...&user=...&title=...&message=...&priority=1"
  },
  "mqtt": { "topic": "sqmeter/alerts", "payload": "{\"event\":\"rain_started\",...}" }
}
```
