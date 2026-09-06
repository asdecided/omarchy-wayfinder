# Arch desktop application

The `wayfinder` package owns `/usr/bin/wayfinder`, `/usr/bin/wayfinder-router`,
the desktop launcher and the system user-unit template. The Qt application is
a thin presentation adapter over the independently released Rust Router 1.1.0.
It never implements provider HTTP calls, credential storage or routing policy.

Open **Wayfinder** from the application launcher, choose **Prepare / refresh
setup**, connect OpenAI, choose a model and explicitly activate it. Verify a
request to establish delivery. Your desktop Secret Service keyring must be
unlocked. Qt and keyring dependencies are declared in the Arch package.

## Existing plugin installations

The app always calls `/usr/bin/wayfinder-router`; a user-installed executable
cannot shadow it. In **Service**, choose **Install / migrate and start service**, which starts it; restart an already-running service to load the new executable. This reuses the standard configuration and setup-owned keyring item.
Custom configurations remain protected by the Rust setup command; do not migrate
a custom service without reviewing its configuration first.

The old `~/.local/bin/wayfinder-router` is preserved. It may still be selected by
terminal commands until you remove it using the old installer's ownership checks.
The packaged app does not claim or delete that file. Removing the package does
not delete configuration or credentials: disconnect first if you want to remove
the setup-owned OpenAI key. Stop the user service before uninstalling the package.

## Distribution status

This PR creates buildable Arch packages and tests package installation and
removal in CI. It does not register an AUR package, add a signed Pacman repository,
or add Wayfinder to Omarchy's application menu. Those require a published source
release and distribution integration. Until then, the reviewed `.pkg.tar.zst`
can be installed with `sudo pacman -U PATH`; dependency handling is native Pacman.
Updates likewise use Pacman, rather than the plugin's binary downloader.

To build locally, archive the exact checkout with prefix `wayfinder-source/`,
place `wayfinder-source.tar.gz` next to PKGBUILD, replace its `@SOURCE_SHA256@`
placeholder with the archive SHA-256, then run `makepkg -si` as an ordinary user.
Remote Router archives always have mandatory pinned checksums.

## Releases

Publish a GitHub release tagged `app-v0.5.0` after this change is merged. That
release automatically builds, tests and attaches the x86_64 package, source
archive, checked PKGBUILD and SHA256SUMS. No preliminary workflow run is needed.
The app version in CMake and PKGBUILD must agree. ARM packaging is deferred until
an Arch ARM runner is verified. AUR publication remains a separate follow-up.

The user-service migration previews the previous unit and saves an exclusive
`.before-arch-UUID` copy alongside it. Existing service overrides still apply;
review them before migration. If installation fails, the backup remains available
for recovery. The app does not automatically delete credentials or old binaries.
