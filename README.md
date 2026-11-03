# SubVulpes Workshop

The Workshop holds plugins, mods and bundles for [SubVulpes](https://github.com/TheGreatVulpzin/SubVulpes) servers.
The launcher of SubVulpes will read the catalog of this repository. The launcher part is not ready yet.

## Status

The build, the index and the signature work. The Workshop has no content yet.
The plan is in the main repository: `docs/plugin-store.md`.

## Layout

| Folder | Content |
| --- | --- |
| `official/` | Work of the SubVulpes maintainers. |
| `community/` | Work of everyone else. A maintainer approves each pull request. |
| `tools/` | The build script, the public key of the index and a helper for the build tools. |
| `tests/` | Tests of the build script. |

Each content folder has three folders: `plugins/` (changes the server), `mods/` (changes the client) and `bundles/` (a plugin and a mod together).
Only `plugins/` is in use now. The seal (Official or Community) comes from the top folder.

## How the build works

The Action in `.github/workflows/build.yml` runs on each pull request and on each push to `main`.

1. It tests the build script and refuses a committed binary file.
2. It downloads the server and the SDK from the latest releases of SubVulpes.
3. It runs `build.ps1` in each plugin folder that has one. That script writes the library of the plugin.
4. `tools/build_index.py` checks each plugin, packs it into `packages/<id>-<version>-native.svplugin` and writes `index.json`.
   The server program checks each manifest first.
5. On a push to `main`, the Action signs `index.json` with the secret key and writes `index.json.sig`.
6. It replaces the branch `dist` with the result. A pull request publishes nothing.

The launcher reads `dist`. It checks the signature with `tools/index-public.pem` (RSA, SHA-256).

```
dist/
  index.json  index.json.sig
  packages/   icons/   thumbs/   images/
```

To run the build on your computer:

```
python -I tests/test_build_index.py
python -I tools/build_index.py --out dist --server-exe path\to\subvulpes_server.exe
```

## Add content

Read [CONTRIBUTING.md](CONTRIBUTING.md).

## License

The code in this repository uses GPL-3.0. Each item in `community/` names its own license in its manifest.
