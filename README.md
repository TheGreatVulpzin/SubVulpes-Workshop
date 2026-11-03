# SubVulpes Workshop

The Workshop holds plugins, mods and bundles for [SubVulpes](https://github.com/TheGreatVulpzin/SubVulpes) servers.
The SubVulpes launcher reads the catalog of this repository. It is not ready yet.

## Status

This repository is a skeleton. The build script, the index generator and the Action do not exist yet.
The plan is in the main repository: `docs/plugin-store.md`.

## Layout

| Folder | Content |
| --- | --- |
| `official/` | Work of the SubVulpes maintainers. |
| `community/` | Work of everyone else. A maintainer approves each pull request. |

Each of them has three folders: `plugins/` (changes the server), `mods/` (changes the client) and `bundles/` (a plugin and a mod together).
Only `plugins/` is in use now. The seal (Official or Community) comes from the top folder.

## Add content

Read [CONTRIBUTING.md](CONTRIBUTING.md).

## License

The code in this repository uses GPL-3.0. Each item in `community/` may name its own license in its manifest.
