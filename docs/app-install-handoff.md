# App installation handoff — 22 September 2026

## Scope and input identity

Development change based on `967659cb8b6235a53df642c38d2948e3cebbdad3`.
This document travels with the reviewed PR commit; the commit identifies all
inputs. App/package/companion version: 0.6.0 (unreleased). Router remains the
released 1.1.0 with its existing reviewed x86_64 archive SHA-256.

Current Omarchy integration source reviewed:
`947e2fc002d6831c7888b29b5761d59d29e69727` on `quattro`.
The legacy plugin compatibility manifest and tests retain their original
Omarchy source pin. Do not infer a new tested desktop range from either pin.

## Behavior

- Pacman installs the app, Router, user-service template, integration helper and
  the small companion under `/usr/share/wayfinder/omarchy`.
- The app offers explicit companion enable/remove actions. No package hook
  edits home directories, starts routing or attempts desktop IPC as root.
- The helper validates the bundle, backs up an existing plugin intact, links
  the package data and asks Omarchy to rescan and enable the same plugin ID.
- The companion has a shared, bounded localhost reachability probe and opens
  the app. It performs no installation, configuration or credential handling.
- Package updates replace the companion files. Re-enable it to reload the shell.
- Removal unlinks only this companion. Router configuration, credentials,
  profiles, the old binary and migration backups remain user-owned.

## Reproduced locally

Ubuntu 24.04 x86_64, Bash and Node. Exact runtime versions and commands are
recorded in the PR validation report.

- `bash test/companion.test.sh`: fresh registration, retry/idempotence, package
  update visibility, dirty checkout backup, unrelated symlink preservation,
  unavailable shell, validation failure, enable/disable failure, ownership-safe
  removal and policy/old-binary preservation. Shell IPC is a test double.
- `bash /path/to/reviewed-omarchy/bin/omarchy-plugin-validate companion`:
  validated the packaged source against the current CLI schema. This does not
  prove a live shell rendered it.
- Legacy manifest/compatibility, presentation-model and installer/lifecycle
  regressions continue to run against the preserved legacy sources.

## Build and live gates

The Arch CI workflow builds the package unprivileged, runs the Qt tests (including
the real released Router starter contract), checks the desktop file, installs
and reinstalls the package, verifies Pacman ownership, removes it, and verifies
configuration survives. It also checks that Pacman did not register a user
plugin. These checks must pass on the actual PR commit before release.

Native Qt compilation and an Arch package build could not be run in the local
Ubuntu environment: Qt/CMake and an Arch container runtime are unavailable;
dependency installation failed under the environment's process restrictions.
Hosted CI is the build evidence, not the shell fixture tests.

Still required on Tom's XPS before the app release and official package request:

- Record `omarchy-version`, architecture and package version.
- Open the app in the real Wayland session, unlock the keyring, connect OpenAI,
  choose a model and verify a request. Confirm closing the window leaves routing
  working, and stop/start/restart work from the app.
- Review migration of the existing user service, restart onto the packaged
  binary and verify an actual agent request through the existing endpoint.
- Enable the companion over the current plugin; confirm its backup, original
  bar placement, click-to-open and status when the gateway is stopped/restarted.
- Check another monitor and a vertical bar if used; reload the shell and verify
  the companion survives. Re-enable it and confirm no extra backup or duplicate.
- Remove the companion and confirm routing continues; recover the old plugin
  from its reported backup if testing rollback.
- Supply a real screenshot of the app and companion. `preview.png` is not being
  presented as a new on-device capture.

ARM package validation, real desktop/keyring acceptance and official Omarchy
repository acceptance are not claimed. Marketplace delisting is a separate
maintainer action; do not report it complete from a submitted request.
