# SEED-NOTES — CalendarDisplay

First architecture artifact (batch 2 of the producer backfill). See
`../../BATCH-2-FINDINGS.md` and `../../REVIEW-2.md` for the shared rationale; this
file records what is specific to this repo.

- **Producer id:** `calendar-display`  ·  **introduced:** `2024-09-15` (repo's first commit, full clone).
- **What this repo owns:** the FIRMWARE «SoftwareProduct» only. The physical device is registered in IoT Support and will be generated there as a `device:` instance — not modeled here.
- **Universal MDM edges (no boundBy):** firmware → `svc:iotsupport-api` (device API),
  → `cap:pub-sub-broker` (MQTT), → `cap:iam` (Keycloak M2M). Addresses are written
  into device NVS by IoT Support from its own config, so the firmware carries no
  `boundBy` recipe; IoT Support's planned device generator will emit the realized
  per-device `Serving` edges. Modeled as element kind **SystemSoftware** (bare-metal
  firmware) so a later `device: —Assignment→ ss:` resolves cleanly.
- **Device-specific:** → `svc:calendar-support`. The calendar host (`calendarsupport.home`) is a **compile-time Kconfig constant** (`CONFIG_CALENDAR_ENDPOINT`), not runtime device config → hardcoded base URL, **no boundBy** (batch-1 external-svc convention).
- **Validation:** `./scripts/arch-validate.py docs/architecture/architecture.yaml` → OK.
