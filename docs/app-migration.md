# Move from the standalone plugin to the Wayfinder app

The new app package includes the bar companion. Existing configurations and
credentials remain in your account. You do not need to uninstall the old plugin
or delete the old Router before installing the package.

1. Install the reviewed `wayfinder` package through Pacman.
2. Open Wayfinder and choose **Prepare / refresh setup**. This inspects existing
   state; it does not replace an existing policy with a new starter.
3. Open **Service → Install / migrate and start service**. Review the displayed
   previous service definition and the proposed default configuration and port.
   Custom service configurations or overrides need review before proceeding.
   A unique `.before-arch-UUID` backup is saved beside the previous unit.
4. Restart an already-running service from the app so it uses
   `/usr/bin/wayfinder-router`, then verify a request. Existing clients continue
   to use the same endpoint when their endpoint is `127.0.0.1:8088`.
5. Choose **Omarchy bar → Enable bar companion**. The app moves the existing
   plugin to `~/.config/omarchy/plugins/.wayfinder-before-app.XXXXXXXX/plugin`
   and shows the exact path in Diagnostics. It then registers the packaged
   companion under the same ID. No second clone or download is involved.

The old `~/.local/bin/wayfinder-router`, policy files, Secret Service items,
project profiles and installer provenance are left intact. The packaged app
always invokes `/usr/bin/wayfinder-router`; terminal commands can still resolve
the old executable until you deliberately retire it. The legacy ownership-aware
uninstaller remains in the backup for that purpose; do not blindly delete it.

The companion monitors the app's default local endpoint. It does not preserve
the old panel's remote monitoring or project/setup controls; those configuration
and management tasks belong in the app or Router CLI. Do not migrate a custom
service until you have checked the endpoint and configuration it needs.

## Recover the previous plugin

Remove the app companion through the app. Move the `plugin` directory from the
specific backup reported during migration back to
`~/.config/omarchy/plugins/io.github.asdecided.wayfinder`, then run:

```bash
omarchy-shell shell rescanPlugins
omarchy plugin enable io.github.asdecided.wayfinder
```

Do not overwrite an occupied target directory. The backup is not automatically
restored on package removal: it may contain older code or local changes that
need review. The migration never alters its contents.

To restore an earlier service, stop the running service, restore the specific
`.before-arch-UUID` unit you reviewed, run `systemctl --user daemon-reload`, and
start the service. Keep its required executable installed. Provider and policy
rollback remains a separate Router operation; moving plugin files does not
change either.
