#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <string>

#include "../src/st7735/lcd_st7735.h"

void log_hex(std::ostream &stream, const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    stream << std::format("{:02x} ", static_cast<int>(data[i]));
  }
  stream << std::endl;
}

struct MockInterfaceFile {
  std::string filename_;

  MockInterfaceFile() {
    char out_filename[1024];
    filename_ = std::string(std::tmpnam(out_filename));
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

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
