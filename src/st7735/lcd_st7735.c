// Copyright (c) 2022 Douglas Reis.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "lcd_st7735.h"

#include <stdint.h>
#include <stdlib.h>

#include "lcd_st7735_cmds.h"
#include "lcd_st7735_init.h"

// clang-format on
static void write_command(St7735Context *ctx, uint8_t command) {
  uint16_t value = (command & 0x00FF);
  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, false);
  ctx->parent.interface->spi_write(ctx->parent.interface->handle, (uint8_t *)&value, 1);
}

static void write_buffer(St7735Context *ctx, const uint8_t *buffer, size_t length) {
  if (length) {
    ctx->parent.interface->spi_write(ctx->parent.interface->handle, (uint8_t *)buffer, length);
  }
}

static inline void delay(St7735Context *ctx, uint32_t millisecond) {
  ctx->parent.interface->timer_delay(ctx->parent.interface->handle, millisecond);
}

static void run_script(St7735Context *ctx, const uint8_t *addr) {
  uint8_t numCommands, numArgs;
  uint16_t delay_ms;

  numCommands = NEXT_BYTE(addr);  // Number of commands to follow

  while (numCommands--) {  // For each command...
    write_command(ctx, NEXT_BYTE(addr));

    numArgs  = NEXT_BYTE(addr);  // Number of args to follow
    delay_ms = numArgs & DELAY;  // If hibit set, delay follows args
    numArgs &= ~DELAY;           // Mask out delay bit

    ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
    write_buffer(ctx, addr, numArgs);
    ctx->parent.interface->gpio_write(ctx->parent.interface->handle, true, true);
    addr += numArgs;

    if (delay_ms) {
      delay_ms = NEXT_BYTE(addr);                     // Read post-command delay time (ms)
      delay_ms = (delay_ms == 255) ? 500 : delay_ms;  // If 255, delay for 500 ms
      delay(ctx, delay_ms);
    }
  }
}

static void set_address(St7735Context *ctx, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
  y0 += ctx->row_offset;
  y1 += ctx->row_offset;
  x0 += ctx->col_offset;
  x1 += ctx->col_offset;

  {
    uint8_t coordinate[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    write_command(ctx, ST7735_CASET);  // Column addr set
    ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
    write_buffer(ctx, coordinate, sizeof(coordinate));
    ctx->parent.interface->gpio_write(ctx->parent.interface->handle, true, true);
  }
  {
    uint8_t coordinate[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    write_command(ctx, ST7735_RASET);  // Row addr set
    ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
    write_buffer(ctx, coordinate, sizeof(coordinate));
    ctx->parent.interface->gpio_write(ctx->parent.interface->handle, true, true);
  }

  write_command(ctx, ST7735_RAMWR);  // write to RAM
}

static void write_register(St7735Context *ctx, uint8_t addr, uint8_t value) {
  write_command(ctx, addr);
  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
  write_buffer(ctx, (uint8_t *)&value, sizeof(value));
  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, true, true);
}

static uint8_t set_orientation(St7735Context *ctx, LCD_Orientation orientation) {
  uint8_t madctl          = 0;
  ctx->parent.orientation = orientation;
  switch (orientation) {
    case LCD_Rotate0:
      madctl = ST77_MADCTL_MV | ST77_MADCTL_MX;
      break;
    case LCD_Rotate90:
      madctl = ST77_MADCTL_MX | ST77_MADCTL_MY;
      SWAP(ctx->parent.width, ctx->parent.height, size_t);
      SWAP(ctx->col_offset, ctx->row_offset, size_t);
      break;
    case LCD_Rotate180:
      madctl = ST77_MADCTL_MV | ST77_MADCTL_MY;
      break;
    case LCD_Rotate270:
      SWAP(ctx->parent.width, ctx->parent.height, size_t);
      SWAP(ctx->col_offset, ctx->row_offset, size_t);
      break;
    default:
      break;
  }
  return madctl;
}

Result lcd_st7735_init(St7735Context *ctx, LCD_Interface *interface) {
  LCD_Init(&ctx->parent, interface, 160, 128, LCD_Rotate0);
  lcd_st7735_set_font_colors(ctx, 0xFFFFFF, 0x000000);
  ctx->col_offset = ctx->row_offset = 0;

  return (Result){.code = 0};
}

Result lcd_st7735_startup(St7735Context *ctx) {
  int32_t result = 0;

  run_script(ctx, init_script_b);
  run_script(ctx, init_script_r);
  run_script(ctx, init_script_r3);

  return (Result){.code = result};
}

Result lcd_st7735_set_orientation(St7735Context *ctx, LCD_Orientation orientation) {
  uint8_t madctl = set_orientation(ctx, orientation);

  write_register(ctx, ST7735_MADCTL, madctl | ST77_MADCTL_RGB);

  return (Result){.code = 0};
}

Result lcd_st7735_clean(St7735Context *ctx) {
  size_t w, h;
  lcd_st7735_get_resolution(ctx, &h, &w);
  return lcd_st7735_fill_rectangle(ctx, (LCD_rectangle){.origin = {.x = 0, .y = 0}, .width = w, .height = h}, 0xffffff);
}

Result lcd_st7735_draw_pixel(St7735Context *ctx, LCD_Point pixel, uint32_t color) {
  if ((pixel.x < 0) || (pixel.x >= ctx->parent.width) || (pixel.y < 0) || (pixel.y >= ctx->parent.height)) {
    return (Result){.code = -1};
  }
  color = LCD_rgb24_to_bgr565(color);

  set_address(ctx, pixel.x, pixel.y, pixel.x + 1, pixel.y + 1);

  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
  write_buffer(ctx, (uint8_t *)&color, 2);
  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, true, true);
  return (Result){.code = 0};
}

Result lcd_st7735_draw_vertical_line(St7735Context *ctx, LCD_Line line, uint32_t color) {
  // Rudimentary clipping
  if ((line.origin.x >= ctx->parent.width) || (line.origin.y >= ctx->parent.height)) {
    return (Result){.code = -1};
  }

  if ((line.origin.y + line.length - 1) >= ctx->parent.height) {
    line.length = ctx->parent.height - line.origin.y;
  }

  color = LCD_rgb24_to_bgr565(color);
  set_address(ctx, line.origin.x, line.origin.y, line.origin.x, line.origin.y + line.length - 1);

  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
  while (line.length--) {
    write_buffer(ctx, (uint8_t *)&color, 2);
  }
  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, true, true);
  return (Result){.code = 0};
}

Result lcd_st7735_draw_horizontal_line(St7735Context *ctx, LCD_Line line, uint32_t color) {
  // Rudimentary clipping
  if ((line.origin.x >= ctx->parent.width) || (line.origin.y >= ctx->parent.height)) {
    return (Result){.code = -1};
  }

  if ((line.origin.x + line.length - 1) >= ctx->parent.width) {
    line.length = ctx->parent.height - line.origin.y;
  }

  set_address(ctx, line.origin.x, line.origin.y, line.origin.x + line.length - 1, line.origin.y);

  color = LCD_rgb24_to_bgr565(color);

  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
  while (line.length--) {
    write_buffer(ctx, (uint8_t *)&color, 2);
  }
  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, true, true);
  return (Result){.code = 0};
}

Result lcd_st7735_fill_rectangle(St7735Context *ctx, LCD_rectangle rectangle, uint32_t color) {
  // rudimentary clipping (drawChar w/big text requires this)
  if ((rectangle.origin.x >= ctx->parent.width) || (rectangle.origin.y >= ctx->parent.height) ||
      (rectangle.origin.x + rectangle.width > ctx->parent.width) ||
      (rectangle.origin.y + rectangle.height > ctx->parent.height)) {
    return (Result){.code = -1};
  }

  uint16_t w = (uint16_t)(MIN(rectangle.origin.x + rectangle.width, ctx->parent.width) - rectangle.origin.x);
  uint16_t h = (uint16_t)(MIN(rectangle.origin.y + rectangle.height, ctx->parent.height) - rectangle.origin.y);

  color = LCD_rgb24_to_bgr565(color);

  // Create an array with the pixes for the lines.
  uint16_t row[w];
  for (int i = 0; i < w; ++i) {
    row[i] = (uint16_t)color;
  }

  set_address(ctx, rectangle.origin.x, rectangle.origin.y, rectangle.origin.x + w - 1, rectangle.origin.y + h - 1);

  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
  // Iterate through the lines.
  for (int x = h; x > 0; x--) {
    write_buffer(ctx, (uint8_t *)row, sizeof(row));
  }
  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, true, true);
  return (Result){.code = 0};
}

Result lcd_st7735_putchar(St7735Context *ctx, LCD_Point origin, char character) {
  const Font *font                    = ctx->parent.font;
  const FontCharInfo *char_descriptor = &font->descriptor_table[character - font->startCharacter];
  uint16_t buffer[char_descriptor->width];

  set_address(ctx, origin.x, origin.y, origin.x + char_descriptor->width - 1, origin.y + font->height - 1);
  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
  const uint8_t *char_bitmap = &font->bitmap_table[char_descriptor->position];
  for (int row = 0; row < font->height; row++) {
    for (int column = 0; column < char_descriptor->width; column++) {
      uint8_t bit = (uint8_t)(column % 8);
      char_bitmap += (uint8_t)(bit == 0);
      buffer[column] =
          (uint16_t)((char_bitmap[-1] & (0x01 << bit)) ? ctx->parent.foreground_color : ctx->parent.background_color);
    }
    write_buffer(ctx, (uint8_t *)buffer, sizeof(buffer));
  }
  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, true, true);
  return (Result){.code = 0};
}

Result lcd_st7735_puts(St7735Context *ctx, LCD_Point pos, const char *text) {
  size_t count = 0;

  while (*text) {
    uint32_t width = ctx->parent.font->descriptor_table[*text - ctx->parent.font->startCharacter].width;
    if ((pos.x + width) > ctx->parent.width) {
      return (Result){.code = 0};
    }

    lcd_st7735_putchar(ctx, pos, *text);

    pos.x = pos.x + width;

    text++;
    count++;
  }

  return (Result){.code = (int32_t)count};  // number of chars printed
}

Result lcd_st7735_draw_bgr(St7735Context *ctx, LCD_rectangle rectangle, const uint8_t *bgr) {
  set_address(ctx, rectangle.origin.x, rectangle.origin.y, rectangle.origin.x + rectangle.width - 1,
              rectangle.origin.y + rectangle.height - 1);

  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
  for (int i = 0; i < rectangle.width * rectangle.height * 3; i += 3) {
    uint16_t color = LCD_rgb24_to_bgr565((uint32_t)(bgr[i] << 16 | bgr[i + 1] << 8 | bgr[i + 2]));
    write_buffer(ctx, (uint8_t *)&color, 2);
  }
  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, true, true);
  return (Result){.code = 0};
}

Result lcd_st7735_draw_rgb565(St7735Context *ctx, LCD_rectangle rectangle, const uint8_t *rgb) {
  set_address(ctx, rectangle.origin.x, rectangle.origin.y, rectangle.origin.x + rectangle.width - 1,
              rectangle.origin.y + rectangle.height - 1);
  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
  for (int i = 0; i < rectangle.width * rectangle.height * 2; i += 2, rgb += 2) {
    uint16_t color = LCD_rgb565_to_bgr565(rgb);
    write_buffer(ctx, (uint8_t *)&color, 2);
  }
  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, true, true);
  return (Result){.code = 0};
}

Result lcd_st7735_rgb565_start(St7735Context *ctx, LCD_rectangle rectangle) {
  set_address(ctx, rectangle.origin.x, rectangle.origin.y, rectangle.origin.x + rectangle.width - 1,
              rectangle.origin.y + rectangle.height - 1);
  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
  return (Result){.code = 0};
}

Result lcd_st7735_rgb565_put(St7735Context *ctx, const uint8_t *rgb, size_t size) {
  for (int i = 0; i < size; i += 2, rgb += 2) {
    uint16_t color = LCD_rgb565_to_bgr565(rgb);
    write_buffer(ctx, (uint8_t *)&color, 2);
  }
  return (Result){.code = 0};
}

Result lcd_st7735_rgb565_finish(St7735Context *ctx) {
  ctx->parent.interface->gpio_write(ctx->parent.interface->handle, true, true);
  return (Result){.code = 0};
}

Result lcd_st7735_reset(St7735Context *ctx, bool hw) {
  if (hw && ctx->parent.interface->reset) {
    ctx->parent.interface->reset(ctx->parent.interface->handle);
  } else {
    write_command(ctx, ST7735_SWRESET);
    delay(ctx, 120);
  }
  return (Result){.code = 0};
}

Result lcd_st7735_close(St7735Context *ctx) { return (Result){.code = 0}; }

void lcd_st7735_set_frame_buffer_resolution(St7735Context *ctx, size_t width, size_t height) {
  size_t w = (width - ctx->parent.width) / 2;
  size_t h = (height - ctx->parent.height) / 2;
  if (ctx->parent.orientation == LCD_Rotate0 || ctx->parent.orientation == LCD_Rotate180) {
    ctx->col_offset = w;
    ctx->row_offset = h;
  } else {
    ctx->col_offset = h;
    ctx->row_offset = w;
  }
}

// Determine if an LCD offset of 2 pixels in the narrow dimension and
// 1 pixel in the wide dimension must be applied for correct function.
// Involves writing a bundle pixels to the LCD and reading some back
// from the start of the affected rows to discover the default width.
//
// The state of the GM[2:0] config pads of the ST7735 controller within
// the LCD is the root value we wish to discover, but can only do so
// by observing side-effects. This function infers GM state from the
// reset value the of CASET register, which itself must be inferred from
// pixel buffer behaviour after a reset as it cannot be read directly.
// The test used is whether the 129th pixel or the 133rd pixel written
// ends up at the start of the second row, as distinguished by writing
// different values after the 132nd. We check multiple rows to be sure.
// Wrapping at 128 infers CASET XE[7:0]=0x7F, which infers GM[2:0]='011'.
// Wrapping at 132 infers CASET XE[7:0]=0x83, which infers GM[2:0]='000'.
//
// GM[2:0]='000' (132x162) is incorrect for the 128x160 panel actually present,
// meaning minor coordinate offsets are needed. The offsets are 2 pixels
// in the narrow dimension (x if portrait) and 1 pixel in the wide dimension
// (y if portrait). This is due to how the ST7735 controller maps the
// contents of the internal frame buffer to display itself. See the
// (unfortunately error-ridden) ST7735 datasheet for more details.
//
// NOTE1: Must be run after a HW or SW reset and before any CASET commands.
// NOTE2: Does NOT always perform a reset before returning. State may be dirty.

Result lcd_st7735_check_frame_buffer_resolution(St7735Context *ctx, size_t *width, size_t *height) {
  enum {
    attempts = 3,
    buf_len  = 4,
  };

  uint8_t buf[buf_len];
  const uint8_t patterns[4] = {0xA8, 0xCC, 0xE0, 0x90};
  uint8_t result;

  if (ctx == NULL && height == NULL && width == NULL) {
    return (Result){.code = ErrorNullArgs};
  }

  if (ctx->parent.interface->spi_read == NULL) {
    return (Result){.code = ErrorNullCallback};
  }

  for (unsigned iter = 0; iter < attempts; iter++) {
    // Ensure CS line is de-asserted ahead of any commands
    ctx->parent.interface->gpio_write(ctx->parent.interface->handle, true, false);

    // Select 18-bit pixel format. Affects writes only (reads always 18-bit).
    // 18-bit pixel format (as per ST7735 datasheet):
    //
    //  MSB                                                                 LSB
    //  R5 R4 R3 R2 R1 R0 -- -- G5 G4 G3 G2 G1 G0 -- -- B5 B4 B3 B2 B1 B0 -- --
    // | First pixel byte      | Second pixel byte     | Last pixel byte       |
    //
    // Where "R5" is the first bit on the wire, and "--" bits are ignored.
    write_command(ctx, ST7735_COLMOD);
    ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
    uint8_t value = 0x06;
    write_buffer(ctx, &value, sizeof(value));

    // Write 4 lots (possibly lines) of 132 pixels into the frame buffer.
    // Change the value being written every 132 pixels.
    write_command(ctx, ST7735_RAMWR);
    ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
    for (unsigned l = 0u; l < sizeof(patterns); l++) {
      for (unsigned p = 0u; p < 132; p++) {
        // 18-bit pixel value packed into 24-bit (3 bytes) payload.
        // Two padding LSBs and 6 MSBs of real data per byte/channel.
        buf[0] = patterns[l];  // red
        buf[1] = patterns[l];  // green
        buf[2] = patterns[l];  // blue
        write_buffer(ctx, buf, 3);
      }
    }

    // Read back a pixel from the start of the second, third, and fourth lines
    // of the external frame buffer to determine whether the ST7735 controller
    // in the LCD assembly is configured for 128x160 or 132x162 (by GM pads).
    // Pixels are always read back in 18-bit format, regardless of COLMOD.
    result = 0;
    for (uint8_t l = 1u; l < sizeof(patterns); l++) {
      // Setting the addess in the frame buffer to start reading the pixels.
      buf[0] = l >> 8;
      buf[1] = l;
      buf[2] = 99 >> 8;
      buf[3] = 99;
      write_command(ctx, ST7735_RASET);
      ctx->parent.interface->gpio_write(ctx->parent.interface->handle, false, true);
      write_buffer(ctx, buf, 4);

      write_command(ctx, ST7735_RAMRD);
      // Read 1 dummy byte and 3 actual bytes (offset by a dummy clock cycle)
      ctx->parent.interface->spi_read(ctx->parent.interface->handle, buf, 4);
      ctx->parent.interface->gpio_write(ctx->parent.interface->handle, true, false);

      if (buf[1] == (patterns[l] >> 1) && buf[2] == (patterns[l] >> 1) && buf[3] == (patterns[l] >> 1)) {
        // Value read was that written for that line (shift adjusted for
        // dummy bit), so controller is configured for 132x162 mode (GM=000).
        result |= 1u << l;
      } else if (!(buf[1] == (patterns[l - 1] >> 1) && buf[2] == (patterns[l - 1] >> 1) &&
                   buf[3] == (patterns[l - 1] >> 1))) {
        // Unexpected value. Reset and retry, or give up and use default.
        result = 0xFFu;
        break;
      }
    }

    if (result == 0x00) {
      // Three out of three agree, 128-high it gladly be
      *width  = 160;
      *height = 128;
      return (Result){.code = ErrorOk};
    }
    if (result == 0x0e) {
      // Three out of three agree, 132-high it sadly be
      *width  = 162;
      *height = 132;
      return (Result){.code = ErrorOk};
    }

    // Software reset to restore most state to default - particularly CASET
    lcd_st7735_reset(ctx, /*hw=*/false);
  }

  // Ran out of attempts, use default (correct 128-wide)
  return (Result){.code = ErrorOperationFailed};
}
