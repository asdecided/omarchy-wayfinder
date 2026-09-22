<h1 align="center">Wayfinder for Omarchy</h1>

<p align="center">
  <a href="https://github.com/tcballard/omarchy-badges"><img src="https://raw.githubusercontent.com/tcballard/omarchy-badges/75975e5b5bf75e7ede3764bcd2950046f7abfe2c/badges/v1/omarchy-app.svg" alt="Built for Omarchy: App" height="20"></a>
</p>

**One local endpoint for your coding agents.**

Wayfinder connects your coding agents to a shared AI router. Open the app to
connect a provider, choose a model and manage your projects. Close the window
when you're done: the background service keeps routing requests.

The optional Omarchy bar companion comes with the app. It shows whether the
local gateway is reachable and opens Wayfinder with a click.

## Install

Wayfinder is an Arch application, installed and updated with Pacman. There is
one package for the app, Router and bar companion; no separate plugin download.

**The bundled install path is being prepared for v0.6.0.** The latest published
[v0.5.0 package](https://github.com/asdecided/omarchy-wayfinder/releases/tag/app-v0.5.0)
predates the bundled companion. This branch can be built and tested using the
[Arch package guide](docs/arch-desktop.md). Official Omarchy packaging is pending.

After installation, open **Wayfinder** from your launcher. Connect OpenAI,
choose a model and verify a request. Then use **Connect an agent** for the
connection instructions. Your desktop keyring must be unlocked.

To add the bar button, choose **Omarchy bar → Enable bar companion** in the app.
Right-click the button to refresh its status; click to open Wayfinder.

## Already using the plugin?

Install the app package, then follow the [migration guide](docs/app-migration.md).
The app preserves existing configuration and credentials, and backs up the old
plugin before replacing it with the bundled companion.

## Update and remove

Update the Wayfinder package through Pacman. The companion updates with it;
re-enable it from the app to reload the bar after an update.

To remove just the bar button, choose **Omarchy bar → Remove bar companion**.
Routing continues. To remove the app, stop the service and remove the package:

```bash
systemctl --user stop wayfinder-router.service
sudo pacman -R wayfinder
```

Remove the companion from the app first if you enabled it. Configuration,
project profiles, credentials and migration backups remain in your account.

[Installation and testing](docs/arch-desktop.md) · [Migration and rollback](docs/app-migration.md) · [Report a bug](https://github.com/asdecided/omarchy-wayfinder/issues)

[Legacy plugin reference](GUIDE.md) · [Apache-2.0 licensed](LICENSE)

<!-- Keep old inbound README anchors useful during the app migration. -->
<a id="configure"></a>
<a id="controls"></a>
<a id="license"></a>
<a id="project-profiles"></a>
<a id="project-value"></a>
<a id="remove"></a>
<a id="requirements"></a>
<a id="validate"></a>
<a id="verified-coding-agents"></a>

[Looking for the former plugin controls? Open the legacy reference →](GUIDE.md)
