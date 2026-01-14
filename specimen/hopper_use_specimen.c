

THIS FILE DOES NOT COMPILE
IT IS HERE FOR REFERENCE ONLY
TO ILLUSTRATE HOW HOPPER SUPPORT 
IS IMPLEMENTED




#include "codegen_internal.h"
#include "codegen_shared.h"
#include "emitter/emit_primitives.h"
#include "emitter/emit_result.h"
#include "emitter/emit_buffer_stream.h"
#include "emitter/emit_collections.h"
#include <stdlib.h>
#include <string.h>

void emit_hopper_record(Codegen *ctx, Expr *layout_expr) {
  ZingLayout *layout = NULL;
  ZingEnum *res_enum = NULL;
  int ok_idx = 0;
  int fail_idx = 0;
  int max_fields = 0;
  int layout_id = 0;
  long size = 0;
  size_t cursor_slot = temp_push(ctx);
  size_t ref_idx_slot = temp_push(ctx);
  size_t ref_ptr_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  char fail_label[64];
  char end_label[64];
  const char *layout_name = NULL;

  if (!layout_expr || layout_expr->kind != EXPR_SYMBOL || !layout_expr->text) {
    fprintf(stderr,
            "zingc: error[E000]: Hopper record: expects a layout symbol\n");
    exit(1);
  }
  layout_name = strip_symbol_hash(layout_expr->text);
  layout = find_layout_local(ctx, layout_name);
  if (!layout) {
    fprintf(stderr, "zingc: error[E000]: unknown layout '%s'\n",
            layout_name ? layout_name : "<layout>");
    exit(1);
  }

  if (!resolve_result_enum(ctx, &res_enum, &ok_idx, &fail_idx)) {
    fprintf(stderr, "zingc: error[E000]: Result enum not found for Hopper\n");
    exit(1);
  }
  max_fields = enum_max_fields(res_enum);
  layout_id = layout_index(layout);
  size = layout->bytes;

  snprintf(fail_label, sizeof(fail_label), "hopper_fail_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_end_%zu", id);

  emit_load_label(ctx, "hopper_cursor");
  emit_store_temp(ctx, cursor_slot);
  emit_load_temp_to_hl(ctx, cursor_slot);
  emit_instr_sym_num(ctx, "LD", "DE", (int)size);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", HOPPER_ARENA_SIZE);
  emit_jr_cond(ctx, "GT", fail_label);
  emit_store_label(ctx, "hopper_cursor");

  emit_load_label(ctx, "hopper_ref_next");
  emit_store_temp(ctx, ref_idx_slot);
  emit_load_temp_to_hl(ctx, ref_idx_slot);
  emit_instr_sym_num(ctx, "CP", "HL", HOPPER_REF_COUNT);
  emit_jr_cond(ctx, "GE", fail_label);

  emit_load_temp_to_hl(ctx, ref_idx_slot);
  emit_instr_sym_num(ctx, "MUL", "HL", HOPPER_REF_SIZE);
  emit_instr_sym_sym(ctx, "LD", "DE", "hopper_ref_pool");
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_store_temp(ctx, ref_ptr_slot);

  emit_load_temp_to_hl(ctx, cursor_slot);
  emit_store_ptr_offset(ctx, ref_ptr_slot, 0);
  emit_instr_sym_num(ctx, "LD", "HL", (int)size);
  emit_store_ptr_offset(ctx, ref_ptr_slot, 4);
  emit_instr_sym_num(ctx, "LD", "HL", layout_id);
  emit_store_ptr_offset(ctx, ref_ptr_slot, 8);

  emit_load_temp_to_hl(ctx, ref_idx_slot);
  emit_instr_sym_num(ctx, "ADD", "HL", 1);
  emit_store_label(ctx, "hopper_ref_next");

  emit_load_temp_to_hl(ctx, cursor_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "hopper_arena");
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_instr_sym_num(ctx, "LD", "BC", (int)size);
  emit_instr_sym_num(ctx, "LD", "A", 0);
  emit_instr0(ctx, "FILL");

  emit_load_temp_to_hl(ctx, ref_ptr_slot);
  emit_result_from_hl(ctx, res_enum, ok_idx, max_fields);
  emit_jr(ctx, end_label);

  emit_label(ctx, fail_label);
  {
    Expr trace = {0};
    Expr msg = {0};
    Expr cause = {0};
    Expr tag = {0};
    Expr fields = {0};
    trace.kind = EXPR_SYMBOL;
    trace.text = "#hopper_oom";
    msg.kind = EXPR_STRING;
    msg.text = "\"hopper: out of memory\"";
    cause.kind = EXPR_NIL;
    tag.kind = EXPR_NIL;
    fields.kind = EXPR_NIL;
    emit_err_construct(ctx, &trace, &msg, &tag, &fields);
  }
  emit_result_from_hl(ctx, res_enum, fail_idx, max_fields);

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}

static ZING_UNUSED void emit_hopper_bounds_check(Codegen *ctx, size_t ref_slot,
                                                 long field_offset,
                                                 long field_size) {
  size_t idx_slot = temp_push(ctx);
  size_t len_slot = temp_push(ctx);
  long end = 0;

  if (field_size <= 0) {
    temp_pop(ctx);
    temp_pop(ctx);
    return;
  }
  end = field_offset + field_size - 1;
  if (end < 0) {
    end = 0;
  }

  emit_load_ptr_offset(ctx, ref_slot, 4);
  emit_store_temp(ctx, len_slot);
  emit_instr_sym_num(ctx, "LD", "HL", (int)end);
  emit_store_temp(ctx, idx_slot);
  emit_bounds_check(ctx, idx_slot, len_slot);
  temp_pop(ctx);
  temp_pop(ctx);
}


void emit_hopper_bounds_check_fail(Codegen *ctx, size_t ref_slot,
                                          long field_offset, long field_size,
                                          const char *fail_label) {
  long end = 0;
  long pool_end = (HOPPER_REF_COUNT * HOPPER_REF_SIZE) - 1;

  if (field_size <= 0) {
    return;
  }
  end = field_offset + field_size - 1;
  if (end < 0) {
    end = 0;
  }

  emit_load_temp_to_hl(ctx, ref_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "hopper_ref_pool");
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", fail_label);

  emit_instr_sym_sym(ctx, "LD", "HL", "hopper_ref_pool");
  emit_instr_sym_num(ctx, "ADD", "HL", (int)pool_end);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, ref_slot);
  emit_instr_sym_sym(ctx, "GTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", fail_label);

  emit_load_ptr_offset(ctx, ref_slot, 4);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_instr_sym_num(ctx, "LD", "HL", (int)end);
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", fail_label);
}

void emit_hopper_field_addr(Codegen *ctx, size_t ref_slot,
                                   long field_offset) {
  emit_load_ptr_offset(ctx, ref_slot, 0);
  emit_instr_sym_sym(ctx, "LD", "DE", "hopper_arena");
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_instr_sym_num(ctx, "ADD", "HL", (int)field_offset);
}

void emit_hopper_bounds_check_offset_fail(Codegen *ctx, size_t ref_slot,
                                                 size_t off_slot, int size,
                                                 const char *fail_label) {
  long pool_end = (HOPPER_REF_COUNT * HOPPER_REF_SIZE) - 1;

  if (size <= 0) {
    return;
  }

  emit_load_temp_to_hl(ctx, ref_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "hopper_ref_pool");
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", fail_label);

  emit_instr_sym_sym(ctx, "LD", "HL", "hopper_ref_pool");
  emit_instr_sym_num(ctx, "ADD", "HL", (int)pool_end);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, ref_slot);
  emit_instr_sym_sym(ctx, "GTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", fail_label);

  emit_load_temp_to_hl(ctx, off_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 0);
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", fail_label);

  emit_load_temp_to_hl(ctx, off_slot);
  emit_instr_sym_num(ctx, "ADD", "HL", size - 1);
  emit_store_temp(ctx, off_slot);

  emit_load_ptr_offset(ctx, ref_slot, 4);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, off_slot);
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", fail_label);
}

void emit_hopper_addr_at(Codegen *ctx, size_t ref_slot,
                                size_t off_slot) {
  emit_load_ptr_offset(ctx, ref_slot, 0);
  emit_instr_sym_sym(ctx, "LD", "DE", "hopper_arena");
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_load_temp_to_hl(ctx, off_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
}

void emit_hopper_read_u8_at(Codegen *ctx, Expr *recv, Expr *offset) {
  size_t ref_slot = temp_push(ctx);
  size_t off_slot = temp_push(ctx);
  size_t addr_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  char fail_label[64];
  char end_label[64];

  snprintf(fail_label, sizeof(fail_label), "hopper_u8_fail_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_u8_end_%zu", id);

  emit_expr(ctx, recv);
  emit_store_temp(ctx, ref_slot);
  emit_expr(ctx, offset);
  emit_store_temp(ctx, off_slot);

  emit_hopper_bounds_check_offset_fail(ctx, ref_slot, off_slot, 1, fail_label);
  emit_hopper_addr_at(ctx, ref_slot, off_slot);
  emit_store_temp(ctx, addr_slot);

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_mem(ctx, "LD8U", "HL", "HL");
  emit_result_ok_from_hl(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, fail_label);
  emit_result_fail_err(ctx, "#t_hopper_oob_9Z6Q2M1N7V4B8C3X5K0L",
                       "\"hopper: offset out of bounds\"");

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_read_u16_at(Codegen *ctx, Expr *recv, Expr *offset) {
  size_t ref_slot = temp_push(ctx);
  size_t off_slot = temp_push(ctx);
  size_t addr_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  char fail_label[64];
  char end_label[64];

  snprintf(fail_label, sizeof(fail_label), "hopper_u16_fail_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_u16_end_%zu", id);

  emit_expr(ctx, recv);
  emit_store_temp(ctx, ref_slot);
  emit_expr(ctx, offset);
  emit_store_temp(ctx, off_slot);

  emit_hopper_bounds_check_offset_fail(ctx, ref_slot, off_slot, 2, fail_label);
  emit_hopper_addr_at(ctx, ref_slot, off_slot);
  emit_store_temp(ctx, addr_slot);

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_mem(ctx, "LD16U", "HL", "HL");
  emit_result_ok_from_hl(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, fail_label);
  emit_result_fail_err(ctx, "#t_hopper_oob_9Z6Q2M1N7V4B8C3X5K0L",
                       "\"hopper: offset out of bounds\"");

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_read_u32_at(Codegen *ctx, Expr *recv, Expr *offset) {
  size_t ref_slot = temp_push(ctx);
  size_t off_slot = temp_push(ctx);
  size_t addr_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  char fail_label[64];
  char end_label[64];

  snprintf(fail_label, sizeof(fail_label), "hopper_u32_fail_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_u32_end_%zu", id);

  emit_expr(ctx, recv);
  emit_store_temp(ctx, ref_slot);
  emit_expr(ctx, offset);
  emit_store_temp(ctx, off_slot);

  emit_hopper_bounds_check_offset_fail(ctx, ref_slot, off_slot, 4, fail_label);
  emit_hopper_addr_at(ctx, ref_slot, off_slot);
  emit_store_temp(ctx, addr_slot);

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_mem(ctx, "LD32", "HL", "HL");
  emit_result_ok_from_hl(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, fail_label);
  emit_result_fail_err(ctx, "#t_hopper_oob_9Z6Q2M1N7V4B8C3X5K0L",
                       "\"hopper: offset out of bounds\"");

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_write_u8_at(Codegen *ctx, Expr *recv, Expr *offset,
                                    Expr *value) {
  size_t ref_slot = temp_push(ctx);
  size_t off_slot = temp_push(ctx);
  size_t addr_slot = temp_push(ctx);
  size_t val_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  char fail_label[64];
  char end_label[64];

  snprintf(fail_label, sizeof(fail_label), "hopper_wu8_fail_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_wu8_end_%zu", id);

  emit_expr(ctx, recv);
  emit_store_temp(ctx, ref_slot);
  emit_expr(ctx, offset);
  emit_store_temp(ctx, off_slot);
  emit_expr(ctx, value);
  emit_store_temp(ctx, val_slot);

  emit_hopper_bounds_check_offset_fail(ctx, ref_slot, off_slot, 1, fail_label);
  emit_hopper_addr_at(ctx, ref_slot, off_slot);
  emit_store_temp(ctx, addr_slot);

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_mem_sym(ctx, "ST8", "DE", "HL");
  emit_result_ok_unit(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, fail_label);
  emit_result_fail_err(ctx, "#t_hopper_oob_9Z6Q2M1N7V4B8C3X5K0L",
                       "\"hopper: offset out of bounds\"");

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_write_u16_at(Codegen *ctx, Expr *recv, Expr *offset,
                                     Expr *value) {
  size_t ref_slot = temp_push(ctx);
  size_t off_slot = temp_push(ctx);
  size_t addr_slot = temp_push(ctx);
  size_t val_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  char fail_label[64];
  char end_label[64];

  snprintf(fail_label, sizeof(fail_label), "hopper_wu16_fail_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_wu16_end_%zu", id);

  emit_expr(ctx, recv);
  emit_store_temp(ctx, ref_slot);
  emit_expr(ctx, offset);
  emit_store_temp(ctx, off_slot);
  emit_expr(ctx, value);
  emit_store_temp(ctx, val_slot);

  emit_hopper_bounds_check_offset_fail(ctx, ref_slot, off_slot, 2, fail_label);
  emit_hopper_addr_at(ctx, ref_slot, off_slot);
  emit_store_temp(ctx, addr_slot);

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_mem_sym(ctx, "ST16", "DE", "HL");
  emit_result_ok_unit(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, fail_label);
  emit_result_fail_err(ctx, "#t_hopper_oob_9Z6Q2M1N7V4B8C3X5K0L",
                       "\"hopper: offset out of bounds\"");

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_write_u32_at(Codegen *ctx, Expr *recv, Expr *offset,
                                     Expr *value) {
  size_t ref_slot = temp_push(ctx);
  size_t off_slot = temp_push(ctx);
  size_t addr_slot = temp_push(ctx);
  size_t val_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  char fail_label[64];
  char end_label[64];

  snprintf(fail_label, sizeof(fail_label), "hopper_wu32_fail_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_wu32_end_%zu", id);

  emit_expr(ctx, recv);
  emit_store_temp(ctx, ref_slot);
  emit_expr(ctx, offset);
  emit_store_temp(ctx, off_slot);
  emit_expr(ctx, value);
  emit_store_temp(ctx, val_slot);

  emit_hopper_bounds_check_offset_fail(ctx, ref_slot, off_slot, 4, fail_label);
  emit_hopper_addr_at(ctx, ref_slot, off_slot);
  emit_store_temp(ctx, addr_slot);

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_mem_sym(ctx, "ST32", "DE", "HL");
  emit_result_ok_unit(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, fail_label);
  emit_result_fail_err(ctx, "#t_hopper_oob_9Z6Q2M1N7V4B8C3X5K0L",
                       "\"hopper: offset out of bounds\"");

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_field_get_bytes(Codegen *ctx, size_t ref_slot,
                                        const ZingLayoutField *field) {
  size_t addr_slot = temp_push(ctx);
  size_t buf_slot = temp_push(ctx);
  size_t dest_slot = temp_push(ctx);
  size_t src_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  char ok_label[64];
  char fail_label[64];
  char end_label[64];

  if (!field || field->size <= 0) {
    emit_result_ok_unit(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    return;
  }

  snprintf(ok_label, sizeof(ok_label), "hopper_buf_ok_%zu", id);
  snprintf(fail_label, sizeof(fail_label), "hopper_buf_fail_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_buf_end_%zu", id);

  emit_hopper_bounds_check_fail(ctx, ref_slot, field->offset, field->size,
                                fail_label);
  emit_instr_sym_num(ctx, "LD", "HL", (int)field->size);
  emit_instr_sym_num(ctx, "LD", "DE", BUFFER_CAP);
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", ok_label);
  emit_instr_sym_num(ctx, "LD", "HL", (int)field->size);
  emit_instr_sym_num(ctx, "LD", "DE", BUFFER_CAP);
  emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", ok_label);
  emit_jr(ctx, fail_label);
  emit_label(ctx, ok_label);

  emit_alloc_buffer(ctx);
  emit_store_temp(ctx, buf_slot);

  emit_hopper_field_addr(ctx, ref_slot, field->offset);
  emit_store_temp(ctx, addr_slot);

  emit_load_ptr_offset(ctx, buf_slot, 0);
  emit_store_temp(ctx, dest_slot);

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_store_temp(ctx, src_slot);

  emit_load_temp_to_hl(ctx, dest_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, src_slot);
  emit_instr_sym_num(ctx, "LD", "BC", (int)field->size);
  emit_instr0(ctx, "LDIR");

  emit_instr_sym_num(ctx, "LD", "HL", (int)field->size);
  emit_store_ptr_offset(ctx, buf_slot, 4);

  emit_load_temp_to_hl(ctx, buf_slot);
  emit_result_ok_from_hl(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, fail_label);
  emit_result_fail_err(ctx, "#bounds", "\"hopper: field out of bounds\"");

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_field_get_numeric(Codegen *ctx, size_t ref_slot,
                                          const ZingLayoutField *field) {
  size_t addr_slot = temp_push(ctx);
  size_t idx_slot = temp_push(ctx);
  size_t acc_slot = temp_push(ctx);
  size_t byte_slot = temp_push(ctx);
  size_t sign_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  int digit_count = field ? (int)field->pic_digits : 0;
  int scale = field ? (int)field->pic_scale : 0;
  int pow10 = 1;
  int pow10_ok = 1;
  char loop_label[64];
  char done_label[64];
  char fail_label[64];
  char bounds_label[64];
  char end_label[64];
  char sign_neg_label[64];
  char sign_pos_label[64];
  char sign_done_label[64];
  char scale_label[64];
  char scale_overflow_label[64];
  char neg_apply_label[64];
  char pos_apply_label[64];

  if (!field || field->size <= 0) {
    emit_result_ok_unit(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    return;
  }

  if (scale > 0) {
    pow10_ok = pic_pow10_i32(scale, &pow10);
  }

  snprintf(loop_label, sizeof(loop_label), "hopper_num_loop_%zu", id);
  snprintf(done_label, sizeof(done_label), "hopper_num_done_%zu", id);
  snprintf(fail_label, sizeof(fail_label), "hopper_num_fail_%zu", id);
  snprintf(bounds_label, sizeof(bounds_label), "hopper_num_bounds_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_num_end_%zu", id);
  snprintf(sign_neg_label, sizeof(sign_neg_label), "hopper_num_signneg_%zu", id);
  snprintf(sign_pos_label, sizeof(sign_pos_label), "hopper_num_signpos_%zu", id);
  snprintf(sign_done_label, sizeof(sign_done_label), "hopper_num_signdone_%zu", id);
  snprintf(scale_label, sizeof(scale_label), "hopper_num_scale_%zu", id);
  snprintf(scale_overflow_label, sizeof(scale_overflow_label),
           "hopper_num_scale_overflow_%zu", id);
  snprintf(neg_apply_label, sizeof(neg_apply_label), "hopper_num_applyneg_%zu",
           id);
  snprintf(pos_apply_label, sizeof(pos_apply_label), "hopper_num_applypos_%zu",
           id);

  emit_hopper_bounds_check_fail(ctx, ref_slot, field->offset, field->size,
                                bounds_label);
  emit_hopper_field_addr(ctx, ref_slot, field->offset);
  emit_store_temp(ctx, addr_slot);
  emit_instr_sym_num(ctx, "LD", "HL", 0);
  emit_store_temp(ctx, acc_slot);
  emit_instr_sym_num(ctx, "LD", "HL", 0);
  emit_store_temp(ctx, idx_slot);
  emit_instr_sym_num(ctx, "LD", "HL", 0);
  emit_store_temp(ctx, sign_slot);

  if (field->pic_signed) {
    emit_load_temp_to_hl(ctx, addr_slot);
    emit_instr_sym_mem(ctx, "LD8U", "HL", "HL");
    emit_store_temp(ctx, byte_slot);

    emit_load_temp_to_hl(ctx, byte_slot);
    emit_instr_sym_num(ctx, "LD", "DE", 45);
    emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
    emit_instr_sym_num(ctx, "CP", "HL", 0);
    emit_jr_cond(ctx, "NE", sign_neg_label);

    emit_load_temp_to_hl(ctx, byte_slot);
    emit_instr_sym_num(ctx, "LD", "DE", 43);
    emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
    emit_instr_sym_num(ctx, "CP", "HL", 0);
    emit_jr_cond(ctx, "NE", sign_pos_label);
    emit_jr(ctx, fail_label);

    emit_label(ctx, sign_neg_label);
    emit_instr_sym_num(ctx, "LD", "HL", 1);
    emit_store_temp(ctx, sign_slot);
    emit_jr(ctx, sign_done_label);

    emit_label(ctx, sign_pos_label);
    emit_instr_sym_num(ctx, "LD", "HL", 0);
    emit_store_temp(ctx, sign_slot);

    emit_label(ctx, sign_done_label);
    emit_load_temp_to_hl(ctx, addr_slot);
    emit_instr_sym_num(ctx, "ADD", "HL", 1);
    emit_store_temp(ctx, addr_slot);
  }

  emit_label(ctx, loop_label);
  emit_load_temp_to_hl(ctx, idx_slot);
  emit_instr_sym_num(ctx, "LD", "DE", digit_count);
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", done_label);

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, idx_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_instr_sym_mem(ctx, "LD8U", "HL", "HL");
  emit_store_temp(ctx, byte_slot);

  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 48);
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", fail_label);

  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 57);
  emit_instr_sym_sym(ctx, "GTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", fail_label);

  emit_load_temp_to_hl(ctx, acc_slot);
  emit_instr_sym_num(ctx, "MUL", "HL", 10);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "SUB", "HL", 48);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_store_temp(ctx, acc_slot);

  emit_load_temp_to_hl(ctx, idx_slot);
  emit_instr_sym_num(ctx, "ADD", "HL", 1);
  emit_store_temp(ctx, idx_slot);
  emit_jr(ctx, loop_label);

  emit_label(ctx, fail_label);
  emit_result_fail_err(ctx, "#pic_invalid", "\"pic: invalid digit\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, bounds_label);
  emit_result_fail_err(ctx, "#bounds", "\"hopper: field out of bounds\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, done_label);
  if (scale > 0) {
    if (!pow10_ok) {
      emit_jr(ctx, scale_overflow_label);
    } else {
      emit_load_temp_to_hl(ctx, acc_slot);
      emit_instr_sym_num(ctx, "LD", "DE", pow10);
      emit_instr_sym_sym(ctx, "REMS", "HL", "DE");
      emit_instr_sym_num(ctx, "CP", "HL", 0);
      emit_jr_cond(ctx, "NE", scale_label);
      emit_load_temp_to_hl(ctx, acc_slot);
      emit_instr_sym_num(ctx, "LD", "DE", pow10);
      emit_instr_sym_sym(ctx, "DIVS", "HL", "DE");
      emit_store_temp(ctx, acc_slot);
    }
  }
  emit_load_temp_to_hl(ctx, sign_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", pos_apply_label);
  emit_jr(ctx, neg_apply_label);

  emit_label(ctx, neg_apply_label);
  emit_load_temp_to_hl(ctx, acc_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_instr_sym_num(ctx, "LD", "HL", 0);
  emit_instr_sym_sym(ctx, "SUB", "HL", "DE");
  emit_store_temp(ctx, acc_slot);
  emit_jr(ctx, pos_apply_label);

  emit_label(ctx, pos_apply_label);
  emit_load_temp_to_hl(ctx, acc_slot);
  emit_result_ok_from_hl(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, scale_label);
  emit_result_fail_err(ctx, "#pic_invalid", "\"pic: scale mismatch\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, scale_overflow_label);
  emit_result_fail_err(ctx, "#overflow", "\"pic: overflow\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_field_format_display(Codegen *ctx, size_t ref_slot,
                                             const ZingLayoutField *field) {
  size_t addr_slot = temp_push(ctx);
  size_t buf_slot = temp_push(ctx);
  size_t out_slot = temp_push(ctx);
  size_t mask_idx_slot = temp_push(ctx);
  size_t digit_rem_slot = temp_push(ctx);
  size_t byte_slot = temp_push(ctx);
  size_t sign_slot = temp_push(ctx);
  size_t saw_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  int digit_count = field ? (int)field->pic_digits : 0;
  int mask_len = field ? (int)field->pic_edit_mask_len : 0;
  const char *mask = field ? field->pic_edit_mask : NULL;
  int has_sign = field ? field->pic_edit_has_sign : 0;
  const char *mask_label = NULL;
  char *mask_lit = NULL;
  size_t mask_label_len = 0;
  char ok_label[64];
  char fail_label[64];
  char bounds_label[64];
  char end_label[64];
  char loop_label[64];
  char done_label[64];
  char digit9_label[64];
  char digitz_label[64];
  char sign_byte_neg_label[64];
  char sign_byte_pos_label[64];
  char sign_byte_done_label[64];
  char mask_sign_label[64];
  char mask_sign_pos_label[64];
  char mask_sign_plus_label[64];
  char sign_invalid_label[64];
  char sign_missing_label[64];
  char digit_missing_label[64];
  char digit_invalid_label[64];
  char z_zero_label[64];
  char z_store_label[64];
  char saw_set_label[64];

  if (!field || !mask || mask_len <= 0 || digit_count <= 0) {
    emit_result_fail_err(ctx, "#pic_invalid", "\"pic: missing edit mask\"");
    goto done;
  }

  if (mask_len > BUFFER_CAP) {
    emit_result_fail_err(ctx, "#pic_invalid", "\"pic: format too long\"");
    goto done;
  }

  mask_lit = make_string_literal(mask, (size_t)mask_len);
  mask_label = get_string_label(ctx, mask_lit, NULL, &mask_label_len);
  free(mask_lit);
  mask_lit = NULL;
  if (!mask_label || (int)mask_label_len != mask_len) {
    emit_result_fail_err(ctx, "#pic_invalid", "\"pic: invalid edit mask\"");
    goto done;
  }

  snprintf(ok_label, sizeof(ok_label), "hopper_fmt_ok_%zu", id);
  snprintf(fail_label, sizeof(fail_label), "hopper_fmt_fail_%zu", id);
  snprintf(bounds_label, sizeof(bounds_label), "hopper_fmt_bounds_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_fmt_end_%zu", id);
  snprintf(loop_label, sizeof(loop_label), "hopper_fmt_loop_%zu", id);
  snprintf(done_label, sizeof(done_label), "hopper_fmt_done_%zu", id);
  snprintf(digit9_label, sizeof(digit9_label), "hopper_fmt_digit9_%zu", id);
  snprintf(digitz_label, sizeof(digitz_label), "hopper_fmt_digitz_%zu", id);
  snprintf(sign_byte_neg_label, sizeof(sign_byte_neg_label),
           "hopper_fmt_signneg_%zu", id);
  snprintf(sign_byte_pos_label, sizeof(sign_byte_pos_label),
           "hopper_fmt_signpos_%zu", id);
  snprintf(sign_byte_done_label, sizeof(sign_byte_done_label),
           "hopper_fmt_signdone_%zu", id);
  snprintf(mask_sign_label, sizeof(mask_sign_label),
           "hopper_fmt_masksign_%zu", id);
  snprintf(mask_sign_pos_label, sizeof(mask_sign_pos_label),
           "hopper_fmt_masksignpos_%zu", id);
  snprintf(mask_sign_plus_label, sizeof(mask_sign_plus_label),
           "hopper_fmt_masksignplus_%zu", id);
  snprintf(sign_invalid_label, sizeof(sign_invalid_label),
           "hopper_fmt_sign_invalid_%zu", id);
  snprintf(sign_missing_label, sizeof(sign_missing_label),
           "hopper_fmt_sign_missing_%zu", id);
  snprintf(digit_missing_label, sizeof(digit_missing_label),
           "hopper_fmt_digit_missing_%zu", id);
  snprintf(digit_invalid_label, sizeof(digit_invalid_label),
           "hopper_fmt_digit_invalid_%zu", id);
  snprintf(z_zero_label, sizeof(z_zero_label), "hopper_fmt_zzero_%zu", id);
  snprintf(z_store_label, sizeof(z_store_label), "hopper_fmt_zstore_%zu", id);
  snprintf(saw_set_label, sizeof(saw_set_label), "hopper_fmt_saw_%zu", id);

  emit_hopper_bounds_check_fail(ctx, ref_slot, field->offset, field->size,
                                bounds_label);
  emit_alloc_buffer(ctx);
  emit_store_temp(ctx, buf_slot);
  emit_load_ptr_offset(ctx, buf_slot, 0);
  emit_store_temp(ctx, out_slot);

  emit_load_temp_to_hl(ctx, out_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_instr_sym_sym(ctx, "LD", "HL", mask_label);
  emit_instr_sym_num(ctx, "ADD", "HL", STR_HEADER);
  emit_instr_sym_num(ctx, "LD", "BC", mask_len);
  emit_instr0(ctx, "LDIR");

  emit_instr_sym_num(ctx, "LD", "HL", mask_len);
  emit_store_ptr_offset(ctx, buf_slot, 4);

  emit_hopper_field_addr(ctx, ref_slot, field->offset);
  emit_store_temp(ctx, addr_slot);

  emit_instr_sym_num(ctx, "LD", "HL", 0);
  emit_store_temp(ctx, sign_slot);

  if (field->pic_signed) {
    emit_load_temp_to_hl(ctx, addr_slot);
    emit_instr_sym_mem(ctx, "LD8U", "HL", "HL");
    emit_store_temp(ctx, byte_slot);

    emit_load_temp_to_hl(ctx, byte_slot);
    emit_instr_sym_num(ctx, "LD", "DE", 45);
    emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
    emit_instr_sym_num(ctx, "CP", "HL", 0);
    emit_jr_cond(ctx, "NE", sign_byte_neg_label);

    emit_load_temp_to_hl(ctx, byte_slot);
    emit_instr_sym_num(ctx, "LD", "DE", 43);
    emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
    emit_instr_sym_num(ctx, "CP", "HL", 0);
    emit_jr_cond(ctx, "NE", sign_byte_pos_label);
    emit_jr(ctx, sign_invalid_label);

    emit_label(ctx, sign_byte_neg_label);
    emit_instr_sym_num(ctx, "LD", "HL", 1);
    emit_store_temp(ctx, sign_slot);
    emit_jr(ctx, sign_byte_done_label);

    emit_label(ctx, sign_byte_pos_label);
    emit_instr_sym_num(ctx, "LD", "HL", 0);
    emit_store_temp(ctx, sign_slot);

    emit_label(ctx, sign_byte_done_label);
    emit_load_temp_to_hl(ctx, addr_slot);
    emit_instr_sym_num(ctx, "ADD", "HL", 1);
    emit_store_temp(ctx, addr_slot);
  }

  if (field->pic_signed && !has_sign) {
    emit_load_temp_to_hl(ctx, sign_slot);
    emit_instr_sym_num(ctx, "CP", "HL", 0);
    emit_jr_cond(ctx, "EQ", ok_label);
    emit_jr(ctx, sign_missing_label);
  }

  emit_label(ctx, ok_label);
  emit_instr_sym_num(ctx, "LD", "HL", mask_len);
  emit_store_temp(ctx, mask_idx_slot);
  emit_instr_sym_num(ctx, "LD", "HL", digit_count);
  emit_store_temp(ctx, digit_rem_slot);
  emit_instr_sym_num(ctx, "LD", "HL", 0);
  emit_store_temp(ctx, saw_slot);

  emit_label(ctx, loop_label);
  emit_load_temp_to_hl(ctx, mask_idx_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", done_label);
  emit_load_temp_to_hl(ctx, mask_idx_slot);
  emit_instr_sym_num(ctx, "SUB", "HL", 1);
  emit_store_temp(ctx, mask_idx_slot);

  emit_load_temp_to_hl(ctx, out_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, mask_idx_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_instr_sym_mem(ctx, "LD8U", "HL", "HL");
  emit_store_temp(ctx, byte_slot);

  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 57);
  emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", digit9_label);

  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 90);
  emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", digitz_label);

  if (field->pic_signed) {
    emit_load_temp_to_hl(ctx, byte_slot);
    emit_instr_sym_num(ctx, "LD", "DE", 43);
    emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
    emit_instr_sym_num(ctx, "CP", "HL", 0);
    emit_jr_cond(ctx, "NE", mask_sign_label);

    emit_load_temp_to_hl(ctx, byte_slot);
    emit_instr_sym_num(ctx, "LD", "DE", 45);
    emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
    emit_instr_sym_num(ctx, "CP", "HL", 0);
    emit_jr_cond(ctx, "NE", mask_sign_label);
  }

  emit_jr(ctx, loop_label);

  emit_label(ctx, digit9_label);
  emit_load_temp_to_hl(ctx, digit_rem_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", digit_missing_label);
  emit_load_temp_to_hl(ctx, digit_rem_slot);
  emit_instr_sym_num(ctx, "SUB", "HL", 1);
  emit_store_temp(ctx, digit_rem_slot);
  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, digit_rem_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_instr_sym_mem(ctx, "LD8U", "HL", "HL");
  emit_store_temp(ctx, byte_slot);

  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 48);
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", digit_invalid_label);

  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 57);
  emit_instr_sym_sym(ctx, "GTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", digit_invalid_label);

  emit_load_temp_to_hl(ctx, out_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, mask_idx_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_mem_sym(ctx, "ST8", "DE", "HL");

  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 48);
  emit_jr_cond(ctx, "EQ", loop_label);
  emit_instr_sym_num(ctx, "LD", "HL", 1);
  emit_store_temp(ctx, saw_slot);
  emit_jr(ctx, loop_label);

  emit_label(ctx, digitz_label);
  emit_load_temp_to_hl(ctx, digit_rem_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", digit_missing_label);
  emit_load_temp_to_hl(ctx, digit_rem_slot);
  emit_instr_sym_num(ctx, "SUB", "HL", 1);
  emit_store_temp(ctx, digit_rem_slot);
  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, digit_rem_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_instr_sym_mem(ctx, "LD8U", "HL", "HL");
  emit_store_temp(ctx, byte_slot);

  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 48);
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", digit_invalid_label);

  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 57);
  emit_instr_sym_sym(ctx, "GTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", digit_invalid_label);

  emit_load_temp_to_hl(ctx, saw_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", z_zero_label);
  emit_jr(ctx, z_store_label);

  emit_label(ctx, z_zero_label);
  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 48);
  emit_jr_cond(ctx, "EQ", saw_set_label);

  emit_label(ctx, z_store_label);
  emit_load_temp_to_hl(ctx, out_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, mask_idx_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_mem_sym(ctx, "ST8", "DE", "HL");

  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 48);
  emit_jr_cond(ctx, "EQ", loop_label);
  emit_instr_sym_num(ctx, "LD", "HL", 1);
  emit_store_temp(ctx, saw_slot);
  emit_jr(ctx, loop_label);

  emit_label(ctx, saw_set_label);
  emit_load_temp_to_hl(ctx, out_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, mask_idx_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_instr_sym_num(ctx, "LD", "HL", 32);
  emit_instr_mem_sym(ctx, "ST8", "DE", "HL");
  emit_jr(ctx, loop_label);

  emit_label(ctx, mask_sign_label);
  emit_load_temp_to_hl(ctx, sign_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", mask_sign_pos_label);
  emit_load_temp_to_hl(ctx, out_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, mask_idx_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_instr_sym_num(ctx, "LD", "HL", 45);
  emit_instr_mem_sym(ctx, "ST8", "DE", "HL");
  emit_jr(ctx, loop_label);

  emit_label(ctx, mask_sign_pos_label);
  emit_load_temp_to_hl(ctx, out_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, mask_idx_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "LD", "BC", 43);
  emit_instr_sym_sym(ctx, "EQ", "HL", "BC");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", mask_sign_plus_label);
  emit_instr_sym_num(ctx, "LD", "HL", 32);
  emit_instr_mem_sym(ctx, "ST8", "DE", "HL");
  emit_jr(ctx, loop_label);

  emit_label(ctx, mask_sign_plus_label);
  emit_instr_sym_num(ctx, "LD", "HL", 43);
  emit_instr_mem_sym(ctx, "ST8", "DE", "HL");
  emit_jr(ctx, loop_label);

  emit_label(ctx, done_label);
  emit_load_temp_to_hl(ctx, digit_rem_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", digit_missing_label);
  emit_load_temp_to_hl(ctx, buf_slot);
  emit_result_ok_from_hl(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, sign_invalid_label);
  emit_result_fail_err(ctx, "#pic_invalid", "\"pic: invalid sign\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, sign_missing_label);
  emit_result_fail_err(ctx, "#pic_invalid", "\"pic: missing sign position\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, digit_missing_label);
  emit_result_fail_err(ctx, "#pic_invalid", "\"pic: edit mask mismatch\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, digit_invalid_label);
  emit_result_fail_err(ctx, "#pic_invalid", "\"pic: invalid digit\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, bounds_label);
  emit_result_fail_err(ctx, "#bounds", "\"hopper: field out of bounds\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, fail_label);
  emit_result_fail_err(ctx, "#pic_invalid", "\"pic: invalid edit mask\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, end_label);

done:
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_field_set_numeric(Codegen *ctx, size_t ref_slot,
                                          const ZingLayoutField *field,
                                          Expr *value) {
  size_t base_slot = temp_push(ctx);
  size_t addr_slot = temp_push(ctx);
  size_t val_slot = temp_push(ctx);
  size_t cnt_slot = temp_push(ctx);
  size_t digit_slot = temp_push(ctx);
  size_t sign_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  int digit_count = field ? (int)field->pic_digits : 0;
  int scale = field ? (int)field->pic_scale : 0;
  int pow10 = 1;
  int pow10_ok = 1;
  int max_val = INT_MAX;
  char loop_label[64];
  char done_label[64];
  char fail_label[64];
  char overflow_label[64];
  char bounds_label[64];
  char ok_label[64];
  char scale_label[64];
  char end_label[64];
  char sign_done_label[64];
  char neg_label[64];
  char pos_label[64];

  if (!field || field->size <= 0 || !value) {
    emit_result_ok_unit(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    return;
  }

  if (scale > 0) {
    pow10_ok = pic_pow10_i32(scale, &pow10);
    if (pow10_ok && pow10 > 0) {
      max_val = INT_MAX / pow10;
    } else {
      max_val = 0;
    }
  }

  snprintf(loop_label, sizeof(loop_label), "hopper_set_loop_%zu", id);
  snprintf(done_label, sizeof(done_label), "hopper_set_done_%zu", id);
  snprintf(fail_label, sizeof(fail_label), "hopper_set_fail_%zu", id);
  snprintf(overflow_label, sizeof(overflow_label), "hopper_set_overflow_%zu", id);
  snprintf(bounds_label, sizeof(bounds_label), "hopper_set_bounds_%zu", id);
  snprintf(ok_label, sizeof(ok_label), "hopper_set_ok_%zu", id);
  snprintf(scale_label, sizeof(scale_label), "hopper_set_scale_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_set_end_%zu", id);
  snprintf(sign_done_label, sizeof(sign_done_label), "hopper_set_signdone_%zu",
           id);
  snprintf(neg_label, sizeof(neg_label), "hopper_set_neg_%zu", id);
  snprintf(pos_label, sizeof(pos_label), "hopper_set_pos_%zu", id);

  emit_expr(ctx, value);
  emit_store_temp(ctx, val_slot);
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 0);
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", neg_label);
  emit_jr(ctx, pos_label);

  emit_label(ctx, pos_label);
  if (field->pic_signed) {
    emit_instr_sym_num(ctx, "LD", "HL", 43);
    emit_store_temp(ctx, sign_slot);
  } else {
    emit_instr_sym_num(ctx, "LD", "HL", 0);
    emit_store_temp(ctx, sign_slot);
  }
  emit_jr(ctx, sign_done_label);

  emit_label(ctx, neg_label);
  if (!field->pic_signed) {
    emit_jr(ctx, fail_label);
  }
  emit_instr_sym_num(ctx, "LD", "HL", 45);
  emit_store_temp(ctx, sign_slot);
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_instr_sym_num(ctx, "LD", "HL", 0);
  emit_instr_sym_sym(ctx, "SUB", "HL", "DE");
  emit_store_temp(ctx, val_slot);

  emit_label(ctx, sign_done_label);
  if (scale > 0) {
    if (!pow10_ok) {
      emit_jr(ctx, scale_label);
    } else {
      emit_load_temp_to_hl(ctx, val_slot);
      emit_instr_sym_num(ctx, "LD", "DE", max_val);
      emit_instr_sym_sym(ctx, "GTS", "HL", "DE");
      emit_instr_sym_num(ctx, "CP", "HL", 0);
      emit_jr_cond(ctx, "NE", overflow_label);
      emit_load_temp_to_hl(ctx, val_slot);
      emit_instr_sym_num(ctx, "MUL", "HL", pow10);
      emit_store_temp(ctx, val_slot);
    }
  }

  emit_hopper_bounds_check_fail(ctx, ref_slot, field->offset, field->size,
                                bounds_label);
  emit_hopper_field_addr(ctx, ref_slot, field->offset);
  emit_store_temp(ctx, base_slot);
  emit_instr_sym_num(ctx, "ADD", "HL", (int)field->size - 1);
  emit_store_temp(ctx, addr_slot);

  emit_instr_sym_num(ctx, "LD", "HL", digit_count);
  emit_store_temp(ctx, cnt_slot);

  emit_label(ctx, loop_label);
  emit_load_temp_to_hl(ctx, cnt_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", done_label);

  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 10);
  emit_instr_sym_sym(ctx, "REMS", "HL", "DE");
  emit_store_temp(ctx, digit_slot);
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 10);
  emit_instr_sym_sym(ctx, "DIVS", "HL", "DE");
  emit_store_temp(ctx, val_slot);

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, digit_slot);
  emit_instr_sym_num(ctx, "ADD", "HL", 48);
  emit_instr_mem_sym(ctx, "ST8", "DE", "HL");

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_num(ctx, "SUB", "HL", 1);
  emit_store_temp(ctx, addr_slot);

  emit_load_temp_to_hl(ctx, cnt_slot);
  emit_instr_sym_num(ctx, "SUB", "HL", 1);
  emit_store_temp(ctx, cnt_slot);
  emit_jr(ctx, loop_label);

  emit_label(ctx, done_label);
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", ok_label);
  emit_jr(ctx, overflow_label);

  emit_label(ctx, ok_label);
  if (field->pic_signed) {
    emit_load_temp_to_hl(ctx, base_slot);
    emit_instr_sym_sym(ctx, "LD", "DE", "HL");
    emit_load_temp_to_hl(ctx, sign_slot);
    emit_instr_mem_sym(ctx, "ST8", "DE", "HL");
  }
  emit_result_ok_unit(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, fail_label);
  emit_result_fail_err(ctx, "#pic_invalid", "\"pic: invalid sign\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, scale_label);
  emit_result_fail_err(ctx, "#overflow", "\"pic: overflow\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, overflow_label);
  emit_result_fail_err(ctx, "#overflow", "\"pic: overflow\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, bounds_label);
  emit_result_fail_err(ctx, "#bounds", "\"hopper: field out of bounds\"");

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_field_get_comp(Codegen *ctx, size_t ref_slot,
                                       const ZingLayoutField *field) {
  size_t addr_slot = temp_push(ctx);
  size_t acc_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  int scale = field ? (int)field->pic_scale : 0;
  int pow10 = 1;
  int pow10_ok = 1;
  char bounds_label[64];
  char scale_label[64];
  char scale_overflow_label[64];
  char end_label[64];

  if (!field || field->size <= 0) {
    emit_result_ok_unit(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    return;
  }

  if (scale > 0) {
    pow10_ok = pic_pow10_i32(scale, &pow10);
  }

  snprintf(bounds_label, sizeof(bounds_label), "hopper_comp_bounds_%zu", id);
  snprintf(scale_label, sizeof(scale_label), "hopper_comp_scale_%zu", id);
  snprintf(scale_overflow_label, sizeof(scale_overflow_label),
           "hopper_comp_scale_overflow_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_comp_end_%zu", id);

  emit_hopper_bounds_check_fail(ctx, ref_slot, field->offset, field->size,
                                bounds_label);
  emit_hopper_field_addr(ctx, ref_slot, field->offset);
  emit_store_temp(ctx, addr_slot);

  if (field->size == 2) {
    emit_load_temp_to_hl(ctx, addr_slot);
    if (field->pic_signed) {
      emit_instr_sym_mem(ctx, "LD16S", "HL", "HL");
    } else {
      emit_instr_sym_mem(ctx, "LD16U", "HL", "HL");
    }
  } else {
    emit_load_temp_to_hl(ctx, addr_slot);
    emit_instr_sym_mem(ctx, "LD32", "HL", "HL");
  }
  emit_store_temp(ctx, acc_slot);

  if (scale > 0) {
    if (!pow10_ok) {
      emit_jr(ctx, scale_overflow_label);
    } else {
      emit_load_temp_to_hl(ctx, acc_slot);
      emit_instr_sym_num(ctx, "LD", "DE", pow10);
      emit_instr_sym_sym(ctx, "REMS", "HL", "DE");
      emit_instr_sym_num(ctx, "CP", "HL", 0);
      emit_jr_cond(ctx, "NE", scale_label);
      emit_load_temp_to_hl(ctx, acc_slot);
      emit_instr_sym_num(ctx, "LD", "DE", pow10);
      emit_instr_sym_sym(ctx, "DIVS", "HL", "DE");
      emit_store_temp(ctx, acc_slot);
    }
  }

  emit_load_temp_to_hl(ctx, acc_slot);
  emit_result_ok_from_hl(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, scale_label);
  emit_result_fail_err(ctx, "#pic_invalid", "\"pic: scale mismatch\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, scale_overflow_label);
  emit_result_fail_err(ctx, "#overflow", "\"pic: overflow\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, bounds_label);
  emit_result_fail_err(ctx, "#bounds", "\"hopper: field out of bounds\"");

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_field_set_comp(Codegen *ctx, size_t ref_slot,
                                       const ZingLayoutField *field,
                                       Expr *value) {
  size_t addr_slot = temp_push(ctx);
  size_t val_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  int scale = field ? (int)field->pic_scale : 0;
  int pow10 = 1;
  int pow10_ok = 1;
  int min_val = 0;
  int max_val = 0;
  char bounds_label[64];
  char overflow_label[64];
  char scale_label[64];
  char negative_label[64];
  char ok_label[64];
  char end_label[64];

  if (!field || field->size <= 0 || !value) {
    emit_result_ok_unit(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    return;
  }

  if (field->size == 2) {
    if (field->pic_signed) {
      min_val = -32768;
      max_val = 32767;
    } else {
      min_val = 0;
      max_val = 65535;
    }
  } else {
    if (field->pic_signed) {
      min_val = INT_MIN;
      max_val = INT_MAX;
    } else {
      min_val = 0;
      max_val = INT_MAX;
    }
  }

  if (scale > 0) {
    pow10_ok = pic_pow10_i32(scale, &pow10);
    if (!pow10_ok || pow10 <= 0) {
      max_val = 0;
      min_val = 0;
    } else {
      max_val = max_val / pow10;
      min_val = min_val / pow10;
    }
  }

  snprintf(bounds_label, sizeof(bounds_label), "hopper_comp_set_bounds_%zu", id);
  snprintf(overflow_label, sizeof(overflow_label), "hopper_comp_set_overflow_%zu", id);
  snprintf(scale_label, sizeof(scale_label), "hopper_comp_set_scale_%zu", id);
  snprintf(negative_label, sizeof(negative_label), "hopper_comp_set_negative_%zu", id);
  snprintf(ok_label, sizeof(ok_label), "hopper_comp_set_ok_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_comp_set_end_%zu", id);

  emit_expr(ctx, value);
  emit_store_temp(ctx, val_slot);

  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 0);
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", negative_label);

  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "LD", "DE", max_val);
  emit_instr_sym_sym(ctx, "GTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", overflow_label);
  emit_jr(ctx, ok_label);

  emit_label(ctx, negative_label);
  if (!field->pic_signed) {
    emit_jr(ctx, overflow_label);
  }
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "LD", "DE", min_val);
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", overflow_label);

  emit_label(ctx, ok_label);
  if (scale > 0) {
    if (!pow10_ok) {
      emit_jr(ctx, scale_label);
    } else {
      emit_load_temp_to_hl(ctx, val_slot);
      emit_instr_sym_num(ctx, "MUL", "HL", pow10);
      emit_store_temp(ctx, val_slot);
    }
  }

  emit_hopper_bounds_check_fail(ctx, ref_slot, field->offset, field->size,
                                bounds_label);
  emit_hopper_field_addr(ctx, ref_slot, field->offset);
  emit_store_temp(ctx, addr_slot);

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, val_slot);
  if (field->size == 2) {
    emit_instr_mem_sym(ctx, "ST16", "DE", "HL");
  } else {
    emit_instr_mem_sym(ctx, "ST32", "DE", "HL");
  }
  emit_result_ok_unit(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, scale_label);
  emit_result_fail_err(ctx, "#overflow", "\"pic: overflow\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, overflow_label);
  emit_result_fail_err(ctx, "#overflow", "\"pic: overflow\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, bounds_label);
  emit_result_fail_err(ctx, "#bounds", "\"hopper: field out of bounds\"");

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_field_set_bytes(Codegen *ctx, size_t ref_slot,
                                        const ZingLayoutField *field,
                                        Expr *value) {
  size_t addr_slot = temp_push(ctx);
  size_t val_slot = temp_push(ctx);
  size_t len_slot = temp_push(ctx);
  size_t src_slot = temp_push(ctx);
  size_t rem_slot = temp_push(ctx);
  size_t pad_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  char ok_label[64];
  char done_label[64];
  char fail_label[64];
  char bounds_label[64];
  char end_label[64];

  if (!field || field->size <= 0 || !value) {
    emit_result_ok_unit(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    return;
  }

  snprintf(ok_label, sizeof(ok_label), "hopper_bytes_ok_%zu", id);
  snprintf(done_label, sizeof(done_label), "hopper_bytes_done_%zu", id);
  snprintf(fail_label, sizeof(fail_label), "hopper_bytes_fail_%zu", id);
  snprintf(bounds_label, sizeof(bounds_label), "hopper_bytes_bounds_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_bytes_end_%zu", id);

  emit_expr(ctx, value);
  emit_store_temp(ctx, val_slot);
  emit_load_ptr_offset(ctx, val_slot, 4);
  emit_store_temp(ctx, len_slot);

  emit_hopper_bounds_check_fail(ctx, ref_slot, field->offset, field->size,
                                bounds_label);
  emit_load_temp_to_hl(ctx, len_slot);
  emit_instr_sym_num(ctx, "LD", "DE", (int)field->size);
  emit_instr_sym_sym(ctx, "GTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", ok_label);
  emit_jr(ctx, fail_label);
  emit_label(ctx, ok_label);

  emit_hopper_field_addr(ctx, ref_slot, field->offset);
  emit_store_temp(ctx, addr_slot);

  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "ADD", "HL", 8);
  emit_store_temp(ctx, src_slot);

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, len_slot);
  emit_instr_sym_sym(ctx, "LD", "BC", "HL");
  emit_load_temp_to_hl(ctx, src_slot);
  emit_instr0(ctx, "LDIR");

  emit_load_temp_to_hl(ctx, len_slot);
  emit_instr_sym_num(ctx, "LD", "DE", (int)field->size);
  emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", done_label);

  emit_load_temp_to_hl(ctx, len_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_instr_sym_num(ctx, "LD", "HL", (int)field->size);
  emit_instr_sym_sym(ctx, "SUB", "HL", "DE");
  emit_store_temp(ctx, rem_slot);

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, len_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_store_temp(ctx, pad_slot);

  emit_load_temp_to_hl(ctx, rem_slot);
  emit_instr_sym_sym(ctx, "LD", "BC", "HL");
  emit_load_temp_to_hl(ctx, pad_slot);
  emit_instr_sym_num(ctx, "LD", "A", 32);
  emit_instr0(ctx, "FILL");

  emit_label(ctx, done_label);
  emit_result_ok_unit(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, fail_label);
  emit_result_fail_err(ctx, "#pic_invalid", "\"pic: invalid length\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, bounds_label);
  emit_result_fail_err(ctx, "#bounds", "\"hopper: field out of bounds\"");

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_field_set_raw_bytes(Codegen *ctx, size_t ref_slot,
                                            const ZingLayoutField *field,
                                            Expr *value) {
  size_t addr_slot = temp_push(ctx);
  size_t val_slot = temp_push(ctx);
  size_t len_slot = temp_push(ctx);
  size_t src_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  char ok_label[64];
  char fail_label[64];
  char bounds_label[64];
  char end_label[64];

  if (!field || field->size <= 0 || !value) {
    emit_result_ok_unit(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    return;
  }

  snprintf(ok_label, sizeof(ok_label), "hopper_raw_ok_%zu", id);
  snprintf(fail_label, sizeof(fail_label), "hopper_raw_fail_%zu", id);
  snprintf(bounds_label, sizeof(bounds_label), "hopper_raw_bounds_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_raw_end_%zu", id);

  emit_expr(ctx, value);
  emit_store_temp(ctx, val_slot);
  emit_load_ptr_offset(ctx, val_slot, 4);
  emit_store_temp(ctx, len_slot);

  emit_hopper_bounds_check_fail(ctx, ref_slot, field->offset, field->size,
                                bounds_label);
  emit_load_temp_to_hl(ctx, len_slot);
  emit_instr_sym_num(ctx, "LD", "DE", (int)field->size);
  emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", ok_label);
  emit_jr(ctx, fail_label);
  emit_label(ctx, ok_label);

  emit_hopper_field_addr(ctx, ref_slot, field->offset);
  emit_store_temp(ctx, addr_slot);

  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "ADD", "HL", 8);
  emit_store_temp(ctx, src_slot);

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, len_slot);
  emit_instr_sym_sym(ctx, "LD", "BC", "HL");
  emit_load_temp_to_hl(ctx, src_slot);
  emit_instr0(ctx, "LDIR");

  emit_result_ok_unit(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, fail_label);
  emit_result_fail_err(ctx, "#pic_invalid", "\"pic: invalid length\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, bounds_label);
  emit_result_fail_err(ctx, "#bounds", "\"hopper: field out of bounds\"");

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_field_get_comp3(Codegen *ctx, size_t ref_slot,
                                        const ZingLayoutField *field) {
  size_t addr_slot = temp_push(ctx);
  size_t idx_slot = temp_push(ctx);
  size_t acc_slot = temp_push(ctx);
  size_t byte_slot = temp_push(ctx);
  size_t hi_slot = temp_push(ctx);
  size_t lo_slot = temp_push(ctx);
  size_t sign_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  int scale = field ? (int)field->pic_scale : 0;
  int pow10 = 1;
  int pow10_ok = 1;
  char loop_label[64];
  char done_label[64];
  char fail_label[64];
  char bounds_label[64];
  char end_label[64];
  char last_label[64];
  char neg_label[64];
  char pos_label[64];
  char sign_done_label[64];
  char neg_apply_label[64];
  char pos_apply_label[64];
  char scale_label[64];
  char scale_overflow_label[64];

  if (!field || field->size <= 0) {
    emit_result_ok_unit(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    return;
  }

  if (scale > 0) {
    pow10_ok = pic_pow10_i32(scale, &pow10);
  }

  snprintf(loop_label, sizeof(loop_label), "hopper_c3_loop_%zu", id);
  snprintf(done_label, sizeof(done_label), "hopper_c3_done_%zu", id);
  snprintf(fail_label, sizeof(fail_label), "hopper_c3_fail_%zu", id);
  snprintf(bounds_label, sizeof(bounds_label), "hopper_c3_bounds_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_c3_end_%zu", id);
  snprintf(last_label, sizeof(last_label), "hopper_c3_last_%zu", id);
  snprintf(neg_label, sizeof(neg_label), "hopper_c3_neg_%zu", id);
  snprintf(pos_label, sizeof(pos_label), "hopper_c3_pos_%zu", id);
  snprintf(sign_done_label, sizeof(sign_done_label), "hopper_c3_signdone_%zu",
           id);
  snprintf(neg_apply_label, sizeof(neg_apply_label), "hopper_c3_applyneg_%zu",
           id);
  snprintf(pos_apply_label, sizeof(pos_apply_label), "hopper_c3_applypos_%zu",
           id);
  snprintf(scale_label, sizeof(scale_label), "hopper_c3_scale_%zu", id);
  snprintf(scale_overflow_label, sizeof(scale_overflow_label),
           "hopper_c3_scale_overflow_%zu", id);

  emit_hopper_bounds_check_fail(ctx, ref_slot, field->offset, field->size,
                                bounds_label);
  emit_hopper_field_addr(ctx, ref_slot, field->offset);
  emit_store_temp(ctx, addr_slot);
  emit_instr_sym_num(ctx, "LD", "HL", 0);
  emit_store_temp(ctx, acc_slot);
  emit_instr_sym_num(ctx, "LD", "HL", 0);
  emit_store_temp(ctx, idx_slot);
  emit_instr_sym_num(ctx, "LD", "HL", 0);
  emit_store_temp(ctx, sign_slot);

  emit_label(ctx, loop_label);
  emit_load_temp_to_hl(ctx, idx_slot);
  emit_instr_sym_num(ctx, "LD", "DE", (int)field->size);
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", done_label);

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, idx_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_instr_sym_mem(ctx, "LD8U", "HL", "HL");
  emit_store_temp(ctx, byte_slot);

  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 16);
  emit_instr_sym_sym(ctx, "DIVS", "HL", "DE");
  emit_store_temp(ctx, hi_slot);
  emit_load_temp_to_hl(ctx, byte_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 16);
  emit_instr_sym_sym(ctx, "REMS", "HL", "DE");
  emit_store_temp(ctx, lo_slot);

  emit_load_temp_to_hl(ctx, idx_slot);
  emit_instr_sym_num(ctx, "LD", "DE", (int)(field->size - 1));
  emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", last_label);

  emit_load_temp_to_hl(ctx, hi_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 9);
  emit_instr_sym_sym(ctx, "GTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", fail_label);
  emit_load_temp_to_hl(ctx, acc_slot);
  emit_instr_sym_num(ctx, "MUL", "HL", 10);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, hi_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_store_temp(ctx, acc_slot);

  emit_load_temp_to_hl(ctx, lo_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 9);
  emit_instr_sym_sym(ctx, "GTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", fail_label);
  emit_load_temp_to_hl(ctx, acc_slot);
  emit_instr_sym_num(ctx, "MUL", "HL", 10);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, lo_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_store_temp(ctx, acc_slot);

  emit_load_temp_to_hl(ctx, idx_slot);
  emit_instr_sym_num(ctx, "ADD", "HL", 1);
  emit_store_temp(ctx, idx_slot);
  emit_jr(ctx, loop_label);

  emit_label(ctx, last_label);
  emit_load_temp_to_hl(ctx, hi_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 9);
  emit_instr_sym_sym(ctx, "GTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", fail_label);
  emit_load_temp_to_hl(ctx, acc_slot);
  emit_instr_sym_num(ctx, "MUL", "HL", 10);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, hi_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_store_temp(ctx, acc_slot);

  emit_load_temp_to_hl(ctx, lo_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 13);
  emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", neg_label);

  emit_load_temp_to_hl(ctx, lo_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 12);
  emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", pos_label);
  emit_load_temp_to_hl(ctx, lo_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 15);
  emit_instr_sym_sym(ctx, "EQ", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", pos_label);
  emit_jr(ctx, fail_label);

  emit_label(ctx, neg_label);
  if (!field->pic_signed) {
    emit_jr(ctx, fail_label);
  }
  emit_instr_sym_num(ctx, "LD", "HL", 1);
  emit_store_temp(ctx, sign_slot);
  emit_jr(ctx, sign_done_label);

  emit_label(ctx, pos_label);
  emit_instr_sym_num(ctx, "LD", "HL", 0);
  emit_store_temp(ctx, sign_slot);

  emit_label(ctx, sign_done_label);
  emit_load_temp_to_hl(ctx, idx_slot);
  emit_instr_sym_num(ctx, "ADD", "HL", 1);
  emit_store_temp(ctx, idx_slot);
  emit_jr(ctx, loop_label);

  emit_label(ctx, fail_label);
  emit_result_fail_err(ctx, "#pic_invalid", "\"pic: invalid comp3\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, bounds_label);
  emit_result_fail_err(ctx, "#bounds", "\"hopper: field out of bounds\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, done_label);
  if (scale > 0) {
    if (!pow10_ok) {
      emit_jr(ctx, scale_overflow_label);
    } else {
      emit_load_temp_to_hl(ctx, acc_slot);
      emit_instr_sym_num(ctx, "LD", "DE", pow10);
      emit_instr_sym_sym(ctx, "REMS", "HL", "DE");
      emit_instr_sym_num(ctx, "CP", "HL", 0);
      emit_jr_cond(ctx, "NE", scale_label);
      emit_load_temp_to_hl(ctx, acc_slot);
      emit_instr_sym_num(ctx, "LD", "DE", pow10);
      emit_instr_sym_sym(ctx, "DIVS", "HL", "DE");
      emit_store_temp(ctx, acc_slot);
    }
  }
  emit_load_temp_to_hl(ctx, sign_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", pos_apply_label);
  emit_jr(ctx, neg_apply_label);

  emit_label(ctx, neg_apply_label);
  emit_load_temp_to_hl(ctx, acc_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_instr_sym_num(ctx, "LD", "HL", 0);
  emit_instr_sym_sym(ctx, "SUB", "HL", "DE");
  emit_store_temp(ctx, acc_slot);
  emit_jr(ctx, pos_apply_label);

  emit_label(ctx, pos_apply_label);
  emit_load_temp_to_hl(ctx, acc_slot);
  emit_result_ok_from_hl(ctx);
  emit_jr(ctx, end_label);

  emit_label(ctx, scale_label);
  emit_result_fail_err(ctx, "#pic_invalid", "\"pic: scale mismatch\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, scale_overflow_label);
  emit_result_fail_err(ctx, "#overflow", "\"pic: overflow\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}

void emit_hopper_field_set_comp3(Codegen *ctx, size_t ref_slot,
                                        const ZingLayoutField *field,
                                        Expr *value) {
  size_t addr_slot = temp_push(ctx);
  size_t idx_slot = temp_push(ctx);
  size_t val_slot = temp_push(ctx);
  size_t sign_slot = temp_push(ctx);
  size_t low_slot = temp_push(ctx);
  size_t high_slot = temp_push(ctx);
  size_t id = ctx->label_id++;
  int scale = field ? (int)field->pic_scale : 0;
  int pow10 = 1;
  int pow10_ok = 1;
  int max_val = INT_MAX;
  char loop_label[64];
  char done_label[64];
  char fail_label[64];
  char overflow_label[64];
  char bounds_label[64];
  char end_label[64];
  char last_label[64];
  char pack_label[64];
  char ok_label[64];
  char neg_label[64];
  char pos_label[64];
  char sign_done_label[64];
  char scale_label[64];

  if (!field || field->size <= 0 || !value) {
    emit_result_ok_unit(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    temp_pop(ctx);
    return;
  }

  if (scale > 0) {
    pow10_ok = pic_pow10_i32(scale, &pow10);
    if (pow10_ok && pow10 > 0) {
      max_val = INT_MAX / pow10;
    } else {
      max_val = 0;
    }
  }

  snprintf(loop_label, sizeof(loop_label), "hopper_c3_set_%zu", id);
  snprintf(done_label, sizeof(done_label), "hopper_c3_set_done_%zu", id);
  snprintf(fail_label, sizeof(fail_label), "hopper_c3_set_fail_%zu", id);
  snprintf(overflow_label, sizeof(overflow_label), "hopper_c3_set_overflow_%zu", id);
  snprintf(bounds_label, sizeof(bounds_label), "hopper_c3_set_bounds_%zu", id);
  snprintf(end_label, sizeof(end_label), "hopper_c3_set_end_%zu", id);
  snprintf(last_label, sizeof(last_label), "hopper_c3_set_last_%zu", id);
  snprintf(pack_label, sizeof(pack_label), "hopper_c3_set_pack_%zu", id);
  snprintf(ok_label, sizeof(ok_label), "hopper_c3_set_ok_%zu", id);
  snprintf(neg_label, sizeof(neg_label), "hopper_c3_set_neg_%zu", id);
  snprintf(pos_label, sizeof(pos_label), "hopper_c3_set_pos_%zu", id);
  snprintf(sign_done_label, sizeof(sign_done_label), "hopper_c3_set_signdone_%zu",
           id);
  snprintf(scale_label, sizeof(scale_label), "hopper_c3_set_scale_%zu", id);

  emit_expr(ctx, value);
  emit_store_temp(ctx, val_slot);
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 0);
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "NE", neg_label);
  emit_jr(ctx, pos_label);

  emit_label(ctx, pos_label);
  emit_instr_sym_num(ctx, "LD", "HL", 12);
  emit_store_temp(ctx, sign_slot);
  emit_jr(ctx, sign_done_label);

  emit_label(ctx, neg_label);
  if (!field->pic_signed) {
    emit_jr(ctx, fail_label);
  }
  emit_instr_sym_num(ctx, "LD", "HL", 13);
  emit_store_temp(ctx, sign_slot);
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_instr_sym_num(ctx, "LD", "HL", 0);
  emit_instr_sym_sym(ctx, "SUB", "HL", "DE");
  emit_store_temp(ctx, val_slot);

  emit_label(ctx, sign_done_label);
  if (scale > 0) {
    if (!pow10_ok) {
      emit_jr(ctx, scale_label);
    } else {
      emit_load_temp_to_hl(ctx, val_slot);
      emit_instr_sym_num(ctx, "LD", "DE", max_val);
      emit_instr_sym_sym(ctx, "GTS", "HL", "DE");
      emit_instr_sym_num(ctx, "CP", "HL", 0);
      emit_jr_cond(ctx, "NE", overflow_label);
      emit_load_temp_to_hl(ctx, val_slot);
      emit_instr_sym_num(ctx, "MUL", "HL", pow10);
      emit_store_temp(ctx, val_slot);
    }
  }
  emit_hopper_bounds_check_fail(ctx, ref_slot, field->offset, field->size,
                                bounds_label);
  emit_hopper_field_addr(ctx, ref_slot, field->offset);
  emit_instr_sym_num(ctx, "ADD", "HL", (int)field->size - 1);
  emit_store_temp(ctx, addr_slot);
  emit_instr_sym_num(ctx, "LD", "HL", 0);
  emit_store_temp(ctx, idx_slot);

  emit_label(ctx, loop_label);
  emit_load_temp_to_hl(ctx, idx_slot);
  emit_instr_sym_num(ctx, "LD", "DE", (int)field->size);
  emit_instr_sym_sym(ctx, "LTS", "HL", "DE");
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", done_label);

  emit_load_temp_to_hl(ctx, idx_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", last_label);

  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 10);
  emit_instr_sym_sym(ctx, "REMS", "HL", "DE");
  emit_store_temp(ctx, low_slot);
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 10);
  emit_instr_sym_sym(ctx, "DIVS", "HL", "DE");
  emit_store_temp(ctx, val_slot);
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 10);
  emit_instr_sym_sym(ctx, "REMS", "HL", "DE");
  emit_store_temp(ctx, high_slot);
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 10);
  emit_instr_sym_sym(ctx, "DIVS", "HL", "DE");
  emit_store_temp(ctx, val_slot);
  emit_jr(ctx, pack_label);

  emit_label(ctx, last_label);
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 10);
  emit_instr_sym_sym(ctx, "REMS", "HL", "DE");
  emit_store_temp(ctx, high_slot);
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "LD", "DE", 10);
  emit_instr_sym_sym(ctx, "DIVS", "HL", "DE");
  emit_store_temp(ctx, val_slot);
  emit_load_temp_to_hl(ctx, sign_slot);
  emit_store_temp(ctx, low_slot);

  emit_label(ctx, pack_label);
  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_sym(ctx, "LD", "BC", "HL");
  emit_load_temp_to_hl(ctx, high_slot);
  emit_instr_sym_num(ctx, "MUL", "HL", 16);
  emit_instr_sym_sym(ctx, "LD", "DE", "HL");
  emit_load_temp_to_hl(ctx, low_slot);
  emit_instr_sym_sym(ctx, "ADD", "HL", "DE");
  emit_instr_sym_sym(ctx, "LD", "DE", "BC");
  emit_instr_mem_sym(ctx, "ST8", "DE", "HL");

  emit_load_temp_to_hl(ctx, addr_slot);
  emit_instr_sym_num(ctx, "SUB", "HL", 1);
  emit_store_temp(ctx, addr_slot);

  emit_load_temp_to_hl(ctx, idx_slot);
  emit_instr_sym_num(ctx, "ADD", "HL", 1);
  emit_store_temp(ctx, idx_slot);
  emit_jr(ctx, loop_label);

  emit_label(ctx, done_label);
  emit_load_temp_to_hl(ctx, val_slot);
  emit_instr_sym_num(ctx, "CP", "HL", 0);
  emit_jr_cond(ctx, "EQ", ok_label);
  emit_jr(ctx, overflow_label);

  emit_label(ctx, fail_label);
  emit_result_fail_err(ctx, "#pic_invalid", "\"pic: invalid sign\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, scale_label);
  emit_result_fail_err(ctx, "#overflow", "\"pic: overflow\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, overflow_label);
  emit_result_fail_err(ctx, "#overflow", "\"pic: overflow\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, bounds_label);
  emit_result_fail_err(ctx, "#bounds", "\"hopper: field out of bounds\"");
  emit_jr(ctx, end_label);

  emit_label(ctx, ok_label);
  emit_result_ok_unit(ctx);

  emit_label(ctx, end_label);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
  temp_pop(ctx);
}
#include "codegen_internal.h"
#include "emitter.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
