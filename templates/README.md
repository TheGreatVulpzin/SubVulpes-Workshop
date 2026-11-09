# Picture templates

Copy a template, paint over it, and keep the size.

| File | Where it goes | Size | Rule |
| --- | --- | --- | --- |
| `icon-template.png` | `icon.png` in the plugin folder | 512 x 512 | Square PNG, from 64 to 512 pixels, at most 128 KiB. Keep the art inside the dashed box. |
| `screenshot-template.png` | `screenshots/` in the plugin folder | 1280 x 720 (16:9) | PNG or JPEG, at most 2 MiB, up to 8 pictures. The first one shows first. |
| `thumbnail-example.png` | Nowhere. The build makes it. | 320 x 180 | Only shows what the small view looks like. |

The launcher shows a screenshot at 640 x 360 and a small view of 112 x 68. The small view crops the sides, so keep the important part in the middle (inside the dashed box of the template).

The build makes the thumbnails from the screenshots. Do not add them to the plugin folder.
