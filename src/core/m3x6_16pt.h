// Copyright (c) 2024 Gary Guo.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// This is a rasterized version of the m3x6 font.
// The m3x6 font is created by Daniel Linssen and is free to use with attribution.
// The original TTF font can be found at https://managore.itch.io/m3x6

#ifndef M3X6_16PT_H_
#define M3X6_16PT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "font.h"

// Font data for Lucida Console 16pt
extern const unsigned char m3x6_16ptBitmaps[];
extern const Font m3x6_16ptFont;
extern const FontCharInfo m3x6_16ptDescriptors[];

#ifdef __cplusplus
}
#endif

#endif /* M3X6_16PT_H_ */
