# SPDX-FileCopyrightText: 2026 Frogfish
# SPDX-License-Identifier: Apache-2.0
# Author: Alexander Croft <alex@frogfish.io>

# CHANGELOG

## 1.0.0
- Initial public ABI and runtime (`hopper.h`, `hopper.c`, `pic.c`) with DISPLAY/COMP/COMP-3 support.
- Configurable arena/ref table; no hidden allocations.
- Shared/static builds; pkg-config file; install target.
- Conformance tests for allocation, bounds, DISPLAY masks, COMP/COMP-3, overlays, scale, DST-too-small.
- Docs: ABI guide, conformance checklist, catalog guidance, bindings overview, getting started.
- Examples: C example, Python/Rust binding sketches, catalog loader, pkg-config/CI setup.
