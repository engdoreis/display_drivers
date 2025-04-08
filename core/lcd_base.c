
// Copyright (c) 2022 Douglas Reis.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "lcd_base.h"

#include "font.h"

Result LCD_Init(LCD_Context *ctx, LCD_Interface *interface, uint32_t width, uint32_t height) {
  ctx->interface = interface;
  ctx->height    = height;
  ctx->width     = width;
  return (Result){.code = 0};
}
