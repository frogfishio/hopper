#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "hopper.h"

static const hopper_field_t g_fields[] = {
    {
        .name_ascii = "raw",
        .name_len = 3,
        .offset = 0,
        .size = 4,
        .kind = HOPPER_FIELD_BYTES,
        .pad_byte = ' ',
        .pic = {0},
        .redefines_index = -1,
    },
    {
        .name_ascii = "num",
        .name_len = 3,
        .offset = 4,
        .size = 3, // 3 digits, unsigned DISPLAY
        .kind = HOPPER_FIELD_NUM_I32,
        .pad_byte = 0,
        .pic =
            {
                .digits = 3,
                .scale = 0,
                .is_signed = 0,
                .usage = HOPPER_USAGE_DISPLAY,
                .mask_ascii = NULL,
                .mask_len = 0,
            },
        .redefines_index = -1,
    },
    {
        .name_ascii = "snum",
        .name_len = 4,
        .offset = 7,
        .size = 4, // sign + 3 digits
        .kind = HOPPER_FIELD_NUM_I32,
        .pad_byte = 0,
        .pic =
            {
                .digits = 3,
                .scale = 0,
                .is_signed = 1,
                .usage = HOPPER_USAGE_DISPLAY,
                .mask_ascii = "+ZZ9",
                .mask_len = 4,
            },
        .redefines_index = -1,
    },
    {
        .name_ascii = "comp",
        .name_len = 4,
        .offset = 11,
        .size = 2,
        .kind = HOPPER_FIELD_NUM_I32,
        .pad_byte = 0,
        .pic =
            {
                .digits = 4,
                .scale = 0,
                .is_signed = 1,
                .usage = HOPPER_USAGE_COMP,
                .mask_ascii = NULL,
                .mask_len = 0,
            },
        .redefines_index = -1,
    },
    {
        .name_ascii = "comp3",
        .name_len = 5,
        .offset = 13,
        .size = 3, // digits=5 -> 3 bytes packed
        .kind = HOPPER_FIELD_NUM_I32,
        .pad_byte = 0,
        .pic =
            {
                .digits = 5,
                .scale = 0,
                .is_signed = 1,
                .usage = HOPPER_USAGE_COMP3,
                .mask_ascii = NULL,
                .mask_len = 0,
            },
        .redefines_index = -1,
    },
};

static const hopper_layout_t g_layouts[] = {
    {
        .name_ascii = "Sample",
        .name_len = 6,
        .record_bytes = 16,
        .layout_id = 1,
        .fields = g_fields,
        .field_count = sizeof(g_fields) / sizeof(g_fields[0]),
    },
};

static const hopper_catalog_t g_catalog = {
    .abi_version = HOPPER_ABI_VERSION,
    .layouts = g_layouts,
    .layout_count = sizeof(g_layouts) / sizeof(g_layouts[0]),
};

static void test_all(void) {
  uint8_t arena[128];
  uint8_t ref_mem[256];
  size_t ctx_size = hopper_sizeof();
  uint8_t hopper_mem[ctx_size];

  hopper_config_t cfg = {
      .abi_version = HOPPER_ABI_VERSION,
      .arena_mem = arena,
      .arena_bytes = sizeof(arena),
      .ref_mem = ref_mem,
      .ref_count = 8,
      .catalog = &g_catalog,
  };

  hopper_t *h = NULL;
  assert(hopper_init(hopper_mem, &cfg, &h) == HOPPER_OK);

  hopper_result_ref_t rec = hopper_record(h, 1);
  assert(rec.ok == 1);

  hopper_ref_info_t info = {0};
  assert(hopper_ref_info(h, rec.ref, &info) == 1);
  assert(info.size == 16);
  assert(info.layout_id == 1);

  // Raw byte access bounds.
  assert(hopper_write_u8(h, rec.ref, 15, 0xAA) == HOPPER_OK);
  assert(hopper_write_u8(h, rec.ref, 16, 0xAA) == HOPPER_E_BOUNDS);
  hopper_result_u32_t rbyte = hopper_read_u8(h, rec.ref, 15);
  assert(rbyte.ok && rbyte.v == 0xAA);

  // Bytes field padding.
  const char *txt = "hi";
  hopper_bytes_t bsrc = {(const uint8_t *)txt, 2};
  assert(hopper_field_set_bytes(h, rec.ref, 0, bsrc) == HOPPER_OK);
  char bout[4] = {0};
  hopper_bytes_mut_t bdst = {(uint8_t *)bout, sizeof(bout)};
  assert(hopper_field_get_bytes(h, rec.ref, 0, bdst) == HOPPER_OK);
  assert(memcmp(bout, "hi  ", 4) == 0);

  // DISPLAY unsigned numeric.
  assert(hopper_field_set_i32(h, rec.ref, 1, 123) == HOPPER_OK);
  hopper_result_i32_t n = hopper_field_get_i32(h, rec.ref, 1);
  assert(n.ok && n.v == 123);
  // should reject negative for unsigned field
  assert(hopper_field_set_i32(h, rec.ref, 1, -1) == HOPPER_E_PIC_INVALID);

  // DISPLAY signed numeric + edit mask.
  assert(hopper_field_set_i32(h, rec.ref, 2, 45) == HOPPER_OK);
  hopper_result_i32_t sn = hopper_field_get_i32(h, rec.ref, 2);
  assert(sn.ok && sn.v == 45);
  char mask_out[4] = {0};
  hopper_bytes_mut_t mask_buf = {(uint8_t *)mask_out, sizeof(mask_out)};
  hopper_result_i32_t fmt = hopper_field_format_display(h, rec.ref, 2, mask_buf);
  assert(fmt.ok && fmt.v == 4);
  assert(memcmp(mask_out, "+ 45", 4) == 0);

  // COMP numeric.
  assert(hopper_field_set_i32(h, rec.ref, 3, 1234) == HOPPER_OK);
  hopper_result_i32_t comp_val = hopper_field_get_i32(h, rec.ref, 3);
  assert(comp_val.ok && comp_val.v == 1234);
  hopper_result_u32_t raw16 = hopper_read_u16le(h, rec.ref, 11);
  assert(raw16.ok && raw16.v == 1234);

  // COMP-3 numeric with negative sign.
  assert(hopper_field_set_i32(h, rec.ref, 4, -12345) == HOPPER_OK);
  hopper_result_i32_t comp3_val = hopper_field_get_i32(h, rec.ref, 4);
  assert(comp3_val.ok && comp3_val.v == -12345);
  // sign nibble should be D.
  uint8_t raw_comp3[3] = {0};
  for (int i = 0; i < 3; i++) {
    hopper_result_u32_t b = hopper_read_u8(h, rec.ref, 13u + (uint32_t)i);
    assert(b.ok);
    raw_comp3[i] = (uint8_t)b.v;
  }
  assert((raw_comp3[2] & 0x0F) == 0xD);
}

int main(void) {
  test_all();
  printf("All Hopper tests passed.\n");
  return 0;
}
