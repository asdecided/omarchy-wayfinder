# From installation to your first request

Install through Omarchy's plugin manager:

```sh
omarchy plugin add https://github.com/asdecided/omarchy-wayfinder.git --enable
```

A catalogue listing alone does not establish that Plugin Workbench can install
it. The catalogue inspected on 2026-09-05 marks Wayfinder `Manual setup`, with
`installAvailable: false` and a note to follow upstream installation instructions.
Workbench therefore hides it under Installable. Use the command above; do not
disable verification or edit catalogue trust flags.

## Start the Router

Open the Wayfinder bar item and choose **Set up Wayfinder**. This downloads the
checksum-pinned Router if needed, creates an untouched local starter policy,
and installs the systemd user service. Existing binaries and custom policies
are preserved. **Router ready** means the process is running; it does not mean
a working model has been connected.

## Connect OpenAI

1. Choose **Connect a provider**, then **OpenAI**.
2. Open the API keys page and create an OpenAI Platform API key. API billing is
   separate from ChatGPT subscriptions. Paste the key into the password field.
3. Choose **Save key and find models**. The key is sent to OpenAI to obtain your
   account's model list and saved in the Linux desktop Secret Service keyring.
   This step does not change routing.
4. Search for and select the model you want to use. Discovery proves that the
   model is listed for your account, not that it supports this chat transport.
5. Choose **Activate hosted routing**. This explicitly replaces only the exact
   untouched local starter with a single-model OpenAI policy. No fallback,
   price, capability estimate, or extra routing tier is inferred.
6. Choose **Send test request**. It sends one small billable request with no
   files or project content, through the running Router. Success requires text
   and a matching successful delivery receipt. The panel records the time of
   this test; it is not a guarantee of future availability.

Guided setup currently supports OpenAI API keys. Other providers and custom
policies retain the existing Router configuration workflow. This change does
not implement ChatGPT account sign-in or import coding-agent credentials.

### Desktop keyring requirements

Guided provider setup needs Python 3 and `/usr/bin/secret-tool` from `libsecret`,
plus an unlocked implementation of the Secret Service API. If you do not
already have one, GNOME Keyring is an option on Arch/Omarchy:

```sh
sudo pacman -S --needed libsecret gnome-keyring
```

Log out and back in. Your login/session must start the keyring and unlock its
collection. If automatic unlocking is not configured, unlock it through your
desktop keyring manager before saving a key or restarting Wayfinder. The
plugin does not change PAM/login configuration or silently fall back to a file.
See [GNOME libsecret](https://gitlab.gnome.org/GNOME/libsecret) for the Secret
Service client contract.

Provider keys never appear in command arguments, TOML, plugin settings, setup
state, receipts, or rendered child errors. The Router resolves a keyring command
reference at startup using its existing bounded credential resolver. If the
keyring is locked at login, unlock it and choose **Maintenance → Repair service**,
then repeat the request test.

## Connect your coding agent

Open **Coding agents** and choose Codex, Claude Code, OpenCode, Pi, or Aider.
The panel displays the recipe from the installed Router's `connect` command.
Copy and merge it into the configuration indicated by that recipe. Existing
agent files and keys are never edited by the plugin.

Ask the agent to reply with `connected`, then check the new successful receipt
in Wayfinder. Test again after a shell reload and after reboot. A direct provider
reply without a Router receipt does not establish that the agent is connected.

To reverse an agent connection:

| Agent | Undo |
| --- | --- |
| Codex | Remove the `wayfinder` provider and its model selection from the TOML you edited. |
| Claude Code | Unset `ANTHROPIC_BASE_URL`, `ANTHROPIC_AUTH_TOKEN`, `ANTHROPIC_MODEL`, and `CLAUDE_CODE_ENABLE_GATEWAY_MODEL_DISCOVERY`. |
| OpenCode | Remove `provider.wayfinder` and any `wayfinder/auto` model selection from the JSON you edited. |
| Pi | Remove `providers.wayfinder` and the saved `wayfinder/auto` selection. |
| Aider | Unset `OPENAI_API_BASE` and the loopback `OPENAI_API_KEY` placeholder; stop selecting `openai/auto`. |

## Retry, repair, and replace a key

- **Key or billing error:** check the API account, permissions and billing.
  Disconnect the guided OpenAI setup before replacing its key.
- **Model error:** refresh discovery. To choose a different active model,
  disconnect and repeat setup. No model is silently substituted.
- **Interrupted save:** re-enter the key and retry; the same owned item identity
  is reused. Disconnect can clean up a partially saved key.
- **Interrupted activation:** use **Repair service**. If the policy had already
  been promoted, repair starts it and returns to the test step. If not, setup
  returns to model selection. Disconnect restores the starter instead.
- **Cancellation:** stops the active helper; recheck state, then repair or
  disconnect. A committed policy change is not silently undone on cancellation.
- **Custom policy detected:** the assistant stops before editing it. Your custom
  configuration requires the existing manual workflow; a starter-policy reset
  is not a general-purpose migration tool.

Setup mutations are serialised by a local lock. State and policy writes are
atomic and contain no key values. Disconnect stops the running Router before
deleting the key, because the process may hold a resolved credential in memory.
Failed cleanup retains its item identity so you can unlock the keyring and retry.

## Upgrade and rollback

Update the plugin through Omarchy's plugin manager. Then open **Maintenance**:

- **upgrade Router** installs this plugin's reviewed pin using the existing
  checksum and ownership checks;
- **rollback Router** restores the verified previous binary;
- **recover Router** resolves an interrupted binary transaction.

These actions require a second click. Restart the service afterwards and repeat
the request test. They do not replace an independently installed or modified
Router. Installing the plugin update does not imply that a different binary is
already running.

## Remove

To remove just the panel while keeping the Router for other clients:

```sh
omarchy plugin remove io.github.asdecided.wayfinder
```

For guided cleanup, do this before removing the panel:

1. **Maintenance → Disconnect OpenAI / replace key**, then confirm. This stops
   the Router, restores the exact original local starter, deletes only this
   assistant's keyring item, and restarts with the starter.
2. Reverse your agent settings using the table above. Roll back any owned
   project profiles that you no longer want before removing the panel.
3. **Remove service**, then confirm. This stops and uninstalls the user service.
4. **Remove owned Router and plugin**, then confirm. The existing uninstaller
   checks the binary's recorded ownership and digest before removal, and invokes
   Omarchy's plugin removal flow.

Custom policies, project files, and independently installed binaries are never
deleted by guided cleanup. The local starter and non-secret setup directory may
remain. If you customised the guided policy, restore or review those changes
before using its automated disconnect; it will not overwrite your edits.

## Release acceptance

Automated tests cover fake-keyring failures, interruption, policy protection,
bounded child processes, and a real released Router talking to a controlled
provider. They do not replace testing a live OpenAI account, an actual desktop
keyring, Omarchy rendering, session reload, reboot, and agent use on the XPS.
