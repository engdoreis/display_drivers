#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <string>

#include "../src/st7735/lcd_st7735.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

constexpr size_t DisplayWidth  = 160;
constexpr size_t DisplayHeight = 128;

void log_hex(std::ostream &stream, const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    stream << std::format("{:02x} ", static_cast<int>(data[i]));
  }
  stream << std::endl;
}

struct MockInterfaceFile {
  std::string filename_;

  MockInterfaceFile() {
    char buffer[1024];
    filename_ = std::string(std::tmpnam(buffer));
  };

  static uint32_t spi_write(void *handle, uint8_t *data, size_t len) {
    MockInterfaceFile *self = (MockInterfaceFile *)handle;
    std::ofstream file(self->filename_, std::ios::app);
    file << std::format("{}: ", __func__);
    log_hex(file, data, len);
    return len;
  }

  static uint32_t gpio_write(void *handle, bool cs, bool dc) {
    MockInterfaceFile *self = (MockInterfaceFile *)handle;
    std::ofstream file(self->filename_, std::ios::app);
    file << std::format("{}: cs={}, dc={}", __func__, cs, dc) << std::endl;
    return 0;
  }

  static uint32_t reset(void *handle) {
    MockInterfaceFile *self = (MockInterfaceFile *)handle;
    std::ofstream file(self->filename_, std::ios::app);
    file << std::format("{}", __func__) << std::endl;
    return 0;
  }

  static void set_pwm(void *handle, uint8_t pwm) {
    MockInterfaceFile *self = (MockInterfaceFile *)handle;
    std::ofstream file(self->filename_, std::ios::app);
    file << std::format("{}: {}", __func__, pwm) << std::endl;
  }

  static void sleep_ms(void *handle, uint32_t ms) {
    MockInterfaceFile *self = (MockInterfaceFile *)handle;
    std::ofstream file(self->filename_, std::ios::app);
    file << std::format("{}: {}", __func__, ms) << std::endl;
  }
};

class DisplayTest : public testing::Test {
 public:
#define GET_GOLDEN_FILE()                                                                                              \
  std::format("./tests/golden_files/test_{}.png", testing::UnitTest::GetInstance()->current_test_info()->name())

  template<typename T>
  T rotate_left(T& v) {
    v = v << 8 | v >> (32 - 8); 
    return v;
  }
  std::string make_temp_filename() {
    char buffer[1024];
    return std::string(std::tmpnam(buffer));
  }

  void compare_img(const std::string &result_img, const std::string &expected_img) {
    int w1, h1, c1, w2, h2, c2;

    unsigned char *result   = stbi_load(result_img.c_str(), &w1, &h1, &c1, 0);
    unsigned char *expected = stbi_load(expected_img.c_str(), &w2, &h2, &c2, 0);

    ASSERT_NE(expected, nullptr) << std::format("File {} not found, to compare with {}", expected_img, result_img);
    ASSERT_NE(result, nullptr) << std::format("File {} not found, to compare with {}", result_img, expected_img);
    ASSERT_TRUE(w1 == w2 && h1 == h2 && c1 == c2);

    size_t size = w1 * h1 * c1;
    ASSERT_TRUE(std::memcmp(result, expected, size) == 0)
        << std::format("Mismatch {} != {}\n", result_img, expected_img)
        << std::format("Run: compare {} {} /tmp/diff.png", result_img, expected_img);

    stbi_image_free(result);
    stbi_image_free(expected);
  }

  void compare_files(const std::string &result_file, const std::string &expected_file) {
    std::ifstream f1(result_file), f2(expected_file);
    std::string result, expected;
    int lineNum = 1;

    while (std::getline(f1, result) && std::getline(f2, expected)) {
      EXPECT_EQ(result, expected) << std::format("File mismatch {} != {} at line{}", result_file, expected_file,
                                                 lineNum);
      ++lineNum;
    }
  }
};

class st7735Test : public DisplayTest {
 public:
  MockInterfaceFile mock_ = MockInterfaceFile();
  St7735Context ctx_;
  LCD_Interface interface_;

  st7735Test() {
    interface_ = {
        .handle            = &mock_,
        .spi_write         = MockInterfaceFile::spi_write,
        .spi_read          = NULL,
        .gpio_write        = MockInterfaceFile::gpio_write,
        .reset             = MockInterfaceFile::reset,
        .set_backlight_pwm = MockInterfaceFile::set_pwm,
        .timer_delay       = MockInterfaceFile::sleep_ms,
    };
    lcd_st7735_init(&ctx_, &interface_);
  }
};

TEST_F(st7735Test, startup) {
  Result res = lcd_st7735_startup(&ctx_);
  EXPECT_EQ(res.code, 0);
  compare_files(mock_.filename_, "./golden_files/st7735_startup.txt");
}

#include <simulator/st7735/controller.hh>
struct MockInterfaceSimulator {
  Simulator::St7735<DisplayWidth, DisplayHeight> simulator;

  MockInterfaceSimulator() {};

  static uint32_t spi_write(void *handle, uint8_t *data, size_t len) {
    MockInterfaceSimulator *self = (MockInterfaceSimulator *)handle;
    self->simulator.spi_write(data, len);
    return len;
  }

  static uint32_t gpio_write(void *handle, bool cs, bool dc) {
    MockInterfaceSimulator *self = (MockInterfaceSimulator *)handle;
    self->simulator.dc_pin(dc ? Simulator::PinLevel::High : Simulator::PinLevel::Low);
    self->simulator.cs_pin(cs ? Simulator::PinLevel::High : Simulator::PinLevel::Low);
    return 0;
  }

  static uint32_t reset(void *handle) {
    MockInterfaceSimulator *self = (MockInterfaceSimulator *)handle;
    return 0;
  }

  static void set_pwm(void *handle, uint8_t pwm) { MockInterfaceSimulator *self = (MockInterfaceSimulator *)handle; }

  static void sleep_ms(void *handle, uint32_t ms) { MockInterfaceSimulator *self = (MockInterfaceSimulator *)handle; }
};

class st7735SimTest : public DisplayTest {
 public:
  MockInterfaceSimulator mock_ = MockInterfaceSimulator();
  St7735Context ctx_;
  LCD_Interface interface_;

  st7735SimTest() {
    interface_ = {
        .handle            = &mock_,
        .spi_write         = MockInterfaceSimulator::spi_write,
        .spi_read          = NULL,
        .gpio_write        = MockInterfaceSimulator::gpio_write,
        .reset             = MockInterfaceSimulator::reset,
        .set_backlight_pwm = MockInterfaceSimulator::set_pwm,
        .timer_delay       = MockInterfaceSimulator::sleep_ms,
    };
    lcd_st7735_init(&ctx_, &interface_);
    lcd_st7735_startup(&ctx_);
  }
};

TEST_F(st7735SimTest, draw_rectangles) {
  Result res = lcd_st7735_clean(&ctx_);
  EXPECT_EQ(res.code, 0);
  size_t increment = 20;
  uint32_t rgb = 0x0000FF;

  {
    LCD_rectangle rec{.origin = {.x = 0, .y = 0}, .width = DisplayWidth, .height = 10};
    auto draw_horizontal = [&](uint32_t color) {
      rec.origin.y += increment;
      res = lcd_st7735_fill_rectangle(&ctx_, rec, color);
      EXPECT_EQ(res.code, 0);
    };

    draw_horizontal(rotate_left(rgb));
    draw_horizontal(rotate_left(rgb));
    draw_horizontal(rotate_left(rgb));
    draw_horizontal(rotate_left(rgb));
    draw_horizontal(rotate_left(rgb));
  }
  {
    LCD_rectangle rec{.origin = {.x = 10, .y = 0}, .width = 10, .height = DisplayHeight};
    auto draw_vertical = [&](uint32_t color) {
      rec.origin.x += increment;
      res = lcd_st7735_fill_rectangle(&ctx_, rec, color);
      EXPECT_EQ(res.code, 0);
    };

    draw_vertical(rotate_left(rgb));
    draw_vertical(rotate_left(rgb));
    draw_vertical(rotate_left(rgb));
    draw_vertical(rotate_left(rgb));
    draw_vertical(rotate_left(rgb));
    draw_vertical(rotate_left(rgb));

    EXPECT_EQ(res.code, 0);
  }

  std::string filename = make_temp_filename();
  mock_.simulator.png(filename);
  compare_img(filename, GET_GOLDEN_FILE());
}

#include <src/core/lucida_console_10pt.h>
#include <src/core/lucida_console_12pt.h>
TEST_F(st7735SimTest, draw_text) {
  Result res = lcd_st7735_clean(&ctx_);
  EXPECT_EQ(res.code, 0);

  std::string ascii = "";
  for (int i = 32; i <= 126; ++i) {
    if (std::isprint(static_cast<unsigned char>(i))) {
      ascii += static_cast<char>(i);
    }
  }
  size_t font_h(0), rows(0), columns(0), ascii_index(0);
  uint32_t bg(0xff), fg(0x00);
  LCD_Point pos{.x = 0, .y = 0};

  auto set_font = [&](const Font *font) {
    res = lcd_st7735_set_font(&ctx_, font);
    EXPECT_EQ(res.code, 0);
    font_h      = ctx_.parent.font->height;
    rows        = DisplayHeight / ctx_.parent.font->height;
    columns     = DisplayWidth / ctx_.parent.font->descriptor_table->width;
    ascii_index = 0;
  };

  auto draw_text = [&]() {
    res = lcd_st7735_set_font_colors(&ctx_, bg, fg);
    EXPECT_EQ(res.code, 0);
    std::string print = ascii.substr(ascii_index, columns);
    res               = lcd_st7735_puts(&ctx_, pos, print.c_str());
    EXPECT_EQ(res.code, print.size());
    ascii_index += columns;
    pos.y += font_h;
    bg = bg << 8 | bg >> (32 - 8);  // Rotate left
    fg = bg ^ 0xffffff;
  };

  set_font(&lucidaConsole_12ptFont);
  do {
    draw_text();
  } while (ascii_index < ascii.size());

  set_font(&lucidaConsole_10ptFont);
  do {
    draw_text();
  } while (pos.y < DisplayHeight - font_h);

  std::string filename = make_temp_filename();
  mock_.simulator.png(filename);
  compare_img(filename, GET_GOLDEN_FILE());
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
