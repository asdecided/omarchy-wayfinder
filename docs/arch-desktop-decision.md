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
