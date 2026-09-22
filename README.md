<h1 align="center">Wayfinder for Omarchy</h1>

<p align="center">
  <a href="https://github.com/tcballard/omarchy-badges"><img src="https://raw.githubusercontent.com/tcballard/omarchy-badges/75975e5b5bf75e7ede3764bcd2950046f7abfe2c/badges/v1/omarchy-plugin.svg" alt="Built for Omarchy: Plugin" height="24"></a>
</p>

**One local endpoint for your coding agents.**

Wayfinder connects compatible coding agents to a shared local AI router. Use the Omarchy plugin to check status and manage setup, with a standalone desktop app for onboarding, models, project profiles and diagnostics.

## Everyday use

Open the Wayfinder bar item and choose **Set up Wayfinder**, then **Connect a provider**. Guided OpenAI setup saves your API key in the desktop keyring and sends a test request. Agent connection recipes help you point supported clients at the router. [Setup guide →](docs/setup.md)

## Install

Omarchy Quattro on x86_64 or ARM64 glibc Linux, curl, tar, sha256sum, flock, systemd user services and wl-copy. OpenAI setup also needs libsecret and an unlocked Secret Service keyring.

```bash
omarchy plugin add https://github.com/asdecided/omarchy-wayfinder.git --enable
```

## Update and remove

Update:

```bash
omarchy plugin update io.github.asdecided.wayfinder
```

Remove:

```bash
omarchy plugin remove io.github.asdecided.wayfinder
```

## A few useful details

First-run setup installs the checksum-pinned `router-v1.1.0` only when missing. An existing
`wayfinder-router` executable is never replaced. Custom policies are preserved.

**Project profiles** keep repository-specific routing together; project tokens pass over stdin. [Profiles and agent connections →](GUIDE.md#project-profiles)

Removing the plugin leaves the gateway running for applications that still use it. The desktop app is introduced alongside this plugin and is not yet listed in the AUR or Omarchy application catalogue. [Desktop app](docs/arch-desktop.md) · [Removal](GUIDE.md#remove)

[Compatibility](docs/compatibility.md) · [Troubleshooting](docs/troubleshooting.md) · [Native smoke checks](docs/native-smoke.md)

[Usage and development guide](GUIDE.md) · [Report a bug](https://github.com/asdecided/omarchy-wayfinder/issues)

[Apache-2.0 licensed](LICENSE).

<!-- Preserve links to sections now in the guide. -->
<a id="configure"></a>
<a id="controls"></a>
<a id="license"></a>
<a id="project-profiles"></a>
<a id="project-value"></a>
<a id="remove"></a>
<a id="requirements"></a>
<a id="validate"></a>
<a id="verified-coding-agents"></a>

[Looking for the previous detailed sections? Open the full guide →](GUIDE.md)
