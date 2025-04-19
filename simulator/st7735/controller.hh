
#include <src/st7735/lcd_st7735.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>
#include <string>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#ifdef SIMULATOR_LOGGING
#include <iostream>
#define LOG(msg) std::cout << msg
#else
#define LOG(msg)                                                                                                       \
  do {                                                                                                                 \
  } while (0)
#endif

namespace Simulator {

// Forwward declaration
template <size_t width, size_t height>
class St7735;

template <size_t width, size_t height>
class State {
 public:
  virtual ~State()                                                              = default;
  virtual void handle(St7735<width, height>& sim, std::vector<uint8_t>& buffer) = 0;
};

template <size_t width, size_t height>
class CommandState : public State<width, height> {
 public:
  void handle(St7735<width, height>& sim, std::vector<uint8_t>& buffer) override { sim.parse_commands(buffer); }
};

template <size_t width, size_t height>
class CasetState : public State<width, height> {
 public:
  void handle(St7735<width, height>& sim, std::vector<uint8_t>& buffer) override { sim.parse_caset(buffer); }
};

template <size_t width, size_t height>
class RasetState : public State<width, height> {
 public:
  void handle(St7735<width, height>& sim, std::vector<uint8_t>& buffer) override { sim.parse_raset(buffer); }
};

template <size_t width, size_t height>
class RamWriteState : public State<width, height> {
 public:
  void handle(St7735<width, height>& sim, std::vector<uint8_t>& buffer) override { sim.ram_write(buffer); }
};

enum class PinLevel {
  Low  = 0,
  High = 1,
};

union __attribute__((packed)) Pixel {
  struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  } rgb;
  uint8_t buffer[3];
};

struct Cursor {
  size_t col_start, col_end, row_start, row_end, row, col;
  void operator++(int) {
    if (++col > col_end) {
      col = col_start;
      if (row++ > row_end) {
        row = row_start;
      }
    }
  }
};

template <size_t width, size_t height>
class St7735 {
  State<width, height>* state = new CommandState<width, height>();

  std::array<std::array<Pixel, width>, height> frame_buffer;
  Cursor cursor;

  PinLevel dc_pin_ = PinLevel::High;
  PinLevel cs_pin_ = PinLevel::High;
  LCD_Orientation orientation_;

 public:
  St7735() {}

  void set_state(State<width, height>* new_state) { state = new_state; }
  void update(std::vector<uint8_t>& data) { state->handle(*this, data); }

  void spi_write(uint8_t* data, size_t len) {
    std::vector<uint8_t> vec(data, data + len);
    update(vec);
  }

  void parse_commands(std::vector<uint8_t>& buffer) {
    LOG(std::format("{}: ", __func__));
    uint8_t cmd = buffer[0];
    switch (cmd) {
      break;
      case ST7735_CASET:
        LOG(std::format("CASET: "));
        this->set_state(new CasetState<width, height>());
        break;
      case ST7735_RASET:
        LOG(std::format("RASET: "));
        this->set_state(new RasetState<width, height>());
        break;
      case ST7735_RAMWR:
        LOG(std::format("RAMWR:\n"));
        this->set_state(new RamWriteState<width, height>());
        break;
      case ST7735_NOP:
      case ST7735_SWRESET:
      case ST7735_RDDID:
      case ST7735_RDDST:
      case ST7735_SLPIN:
      case ST7735_SLPOUT:
      case ST7735_PTLON:
      case ST7735_NORON:
      case ST7735_INVOFF:
      case ST7735_INVON:
      case ST7735_DISPOFF:
      case ST7735_DISPON:
      case ST7735_PTLAR:
      case ST7735_COLMOD:
      case ST7735_MADCTL:
      case ST7735_FRMCTR1:
      case ST7735_FRMCTR2:
      case ST7735_FRMCTR3:
      case ST7735_INVCTR:
      case ST7735_DISSET5:
      case ST7735_PWCTR1:
      case ST7735_PWCTR2:
      case ST7735_PWCTR3:
      case ST7735_PWCTR4:
      case ST7735_PWCTR5:
      case ST7735_VMCTR1:
      case ST7735_RDID1:
      case ST7735_RDID2:
      case ST7735_RDID3:
      case ST7735_RDID4:
      case ST7735_PWCTR6:
      case ST7735_GMCTRP1:
      case ST7735_GMCTRN1:
      case ST7735_RAMRD:
      default:
        LOG(std::format("cmd[{:#02x}] unimp: \n", cmd));
        break;
    }
  }

  void parse_caset(std::vector<uint8_t>& buffer) {
    cursor.col = cursor.col_start = buffer[0] << 8 | buffer[1];
    cursor.col_end                = buffer[2] << 8 | buffer[3];
    LOG(std::format("x: {},y:{} \n", col_addr_s_, col_addr_e_));
  }

  void parse_raset(std::vector<uint8_t>& buffer) {
    cursor.row = cursor.row_start = buffer[0] << 8 | buffer[1];
    cursor.row_end                = buffer[2] << 8 | buffer[3];
    LOG(std::format("x: {},y:{} \n", row_addr_s_, row_addr_e_));
  }

  void ram_write(std::vector<uint8_t>& buffer) {
    for (size_t i = 0; i < buffer.size() - 1; i += 2) {
      uint16_t bgr565 = buffer[i] << 8 | buffer[i + 1];
      Pixel pixel    = {.rgb = {.r = static_cast<uint8_t>((((bgr565 >> 0) & 0x1f) << 3) | 0x7),
                                .g = static_cast<uint8_t>((((bgr565 >> 5) & 0x3f) << 2) | 0x3),
                                .b = static_cast<uint8_t>((((bgr565 >> 11) & 0x1f) << 3) | 0x7)}};

      frame_buffer[cursor.row][cursor.col] = pixel;
      cursor++;
    }
  }

  void render() {
    for (auto& row : frame_buffer) {
      LOG(std::format("{{"));
      for (auto& pixel : row) {
        std::cout << std::format("{:02x}{:02x}{:02x},", pixel.rgb.r, pixel.rgb.g, pixel.rgb.b);
      }
      std::cout << std::format("}}\n");
    }
  }

  void bmp(std::string filename) {
    unsigned char bpm[width * height * 3];

    size_t i = 0;
    for (auto& row : frame_buffer) {
      for (auto& pixel : row) {
        bpm[i++] = pixel.rgb.r;
        bpm[i++] = pixel.rgb.g;
        bpm[i++] = pixel.rgb.b;
      }
    }
    stbi_write_bmp(filename.c_str(), width, height, 3, bpm);
  }

  void png(std::string filename) {
    unsigned char png[width * height * 3];

    size_t i = 0;
    for (auto& row : frame_buffer) {
      for (auto& pixel : row) {
        png[i++] = pixel.rgb.r;
        png[i++] = pixel.rgb.g;
        png[i++] = pixel.rgb.b;
      }
    }
    stbi_write_png(filename.c_str(), width, height, 3, png, 3 * width);
  }

  void dc_pin(PinLevel level) {
    if (level == PinLevel::Low) {
      this->set_state(new CommandState<width, height>());
    }
    dc_pin_ = level;
  }

  void cs_pin(PinLevel level) { cs_pin_ = level; }
};

}  // namespakce Simulator
