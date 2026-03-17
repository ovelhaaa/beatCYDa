#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "../CYD_Config.h"

class LGFX_CYD : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;
  lgfx::Touch_XPT2046 _touch_instance;

public:
  LGFX_CYD(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = VSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = CYDConfig::TftSclk;
      cfg.pin_mosi = CYDConfig::TftMosi;
      cfg.pin_miso = CYDConfig::TftMiso;
      cfg.pin_dc = CYDConfig::TftDc;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = CYDConfig::TftCs;
      cfg.pin_rst = CYDConfig::TftRst;
      cfg.pin_busy = -1;
      cfg.panel_width = CYDConfig::ScreenHeight;
      cfg.panel_height = CYDConfig::ScreenWidth;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 16;
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = CYDConfig::UseSharedSpiWiring;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = CYDConfig::TftBacklight;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }
    {
      auto cfg = _touch_instance.config();
      cfg.x_min = CYDConfig::TouchMinX;
      cfg.x_max = CYDConfig::TouchMaxX;
      cfg.y_min = CYDConfig::TouchMinY;
      cfg.y_max = CYDConfig::TouchMaxY;
      cfg.pin_int = CYDConfig::TouchIrq;
      cfg.bus_shared = CYDConfig::UseSharedSpiWiring;
      cfg.offset_rotation = 2; // Fixed touch orientation
      cfg.spi_host = -1; // Use software SPI bit-bang to avoid conflicts when SD card is added
      cfg.freq = 1000000;
      cfg.pin_sclk = CYDConfig::TouchSclk;
      cfg.pin_mosi = CYDConfig::TouchMosi;
      cfg.pin_miso = CYDConfig::TouchMiso;
      cfg.pin_cs = CYDConfig::TouchCs;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

extern LGFX_CYD tft;
