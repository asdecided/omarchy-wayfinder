# Arch owns the Linux application installation

Status: proposed, 2026-09-06.

Wayfinder's full setup and maintenance interface belongs in a standalone desktop
application. A Pacman package owns binaries, launcher and service template. The
Omarchy bar remains optional. The Rust gateway retains provider credentials,
configuration writes, request verification and recovery authority.

The native Qt adapter calls fixed executable paths with argument arrays. API keys
use stdin and are never placed in command arguments, settings or diagnostics.
Mutation is serialized; cancellation requests the Rust command's SIGTERM cleanup.
The standard configuration and Secret Service item survive migration. Existing
user-installed binaries are never silently replaced or deleted.

Package publication and Omarchy menu inclusion are distinct from a tested package
build. No AUR or official-repository availability is claimed before acceptance.

## 2026-09-22: one app installation, bundled companion

Decision: Pacman is the installation and update owner for Wayfinder on Omarchy.
Prepare app 0.6.0 with the optional companion in the same package. Retire the
standalone marketplace listing and withdraw its standard-installation request.
Keep the repository identity and legacy sources for existing installations and
recovery; do not rename or delete a working checkout as part of delisting.

New companion sources live in `companion/`. The old root QML, installer and
contract tests remain migration references and are not shipped in the app.
The app's user-session helper registers a package-owned directory link after
validating its source, preserving any existing installation as a hidden backup.
The shell already supports directory links in its catalog, loader and removal
command. The registration remains third-party code and is not described as a
first-party or marketplace-verified plugin.

Reviewed current Omarchy Quattro source:
`947e2fc002d6831c7888b29b5761d59d29e69727` (22 September 2026).
Relevant contracts: `bin/omarchy-plugin-catalog`, `bin/omarchy-plugin-validate`,
`bin/omarchy-plugin-remove`, `shell/services/PluginRegistry.qml` and
`shell/shell.qml` (rescan and enable IPC). Native desktop acceptance is separate.
