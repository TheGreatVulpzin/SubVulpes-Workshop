# Contributing

Content enters this repository by a pull request. The rules below are the first draft. They will change when the build and the checks exist.

## A plugin

1. Make a folder `community/plugins/<id>/`. The id has the form `author.name`, in lower case.
2. Put these files in it: `plugin.ini` (the manifest), the source of the plugin, `build.ps1` (it builds the library that the manifest names), `README.md`, and an optional `icon.png`.
   Pictures for the plugin page go in `screenshots/`: up to 8 files, PNG or JPEG, at most 2 MiB each.
   The Action sets `SUBVULPES_SDK` to the SDK folder. `tools/vcvars.ps1` loads the 32-bit build tools.
3. The icon is a square PNG, 256 by 256 pixels is best. The limits are 64 to 512 pixels and 128 KiB.
4. Name a license in the manifest (`license=`). The CI refuses content without one.
5. Open a pull request. A maintainer reads it before it merges.

The manifest format is in the main repository: `docs/plugin-manifest.md`.

## The official plugins

The source and the tests of the official plugins live in `official/plugins/<id>/`. Code that several of them share is in `official/common/`.
The server repository (`TheGreatVulpzin/SubVulpes`) keeps the core, the public headers and the SDK.

To build and test them on your computer:

1. Download the SDK of a server release, or make one from a server build with `tools\sdk\make_sdk.ps1`. Unzip it.
2. Set `SUBVULPES_SDK` to the folder inside it, and load the 32-bit tools: `. tools/vcvars.ps1`.
3. Run `tools\run_plugin_tests.ps1`. It builds every plugin, then runs the native tests and the scenarios of the plugins named by `-Only`, or of all of them.

A change of the plugin API needs a pull request in each repository. Merge the server first and release it. The plugin pull request comes after it, because this Action builds with the newest server release.
The same rule holds for a new key of the manifest.

## Rules

- A native plugin of the community enters only after a person reads its C code.
- A pull request does not change `official/`. Only the maintainers change it.
- Do not add a binary file other than the icon and the pictures. The CI builds the package from the source.
- Mods and bundles are not accepted yet.
