# Contributing

Content enters this repository by a pull request. The rules below are the first draft. They will change when the build and the checks exist.

## A plugin

1. Make a folder `community/plugins/<id>/`. The id has the form `author.name`, in lower case.
2. Put these files in it: `plugin.ini` (the manifest), the source of the plugin, `README.md`, and an optional `icon.png`.
3. The icon is a square PNG, 256 by 256 pixels is best. The limits are 64 to 512 pixels and 128 KiB.
4. Name a license in the manifest (`license=`). The CI refuses content without one.
5. Open a pull request. A maintainer reads it before it merges.

The manifest format is in the main repository: `docs/plugin-manifest.md`.

## Rules

- A native plugin of the community enters only after a person reads its C code.
- A pull request does not change `official/`. Only the maintainers change it.
- Do not add a binary file other than the icon and the pictures. The CI builds the package from the source.
- Mods and bundles are not accepted yet.
