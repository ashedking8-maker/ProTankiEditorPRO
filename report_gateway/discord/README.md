# Discord reporting gateway (0.5.28)

Replace the existing Cloudflare Worker `protankieditorbugreport` code via **Edit code** with `worker.mjs`, then **Deploy**. In Settings keep:

- `DISCORD_WEBHOOK_URL`: **Secret**. Rotate any webhook URL previously visible in screenshots.
- `REPORT_LIMITS`: Workers KV binding to `protanki-report-limits`.

The public endpoint is `https://protankieditorbugreport.ashedking8.workers.dev/api/report`. In the GitHub repository, set **Settings → Secrets and variables → Actions → Variables → New repository variable**: `PTPRO_BUG_REPORT_ENDPOINT` to this full HTTPS URL; do not set the Discord webhook URL in GitHub.

The native client sends version, random persistent installation ID, subject, description, and optionally excerpts of recent redacted logs. The client silently stops sending for 30 minutes after a successful 202 (even after an app restart). Cloudflare also stores *HMAC-hashed* installation and IP cooldown keys with 1800-second TTL and silently acknowledges repeated reports without sending to Discord. Users behind the same public IP may share the server limit. The limit is intentionally best-effort: **Workers KV is eventually consistent and does not provide atomic cross-location locking**; this is not a strict abuse-proof limit against coordinated simultaneous traffic. For hard rate guarantees add Durable Objects / WAF rate limiting before public deployment.

Test with the Cloudflare HTTP panel: POST `/api/report`, header `Content-Type: application/json`, body:

```json
{"version":"0.5.28","client_id":"0123456789abcdef0123456789abcdef","subject":"Discord test","description":"Checking report delivery","logs_opt_in":false,"logs":""}
```

First valid request should send one Discord message and get HTTP 202. Repeated immediate valid request should get the same HTTP 202 but must not send a new message. To test optional logs, wait 30 minutes or use a different IP/test KV namespace; do not remove production cooldown keys to force extra reports. The gateway is public and **does not authenticate official editor clients**. Never put a secret in the desktop binary.
