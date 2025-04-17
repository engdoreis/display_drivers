
// Copyright (c) 2022 Douglas Reis.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#ifndef DISPLAY_DRIVERS_COMMON_BASE_H_
#define DISPLAY_DRIVERS_COMMON_BASE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "font.h"

#ifndef LCD_IS_LITTLE_ENDIAN
// TODO: define a way to check endianess in multiple platforms.
#define LCD_IS_LITTLE_ENDIAN 1
#endif

#if LCD_IS_LITTLE_ENDIAN
#define ENDIANESS_TO_HALF_WORD(_x) (uint16_t)((_x >> 8) | (_x << 8))
#else
#define ENDIANESS_TO_HALF_WORD(_x) (uint16_t)(_x)
#endif

#ifndef MIN
#define MIN(_A, _B) _A > _B ? _B : _A
#endif

#ifndef MAX
#define MAX(_A, _B) _A < _B ? _B : _A
#endif

#ifndef SWAP
#define SWAP(_A, _B, TYPE)                                                                                             \
  do {                                                                                                                 \
    TYPE temp = (_A);                                                                                                  \
    (_A)      = (_B);                                                                                                  \
    (_B)      = temp;                                                                                                  \
  } while (false)
#endif

typedef enum {
  LCD_Rotate0 = 0,
  LCD_Rotate90,
  LCD_Rotate180,
  LCD_Rotate270,
} LCD_Orientation;

typedef enum ErrorCode_e {
  ErrorOk              = 0,
  ErrorNullArgs        = -1,
  ErrorNullCallback    = -2,
  ErrorOperationFailed = -3,
} ErrorCode;

typedef struct Result_st {
  int32_t code; /*!< See #ErrorCode */
} Result;

/**
 * @brief Struct with the callbacks needed by the display driver to access the hardware.
 */
typedef struct LCD_Interface_st {
  void *handle; /*!< Pointer that will passed to the callbacks calls. It is reserved for exclusive use of the
                   application. If not intended to be used it can be `NULL` */

  /**
   * @brief Write bytes to the spi bus.
   *
   * @param data Pointer to data array to be sent.
   * @param len Length of the data to be sent.
   *
   * @return the number of bytes sent.
   */
  uint32_t (*spi_write)(void *handle, uint8_t *data, size_t len);

  /**
   * @brief Clock the spi and read the data.
   *
   * This function is only used in more complex operations, for basic use it can be NULL.
   *
   * @param data Pointer to array to store the data read.
   * @param len Length of the data to be sent.
   *
   * @return the number of bytes read.
   */
  uint32_t (*spi_read)(void *handle, uint8_t *data, size_t len);

  /**
   * @brief Set the state of the chip select and D/C pins.
   *
   * @param cs_high Set chip select pin to 1 if true, otherwise 0.
   * @param dc_high Set D/C pin to 1 if true, otherwise 0.
   *
   * @return 0 if ok.
   */
  uint32_t (*gpio_write)(void *handle, bool cs_high, bool dc_high);

  /**
   * @brief Apply a hardware reset to the display by toggling the pin.
   */
  uint32_t (*reset)(void *handle);

  /** @brief Apply a hardware reset to the display by toggling the pin. @param pwm Set the pwm (0-100).
   *
   * Note: Future use only.
   */
  void (*set_backlight_pwm)(void *handle, uint8_t pwm);

  /**
   * @brief Simple cpu delay
   *
   * @param millis Time the delay should take in milliseconds.
   */
  void (*timer_delay)(void *handle, uint32_t millis);
} LCD_Interface;

typedef struct LCD_Context_st {
  LCD_Interface *interface;  /*!< Callbacks to access the hardware.*/
  uint32_t width;            /*!< Width of the display in pixels*/
  uint32_t height;           /*!< Height of the display in pixels*/
  const Font *font;          /*!< Font bitmaps*/
  uint32_t background_color; /*< Background color for font bitmaps */
  uint32_t foreground_color; /*< Foreground color for font bitmaps */
  LCD_Orientation orientation;
} LCD_Context;

typedef struct LCD_Point_st {
  uint32_t x; /*!< X coordinate.*/
  uint32_t y; /*!< y coordinate.*/
} LCD_Point;

typedef struct LCD_Line_st {
  LCD_Point origin; /*!< Coordinates of the origin.*/
  size_t length;    /*!< Length from the origin.*/
} LCD_Line;

typedef struct LCD_rectangle_st {
  LCD_Point origin; /*!< Coordinates of the origin.*/
  size_t width;     /*!< Width from the origin.*/
  size_t height;    /*!< Height from the origin.*/
} LCD_rectangle;

Result LCD_Init(LCD_Context *ctx, LCD_Interface *interface, uint32_t width, uint32_t height,
                LCD_Orientation orientation);

static inline Result LCD_get_resolution(LCD_Context *ctx, size_t *height, size_t *width) {
  *height = ctx->height;
  *width  = ctx->width;
  return (Result){.code = 0};
}

static inline Result LCD_set_font_colors(LCD_Context *ctx, uint32_t background_color, uint32_t foreground_color) {
  ctx->background_color = background_color;
  ctx->foreground_color = foreground_color;
  return (Result){.code = 0};
}

static inline Result LCD_set_font(LCD_Context *ctx, const Font *font) {
  ctx->font = font;
  LCD_set_font_colors(ctx, 0xffffff, 0x00);
  return (Result){.code = 0};
}

static inline Result LCD_get_font_size(LCD_Context *ctx, size_t *width, size_t *height) {
  if (ctx->font == NULL) {
    return (Result){.code = -1};
  }

  *height = ctx->font->height;
  *width  = ctx->font->descriptor_table->width;
  return (Result){.code = 0};
}

static inline uint16_t LCD_rgb24_to_bgr565(uint32_t rgb) {
  uint8_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
  uint16_t color = (uint16_t)(((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3));
  return ENDIANESS_TO_HALF_WORD(color);
}

static inline uint16_t LCD_rgb565_to_bgr565(const uint8_t rgb[2]) {
  // |        B0              |           B1           |
  // | r  r  r  r  r  g  g  g |  g  g  g  b  b  b  b  b|
  // | 0              5     7 |  0        3           7|

  // |                half word                       |
  // |b  b  b  b  b  g  g  g  g  g  g  r  r  r  r  r  |5f0d
  // |15          11                5              0  |
  uint8_t b = (uint8_t)(rgb[1] >> 3), g = (uint8_t)((rgb[1] & 0x7) << 3 | rgb[0] >> 5), r = (uint8_t)(rgb[0] & 0x1F);
  uint16_t color = (uint16_t)(b | g << 5 | r << 11);
  return ENDIANESS_TO_HALF_WORD(color);
}

#endif
