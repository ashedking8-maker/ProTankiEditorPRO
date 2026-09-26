# Optional secure bug-report gateway – deployment required

The desktop app never contains the recipient email, SMTP password or Resend key.
**This repository has no live report endpoint by default**. Obtain a Cloudflare
Workers account, a Resend account and a verified sender domain. Deploy
`worker.mjs` with `wrangler deploy`; configure Worker secrets using the Cloudflare
dashboard or `npx wrangler secret put <NAME>`:

- `REPORT_TO_EMAIL` – maintainer's private inbox (set to the address you supplied).
- `REPORT_FROM_EMAIL` – verified sender address for Resend.
- `RESEND_API_KEY` – server-side Resend API key, never put it into CMake/GitHub source.
- `REPORT_HASH_KEY` – cryptographically random long secret for keyed address hashing.

After deployment, set the CMake variable `PTPRO_BUG_REPORT_ENDPOINT` to your
Worker URL ending in `/api/report` in the Windows build configuration. It is a
public endpoint URL, not a secret. The app displays the report dialog even
before setup but disables Send with an explicit configuration explanation.

Durable Object groups subsequent submissions from the same installation or
public IP into a single email for 30 minutes. It records the number of duplicates and responds
`202` ('received') to each. The app does not say that every request generated
an email; its message states reports may be grouped. This per-IP approximation
can combine users on shared networks; no mechanism can fully guarantee
per-person identity without accounts. Separate server-side gateway and
standard HTTPS avoid shipping mail credentials and unattended file exfiltration.

Only explicit submit transfers subject, description, a random installation ID
and (if checked) up to two recent, sanitized .log excerpts; never .dmp, maps,
3DS or screenshots. Review privacy/data retention and provider terms before
public launch. The gateway does not promise perfect automated redaction.
