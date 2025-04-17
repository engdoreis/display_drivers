# display_drivers
This driver prioritizes platform portability and ease of use. It achieves hardware independence
through application-provided callback functions, which handle all necessary hardware interactions.

Example:
```C
static uint32_t spi_write(void *handle, uint8_t *data, size_t len){
    //Code here.
    return len;
}

static uint32_t gpio_write(void *handle, bool cs, bool dc){
    //Code here.
    return 0;
}

static uint32_t reset(void *handle){
    //Code here.
    return 0;
}

static void set_pwm(void *handle, uint8_t pwm){
    //Code here.
}

static void sleep_ms(void *handle, uint32_t ms){
    //Code here.
}

void main(void){
    St7735Context ctx;
    LCD_Interface interface = {
        .handle = NULL,
        .spi_write = spi_write,
        .spi_read = NULL,
        .gpio_write = gpio_write,
        .reset = reset,
        .set_backlight_pwm = set_pwm,
        .timer_delay = sleep_ms,
    };

    lcd_st7735_init(&ctx, &interface);
    lcd_st7735_startup(&ctx);
    lcd_st7735_fill_rectangle(&ctx, (LCD_rectangle){.origin = {.x = 0, .y = 0},
        .end = {.x = 160, .y = 128}}, 0x00FF00);
}
```

## Configuration
### Endianess
The driver assumes the platform is little-endian by default. If your platform is big-endian, define the macro LCD_IS_LITTLE_ENDIAN as 0 before including header or in the build system.

## Detecting whether Offset is needed
For some cheap displays, the controller resolution may be configured to 132x162 pixels, which exceeds the panel's actual resolution of 128x160 pixels. This can be detected automatically using the function `lcd_st7735_check_offset`.

Note: The funciton  `lcd_st7735_check_offset` requires the callback `spi_read` to be implemented. 
```C
void main(void){
    St7735Context ctx;
    LCD_Interface interface = {
        .handle = NULL,
        .spi_write = spi_write,
        .spi_read = spi_read,
        .gpio_write = gpio_write,
        .reset = reset,
        .set_backlight_pwm = set_pwm,
        .timer_delay = sleep_ms,
    };

    lcd_st7735_init(&ctx, &interface);

    size_t w = 0, h = 0;
    Result res = lcd_st7735_check_frame_buffer_resolution(&lcd, &w, &h);
    if(res.code != 0){
        LOG( "Warning: Unable to determine LCD offset. Try slower SPI clock.\r\n");
    }else if(w != 160){
        LOG("Offset needed");
        lcd_st7735_set_frame_buffer_resolution(&lcd, w,h); 
    }else{
        LOG("No offset needed");
    }

    lcd_st7735_startup(&ctx);
    lcd_st7735_fill_rectangle(&ctx, (LCD_rectangle){.origin = {.x = 0, .y = 0},
            .end = {.x = 160, .y = 128}}, 0x00FF00);
}
```

