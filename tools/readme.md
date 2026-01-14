# SPDX-FileCopyrightText: 2026 Frogfish
# SPDX-License-Identifier: Apache-2.0
# Author: Alexander Croft <alex@frogfish.io>

# Tools

- `catalog_example.json`: Illustrative JSON catalog matching `doc/catalog.md`. This is not consumed by Hopper directly; external toolchains can parse/emit this shape and populate `hopper_catalog_t` before calling `hopper_init`.

Future additions could include:
- Simple converter scripts (JSON → C structs) if needed.
