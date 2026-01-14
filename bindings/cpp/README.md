# SPDX-FileCopyrightText: 2026 Frogfish
# SPDX-License-Identifier: Apache-2.0
# Author: Alexander Croft <alex@frogfish.io>

# C++ usage sketch

Hopper is a C ABI. In C++ you can include the header inside an `extern "C"` block and optionally wrap it with RAII helpers.

```cpp
extern "C" {
#include <hopper.h>
}

int main() {
  // Size allocations
  size_t ctx_size = hopper_sizeof();
  size_t ref_entry_size = hopper_ref_entry_sizeof();
  // Allocate storage (example only; prefer std::vector or unique_ptr)
  std::vector<uint8_t> arena(1024);
  std::vector<uint8_t> refs(ref_entry_size * 16);
  std::vector<uint8_t> ctx(ctx_size);

  hopper_config_t cfg{};
  cfg.abi_version = HOPPER_ABI_VERSION;
  cfg.arena_mem = arena.data();
  cfg.arena_bytes = (uint32_t)arena.size();
  cfg.ref_mem = refs.data();
  cfg.ref_count = 16;
  cfg.catalog = nullptr; // or point to a real catalog

  hopper_t *h = nullptr;
  hopper_err_t err = hopper_init(ctx.data(), &cfg, &h);
  if (err != HOPPER_OK) return 1;
  return 0;
}
```

Compile with `pkg-config --cflags --libs hopper` to pull in the header and library.
