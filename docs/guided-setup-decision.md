# Guided installation lifecycle

Status: proposed implementation, 2026-09-05.

The fresh-install acceptance run reached an online offline-mode gateway with
an untested `local` destination and no next step for OpenAI. The former success
message conflated process startup with useful provider delivery.

The Mac SetupState/SetupService design separates requirements, credentials,
configuration, service repair and result. Its keychain boundary, cancellation,
no-clobber policy creation and existing-service repair are reused as principles.
The Omarchy implementation uses Linux Secret Service instead of Apple Keychain;
it does not port the Mac UI, mobile pairing, or account-provider platform.

The new panel orchestrates the released Router's doctor, service and connect
commands. The existing Router remains the only routing/delivery authority.
Provider setup is explicitly distinct from activating hosted routing. The first
workflow handles one OpenAI model, with no inferred routing ladder. Only an exact
starter or an unchanged assistant-owned policy is eligible for replacement.

The helper keeps a credential item identity and stage journal, with no key values.
It records activation before policy promotion so interruption is recoverable.
The key lives in Secret Service and reaches the Router through its existing
bounded api_key_cmd resolver. Key deletion stops the running service first.
External customisations fail closed; they are never merged heuristically.

Readiness is a successful, dated model request plus its matching Router receipt.
Discovery alone cannot prove transport compatibility, and credentials alone
cannot prove billing or model availability. User-facing labels preserve those
distinctions. Agent connection remains a copyable, reversible recipe owned by
the installed Router, with no credential import or config-file overwrite.

The deliberate limitation is guided OpenAI setup on the standard local path.
Custom policies retain the manual configuration surface. Additional provider
adapters can follow evidence from this full first-request acceptance journey.
