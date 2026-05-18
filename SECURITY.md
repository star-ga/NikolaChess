# Security Policy

## Supported Versions

| Version | Supported |
| ------- | --------- |
| 3.20.x  | ✅        |
| < 3.20  | ❌        |

## Reporting a Vulnerability

Please email **security@star.ga** with:

1. A description of the issue and its impact.
2. Steps to reproduce, or a proof-of-concept.
3. The affected version (e.g. `nikola --version`).

We acknowledge reports within 72 hours and aim to ship a fix within 30
days for high-severity issues. Coordinated disclosure timelines are
discussed case-by-case; please do not file public issues for sensitive
findings.

## Disclosed Issues

### 2026-05-18 — Hardcoded auth key in legacy C-stub header

`runtime-build/nikola_auth.h` (deleted in this commit) shipped a
SipHash-2-4 key encoded as `_nikola_key_enc[32] XOR 0x1F` together with
the full client-side response routine. Anyone with read access to the
public repository could derive the key and forge a valid response for
any challenge issued by the runtime.

* **Affected:** all public clones prior to this commit. The legacy
  C-stub flow is *not* on the build path for NikolaChess `3.20.0+`
  (the runtime moved to 100% MIND in `runtime-build/runtime_*.mind`),
  so binaries shipped from the 3.20.x line are unaffected.
* **Mitigation:** the embedded key has been retired. The new auth
  flow uses MIND-language primitives in the private runtime sources
  and exposes no key material in any public artefact.
* **Action for downstream packagers:** rebuild against
  `nikolachess-main >= 3.20.0` with the current `libmind_*.so/.dylib`
  set from `nikolachess-source`. No public-header changes are required
  for callers — the legacy `nikola_auth_runtime()` entry point is
  removed.

Older binaries that linked against the leaked header should be treated
as not enforcing runtime auth and rebuilt before redistribution.
