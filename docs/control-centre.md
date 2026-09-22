# Wayfinder control centre — development handoff

This change builds on app-installation PR #26 (98a995b99cbeb80377e092db64344c31d05d52ec).
The Mac reference is `asdecided/WayfinderRouter` at
`3fd791956504ef04ed618c54c8238047f5d810f9` (Router 1.1.0). App version remains
0.6.0, unreleased. This is a development PR, not a release or XPS acceptance claim.

## What works

- Sidebar navigation: Overview, Chat, Connections, Routing, Gateway, agents,
  projects, companion and diagnostics. Repeated launches activate the existing
  window where the desktop permits the local activation socket.
- Overview reads actual gateway health, configured models, credential availability,
  bounded recent decisions and priced savings. Missing data stays unavailable;
  reachability is not labelled provider verification. Refresh is every 15 seconds
  while the overview is visible, with a manual refresh and Ctrl+R.
- Connections retains the guided OpenAI discovery/activation/request test. The
  management form adds OpenAI-compatible and Anthropic destinations, including
  Gemini, Ollama, LM Studio and custom endpoints. Discover local model servers
  fills the form from the Router's fixed-loopback discovery command. Adding a
  connection does not silently change Automatic routing.
- API keys go over stdin to the installed connection adapter and then to Secret
  Service. No plaintext key is written to TOML or returned to the UI. Blank key
  input retains the existing reference; explicit key removal only clears the
  exact app-owned connection item. Restart is required to discard cached keys.
- Connections can enable/disable offline mode. The change requires a gateway
  restart. API endpoints with credentials, query strings or fragments are refused;
  hosted URLs require HTTPS, local URLs may use loopback HTTP.
- Routing loads the same native read-routing contract as the Mac app. Edit
  thresholds/tiers, add/remove non-base tiers, tune/reset feature weights and apply
  through native apply-routing. Classifier policies remain read-only. Draft prompt
  previews use the Rust scorer on a private temporary policy, without changing the
  saved policy or contacting providers. No Swift/C++ imitation scorer is used.
- Chat sends bounded history only to 127.0.0.1:8088, shows the reply and routing
  receipt, preserves unavailable pinned destinations, and offers stop/retry.
  Responses stream with bounded SSE parsing; incomplete replies are not added to
  history as verified responses. Each completed turn retains its routing receipt. Conversations can be
  switched during a session. Saving local history is opt-in; files are private,
  bounded and replaced atomically. Turning saving off deletes the saved file after
  confirmation. Invalid history is preserved, not silently overwritten.
- Gateway controls, copyable integration endpoints, configuration-folder access,
  agent instructions, project profiles and companion controls remain available.
- Omarchy's `omarchy-theme-color --all` supplies semantic palette values. The
  adapter checks every three seconds, handles replaced theme links, keeps the last
  good palette after invalid output, and otherwise uses the system Qt palette.
  Fonts and outer window rounding remain with the toolkit/compositor.

## Boundaries

The C++/Qt app is the presentation layer. `Command` owns bounded child processes;
`Gateway` owns bounded literal-loopback HTTP with proxies and redirects disabled.
Feature panels own their request lifetimes. Rust remains the authoritative scorer,
configuration validator and gateway. Routing writes use its native config command.

The new Python/TOMLKit connection adapter performs Linux-specific keyring and
comment-preserving connection edits because Router 1.1.0 has no native connection
CRUD command. It stages a private candidate and asks the packaged Rust `doctor`
for a passing configuration check before replacing the file. It uses revision
checks, a connection-writer lock and configuration backups. Unrelated TOML and
credentials survive. A changed credential-bearing endpoint requires a new route,
so a saved key is not silently forwarded to a different host.

Configuration backups do not back up API key values. If storing a new key succeeds
but a subsequent filesystem operation fails, the UI reports failure and the key
may already exist in Secret Service; reload before retrying. Connection removal
reports separately when credential cleanup cannot be confirmed. External editors
should not write during app mutations; the legacy Rust writer does not participate
in the connection adapter's lock.

No account tokens are read from other applications. ChatGPT subscription sign-in
is unavailable in this Linux build; the Mac integration depends on a verified,
separately installed macOS ChatGPT runtime. Apple Foundation Models are unavailable.
There is no feature-parity claim for those platform-specific integrations.

## Build, install and rollback

Use [the Arch build instructions](arch-desktop.md) from this PR's committed
checkout. The package adds `python` and `python-tomlkit`, and installs
`/usr/bin/wayfinder-connections`. Pacman still does not edit a home directory or
start the gateway. Close the app before replacing its package.

Saved chat history lives at `${XDG_DATA_HOME:-~/.local/share}/wayfinder/chat.json`.
Configuration and `.before-routing-*` / `.before-connections-*` backups live beside
`wayfinder-router.toml`. New keyring entries use `application=io.github.asdecided.wayfinder`
and `connection=<route-id>`. They are distinct from guided setup's credential items.

To roll back application code, stop the service and reinstall the previous package
with `pacman -U`. The Router version is unchanged. To roll back policy, stop the
service, restore the chosen configuration backup and restart. Restoring a config
file does not restore deleted or replaced keys. Package removal preserves all
user configuration, keyring entries and optional chat history.

## Verification

Reproduced locally on Ubuntu 24.04 x86_64 with GCC 13.3, CMake 3.28.3, Qt 6.4.2,
Python 3 and TOMLKit 0.12.4. The Qt build and offscreen desktop suite run against
these PR sources. Released Router 1.1.0 archive verified against
`cdbfc01872d967236f3ce742d546b2f3891b8ac10b05ef1a92dd80468ebaa1bf`.

- Native application and desktop tests compile.
- Desktop tests cover the released starter/routing contracts, preserving provider
  configuration, stale edits, classifier read-only state, pinned route retention,
  cancellation, secret-free argv and palette fallback.
- Connection tests cover the native validation contract, revision conflicts,
  keyring stdin, policy/comment preservation, private file permissions, insecure
  endpoints, credential endpoint changes, referenced-route deletion and symlinks.
- HTTP redirect and streamed-receipt tests pass against a local TCP fixture.
  The released Router also delivered a request through a local mock OpenAI provider,
  returning the actual debug metadata and matching request-ID header. No live
  external provider or paid request was used. Unix activation sockets are restricted
  here; the app keeps its instance lock and opens with activation unavailable.
- Offscreen captures are actual Qt widgets on Ubuntu. They are not Omarchy/XPS
  screenshots and do not replace Tom's pending README image.
- System package installation was unavailable locally; isolated extracted Ubuntu
  build dependencies were used. Arch package install/reinstall/remove runs in CI.

Commands (with local toolchain paths supplied through the environment):

```sh
cmake -S app -B build -G Ninja
cmake --build build
WAYFINDER_TEST_ROUTER=/path/to/released/wayfinder-router ctest --test-dir build --output-on-failure
WAYFINDER_TEST_ROUTER=/path/to/released/wayfinder-router python test/connections_test.py
bash test/companion.test.sh
```

Still required on the XPS: real Secret Service store/remove/unlock, live provider
requests through both the guided and managed connection flows, service restart and
offline enforcement, theme switching under Familiar (including rounded windows),
launcher/reactivation, fractional scaling and keyboard focus, and a real screenshot.
ChatGPT/Linux account support remains separate platform work.
