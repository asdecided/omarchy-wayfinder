# Wayfinder for Omarchy

Wayfinder makes the existing Rust gateway feel native in Omarchy Quattro. The
bar widget shows whether the local service is running and opens a compact panel
with recent routes, local-versus-hosted distribution, model readiness, savings,
and the OpenAI-compatible endpoint.

The plugin is a control surface. Routing remains inside the independently
supervised `wayfinder-router` process, so reloading or disabling the Omarchy
shell does not interrupt requests.

## Requirements

- Omarchy Quattro with third-party plugin support.
- An x86_64 or ARM64 glibc-based Linux system.
- `curl`, `tar`, `sha256sum`, `systemd --user`, and `wl-copy` (provided by a
  standard Omarchy installation).

The plugin never stores provider credentials. It reads only the gateway's
prompt-free local status surfaces.

## Install

```sh
omarchy plugin add https://github.com/asdecided/omarchy-wayfinder.git
cd ~/.config/omarchy/plugins/io.github.asdecided.wayfinder
./install.sh
```

When the Router is missing, the installer downloads the native
`router-v2026.8.0` archive for the current architecture and verifies its pinned
SHA-256 digest before extraction. It does not require Rust or Cargo. An existing
`wayfinder-router` executable is never replaced.

The installer rescans plugins and enables the widget. It does not start the
Router service, create a gateway configuration, or add a provider credential.

Open the Wayfinder bar item and choose **Install service**. The existing CLI
writes and starts `~/.config/systemd/user/wayfinder-router.service` on the
configured loopback address.

The Router binary is installed to `~/.local/bin` by default. Set
`WAYFINDER_BIN_DIR` before running the installer when another user-owned binary
directory is required. Router upgrades remain explicit plugin changes: the
release version and both architecture digests are reviewed before these pins
move. The installer records the exact release, target, archive digest, binary
digest, and user-owned path for a binary that it installs.

Omarchy clones third-party plugins disabled so their source can be reviewed
before enablement. The script performs the explicit enable step.

## Configure

Use **Setup → Plugins → Wayfinder** to change:

- **Gateway endpoint** — defaults to `http://127.0.0.1:8088`.
- **Refresh interval** — defaults to 15 seconds and is bounded to 5–300.
- **Router config** — optional path passed to `wayfinder-router service install`.

Only a loopback HTTP endpoint can be installed or restarted by the widget.
Remote HTTPS Wayfinder gateways are observed but never managed.

Point compatible applications at the endpoint shown in the panel:

```sh
export OPENAI_BASE_URL=http://127.0.0.1:8088/v1
```

Provider keys remain in the environment or the existing reviewed credential
boundary referenced by the Wayfinder configuration.

## Controls

- Left click: open or close the status panel.
- Right click: refresh status.
- Middle click: copy the endpoint.
- **Install / Start / Restart service**: manage the local systemd user unit.

When operator endpoints are protected by OIDC, the plugin continues to show
health but hides unavailable model, recent-route, and savings metadata. It does
not ask for or persist an operator token.

## Remove

```sh
omarchy plugin remove io.github.asdecided.wayfinder
```

Removal intentionally leaves the Wayfinder gateway running because other
applications may still use it. Remove that independently only when desired:

```sh
wayfinder-router service uninstall
```

The plugin also leaves the Router executable in place by default. To remove a
binary that this plugin installed, use:

```sh
./uninstall.sh --remove-owned-router
```

Removal succeeds only when the recorded path is inside the current user's home
directory and the executable still matches its installation digest. A missing,
modified, symlinked, or independently installed Router is never deleted.

## Validate

```sh
node scripts/validate.mjs
node test/model.test.mjs
bash test/install.test.sh
bash -n install.sh uninstall.sh
omarchy plugin validate  # when run on Omarchy Quattro
```

## License

Apache-2.0. See `LICENSE`.
