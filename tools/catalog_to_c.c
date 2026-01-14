// SPDX-FileCopyrightText: 2026 Frogfish
// SPDX-License-Identifier: Apache-2.0
// Author: Alexander Croft <alex@frogfish.io>
// Tiny helper: read a JSON catalog and emit a C snippet (fields/layout arrays).
// Note: This is minimal and expects a small JSON file on stdin; no full JSON parser here.
// For real use, prefer integrating with your build tooling or Python loader.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This is intentionally simple: it does not parse arbitrary JSON.
// It's a placeholder reminding integrators to emit C from their own tooling.
int main(void) {
  fprintf(stderr, "catalog_to_c is a stub; use tools/load_catalog.py or your own generator.\n");
  return 1;
}
