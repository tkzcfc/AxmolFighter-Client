# Android release keystore

Local signing material for `build_apk.ps1`. All paths are relative to the repo; nothing here should hard-code a machine path.

| File | Tracked? | Purpose |
|---|---|---|
| `axmolfighter-release.jks` | no | PKCS12 keystore under `proj.android/keystore/` |
| `keystore.properties` | no | `KEY_STORE_PASSWORD` / `KEY_ALIAS` / `KEY_PASSWORD` only |
| `README.md` | yes | this file |

`build_apk.ps1` creates the `.jks` + `keystore.properties` on first run if missing, and passes Gradle:

`KEY_STORE_FILE=../keystore/axmolfighter-release.jks`

(relative to `proj.android/app`).

**Back up the `.jks` and properties** before shipping. Losing them means updates cannot keep the same signing identity.
