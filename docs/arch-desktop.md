# Wayfinder app installation

The `wayfinder` package owns the desktop app, Router 1.1.0, launcher, service
unit template and Omarchy bar companion. The app is a Qt adapter over the Rust
Router; routing decisions and validation stay with the Router. The connection adapter
stages comment-preserving edits and stores keys through desktop Secret Service.
The package currently targets x86_64 Arch Linux. ARM packaging remains deferred.

## Release status

This branch prepares **app-v0.6.0**. It has not been published and is not yet
available from the official Omarchy package repositories or AUR. The published
app-v0.5.0 release does not include the companion described here.

Publishing an `app-v0.6.0` release after merge triggers the Arch build and checks.
Only a successful build attaches the package, source archive, checked PKGBUILD
and SHA256SUMS. Once available, download the package and SHA256SUMS from that
release into the same directory:

```bash
sha256sum --ignore-missing -c SHA256SUMS &&
  sudo pacman -U ./wayfinder-0.6.0-1-x86_64.pkg.tar.zst
```

Do not install if the checksum check fails. There is no second plugin install.

## Build this development version

On an x86_64 Arch machine, from the reviewed, committed checkout:

```bash
build_dir=$(mktemp -d)
git archive --prefix=wayfinder-source/ HEAD | gzip -n > "$build_dir/wayfinder-source.tar.gz"
cp packaging/arch/PKGBUILD "$build_dir/PKGBUILD"
source_digest=$(sha256sum "$build_dir/wayfinder-source.tar.gz" | cut -d ' ' -f 1)
sed -i "s/@SOURCE_SHA256@/$source_digest/" "$build_dir/PKGBUILD"
(cd "$build_dir" && makepkg -si)
```

Run `makepkg` as an ordinary user. Review the PKGBUILD before building it. The
archive contains committed files only. Package checks run the native Qt tests,
released Router starter contract and companion migration tests. Hosted checks
cannot establish live Omarchy rendering, keyring or provider acceptance.

## First run

Open **Wayfinder**, choose **Prepare / refresh setup**, connect OpenAI, choose
a model and activate it. **Verify a request** requires a Router delivery receipt;
a reachable gateway is not proof that a provider works. Your desktop Secret
Service keyring must be unlocked. Use **Connect an agent** to get client-specific
instructions; Wayfinder does not rewrite agent configuration for you.

The gateway runs independently under `systemd --user`. Package installation does
not enable or start it. Closing Wayfinder leaves an already-started gateway
running. Use the app's Service controls to inspect, start, stop or restart it.

## Included Omarchy bar companion

Choose **Omarchy bar → Enable bar companion**, or run `wayfinder-omarchy enable`
in your desktop session. That action validates the packaged companion and links
`~/.config/omarchy/plugins/io.github.asdecided.wayfinder` to
`/usr/share/wayfinder/omarchy`. Pacman does not modify home directories.

An existing plugin directory or symlink is moved intact into a unique hidden
backup beside it. Its Git history, dirty changes and untracked files survive.
The companion retains the same plugin ID, so Omarchy retains existing placement.
Shell enablement and reload go through Omarchy IPC, not direct shell.json edits.
The native catalog and shell support directory links; validation is performed
on the package-owned source directory, whose contents contain no symlinks.

The companion only probes the app's local gateway at `127.0.0.1:8088`, reports
reachability and opens `/usr/bin/wayfinder`. It has no provider setup, credentials,
policy editor, Router downloader or independent Git update path. Legacy widget
settings for remote endpoints and project views do not apply to this companion.

Package updates replace the shared files. Re-enable the companion from the app
after an update to reload it in the current shell. This is idempotent and keeps
its placement. No package hook tries to reach another user's desktop session.

## Removal and rollback

Choose **Remove bar companion**, or run `wayfinder-omarchy disable`, before
uninstalling the app. Only the exact package-owned link is removed. An unrelated
checkout is never deleted. This action does not stop routing.

Stop the service before `sudo pacman -R wayfinder`. Package removal preserves
configuration, profiles, credentials and migration backups. If the bar link was
left behind, `omarchy plugin remove io.github.asdecided.wayfinder` can unlink it.

See [migration and rollback](app-migration.md) for existing service units and
old user-installed Router binaries. See [development evidence](app-install-handoff.md)
for the exact checks and outstanding live acceptance.

## Official Omarchy packaging

After the source release and XPS acceptance, contribute the checked release
PKGBUILD to `omacom/omarchy-pkgs` as `wayfinder`, initially on edge. A companion
marketplace listing is not required. No official acceptance is claimed here.

See [control-centre development handoff](control-centre.md) for routing, connections, chat and live desktop checks.
