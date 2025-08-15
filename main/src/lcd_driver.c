//======================================================================================================================
/// @file       lcd_driver.c
/// @brief      LCD driver (ST7789)
/// @date       2025/8/13
//======================================================================================================================

#include "lcd_driver.h"
#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_camera.h"
#include "esp_check.h"
#include "esp_jpg_decode.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

//======================================================================================================================
// Constant definition
//======================================================================================================================
// LCD pin configuration
#define LCD_HOST SPI2_HOST
#define PIN_NUM_MOSI 32  // ST7789 SDA
#define PIN_NUM_CLK 33   // ST7789 SCL
#define PIN_NUM_DC 0    // ST7789 DC
#define PIN_NUM_CS -1    // Not use
#define PIN_NUM_RST 13   // ST7789 RES (Not use HW reset)
#define PIN_NUM_BL -1    // ST7789 BLK (Backlight)

// LCD parameters
#define LCD_WIDTH 240
#define LCD_HEIGHT 240
#define LCD_SOFT_BYTE_SWAP 0
#define LCD_COLOR_SPACE_BGR 1
#define LCD_Y_GAP 0
#define LCD_PCLK_HZ (20 * 1000 * 1000)
#define COLOR_BITS 16
#define SWAP_RB_AFTER_JPG 0

//======================================================================================================================
// Prototype
//======================================================================================================================
static esp_err_t lcd_test_fill(uint16_t color565);
static inline uint16_t swap16(uint16_t v);
static size_t jpg_mem_reader(void* arg, size_t index, uint8_t* buf, size_t len);
static bool jpg_write_cb(void* arg, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t* data);
static inline uint16_t convert_to_bgr565(uint16_t rgb565);
static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);

static void lcd_test_rgb565_bit_pattern(void);
static esp_err_t lcd_test_swap_pattern(void);

//======================================================================================================================
// Private values definition
//======================================================================================================================

static const char* TAG = "LCD";
static esp_lcd_panel_handle_t panel_handle = NULL;

static uint16_t lcd_buffer[LCD_WIDTH * LCD_HEIGHT];

// ---- JPEG decode writer context ----
typedef struct {
  uint16_t* buf;    // destination RGB565 buffer
  uint16_t stride;  // line stride in pixels (e.g., 320 for QVGA)
} jpg_out_t;

// ---- JPEG input (memory) reader ----
typedef struct {
  const uint8_t* p;  // pointer to JPEG data
  size_t len;
} jpg_src_t;

typedef struct {
  // input (JPEG in memory)
  const uint8_t* jpg;
  size_t jpg_len;
  uint16_t* dst;    // must be non-NULL
  uint16_t stride;  // e.g., 320
  uint16_t out_w;   // expected full width (e.g., 320)
  uint16_t out_h;   // expected full height (e.g., 240)
} jpg_ctx_t;

//======================================================================================================================
// Public
//======================================================================================================================
esp_err_t lcd_display_init(void) {
  ESP_LOGI(TAG, "Initializing LCD");

  if (panel_handle) {
    ESP_LOGW(TAG, "LCD already initialized");
    return ESP_OK;
  }

  // 1) SPI bus (no MISO)
  spi_bus_config_t buscfg = {
      .mosi_io_num = PIN_NUM_MOSI,
      .miso_io_num = -1,
      .sclk_io_num = PIN_NUM_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2,  // full-frame RGB565
  };
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

  // 2) Panel IO (SPI mode 3 is required on some CS-less ST7789 boards)
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_spi_config_t io_config = {0};
  io_config.dc_gpio_num = PIN_NUM_DC;
  io_config.cs_gpio_num = PIN_NUM_CS;  // -1 for no CS
  io_config.pclk_hz = LCD_PCLK_HZ;
  io_config.lcd_cmd_bits = 8;
  io_config.lcd_param_bits = 8;
  io_config.spi_mode = 3;  // try 3 first; 0 on some boards
  io_config.trans_queue_depth = 10;
  io_config.flags.lsb_first = 0;
  // NOTE: Your IDF struct has no flags.swap_color_bytes; keep software swap enabled.

  ESP_ERROR_CHECK(
      esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

  // 3) Panel device (ST7789)
  esp_lcd_panel_dev_config_t panel_config = {
    .reset_gpio_num = PIN_NUM_RST,  // active-low
    .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
    // .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
    .bits_per_pixel = 16,
#if LCD_COLOR_SPACE_BGR
    // .color_space = LCD_RGB_ELEMENT_ORDER_BGR,
    .rgb_endian = LCD_RGB_ENDIAN_BGR,
  // .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
#else
    // .color_space = LCD_RGB_ELEMENT_ORDER_RGB,
    .rgb_endian = LCD_RGB_ENDIAN_RGB,
  // .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
#endif
    .flags = {.reset_active_high = false},
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

  // 4) Reset / init / gap / display on
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

  ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, LCD_Y_GAP));
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

  // 5) Backlight ON (simple GPIO; move to LEDC later if PWM needed)
  if (PIN_NUM_BL >= 0) {
    gpio_config_t bl_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BL,
    };
    ESP_ERROR_CHECK(gpio_config(&bl_cfg));
    gpio_set_level(PIN_NUM_BL, 1);
  }

  ESP_LOGI(TAG, "LCD initialization completed");

  // // Quick sanity sweep: pure fills via draw_bitmap path
  // ESP_LOGI(TAG, "LCD fill test (R, G, B)...");
  // // lcd_test_fill(0xF800); // yellow
  // lcd_test_fill(0xFFE0);  // red
  // vTaskDelay(pdMS_TO_TICKS(1000));
  // lcd_test_fill(0x07E0);
  // vTaskDelay(pdMS_TO_TICKS(1000));
  // lcd_test_fill(0x001F);
  // vTaskDelay(pdMS_TO_TICKS(1000));

  // ESP_LOGI(TAG, "Test pattern");
  // lcd_test_all_patterns();

  // lcd_test_swap_pattern();
  // vTaskDelay(pdMS_TO_TICKS(60000));

  // ESP_LOGI(TAG, "Bit pattern");
  // lcd_test_rgb565_bit_pattern();

  return ESP_OK;
}

static inline uint16_t swap_rb_565(uint16_t c) {
  return (uint16_t)((c & 0x07E0) | ((c & 0x001F) << 11) | ((c & 0xF800) >> 11));
}

esp_err_t lcd_display_show(camera_fb_t* frame) {
  if (!frame || !panel_handle) return ESP_ERR_INVALID_ARG;

  const uint16_t cam_w = 320, cam_h = 240;
  const uint16_t lcd_w = LCD_WIDTH, lcd_h = LCD_HEIGHT;

  if (frame->format != PIXFORMAT_JPEG) {
    ESP_LOGE(TAG, "Unsupported frame format (expect PIXFORMAT_JPEG)");
    return ESP_ERR_INVALID_ARG;
  }

  // 1) PSRAM に 320x240 の RGB565 ワーク領域
  uint16_t* psram_frame = heap_caps_malloc((size_t)cam_w * cam_h * sizeof(uint16_t),
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!psram_frame) {
    ESP_LOGE(TAG, "Failed to allocate PSRAM frame");
    return ESP_ERR_NO_MEM;
  }

  // 2) JPEG→RGB565 に“必ず”変換（コールバック不要）
  //    out は 16bpp 固定で cam_w*cam_h*2 バイト連続
  bool ok = jpg2rgb565(frame->buf, frame->len, (uint8_t*)psram_frame, JPG_SCALE_NONE);
  if (!ok) {
    ESP_LOGE(TAG, "jpg2rgb565 failed");
    free(psram_frame);
    return ESP_FAIL;
  }

#if SWAP_RB_AFTER_JPG
  // 必要に応じて赤青入替（全画素一括）
  for (size_t i = 0; i < (size_t)cam_w * cam_h; ++i) {
    psram_frame[i] = swap_rb_565(psram_frame[i]);
  }
#endif

  // 3) 送信用ストリップ（内部RAM, DMA）
  enum { STRIP_H = 20 };
  static uint16_t* strip = NULL;
  if (!strip) {
    strip = heap_caps_malloc((size_t)lcd_w * STRIP_H * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!strip) {
      ESP_LOGE(TAG, "Failed to allocate DMA strip buffer");
      free(psram_frame);
      return ESP_ERR_NO_MEM;
    }
  }

  // 4) 中央240幅にトリムして分割描画
  const uint16_t trim_x = (cam_w - lcd_w) / 2;  // 320→240 なので 40
  esp_err_t ret = ESP_OK;

  for (uint16_t y = 0; y < lcd_h; y += STRIP_H) {
    uint16_t h = (y + STRIP_H <= lcd_h) ? STRIP_H : (lcd_h - y);

    // psram_frame から 1 ラインずつ 240px を strip にコピー
    for (uint16_t j = 0; j < h; ++j) {
      const uint16_t* src = &psram_frame[(y + j) * cam_w + trim_x];
      uint16_t* dst = &strip[j * lcd_w];
      memcpy(dst, src, (size_t)lcd_w * sizeof(uint16_t));
    }

    ret = esp_lcd_panel_draw_bitmap(panel_handle, 0, y, lcd_w, (uint16_t)(y + h), strip);
    if (ret != ESP_OK) break;
  }

  free(psram_frame);
  return ret;
}

//======================================================================================================================
// Private
//======================================================================================================================

// Fill whole screen with a solid RGB565 color via draw_bitmap path
static esp_err_t lcd_test_fill(uint16_t color565) {
  const uint16_t w = LCD_WIDTH, h = LCD_HEIGHT;
#define STRIP_H 20
  uint16_t* strip = heap_caps_malloc((size_t)w * STRIP_H * sizeof(uint16_t), MALLOC_CAP_DMA);
  if (!strip) return ESP_ERR_NO_MEM;

#if LCD_SOFT_BYTE_SWAP
  uint16_t swapped = swap16(color565);
  for (int i = 0; i < (int)w * STRIP_H; ++i) strip[i] = swapped;
#else
  for (int i = 0; i < (int)w * STRIP_H; ++i) strip[i] = color565;
#endif

  for (uint16_t y = 0; y < h; y += STRIP_H) {
    uint16_t hh = (y + STRIP_H <= h) ? STRIP_H : (h - y);
    esp_err_t r = esp_lcd_panel_draw_bitmap(panel_handle, 0, y, w, (uint16_t)(y + hh), strip);
    if (r != ESP_OK) {
      free(strip);
      return r;
    }
  }
  free(strip);
  return ESP_OK;
}

#if LCD_SOFT_BYTE_SWAP
static inline uint16_t swap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
#endif

// Reader that supports skip when buf == NULL (TJPGD behavior)
static size_t jpg_mem_reader(void* arg, size_t index, uint8_t* buf, size_t len) {
  jpg_ctx_t* ctx = (jpg_ctx_t*)arg;
  if (!ctx || !ctx->jpg || index >= ctx->jpg_len) return 0;
  size_t n = ctx->jpg_len - index;
  if (n > len) n = len;
  if (buf) memcpy(buf, ctx->jpg + index, n);
  return n;  // also used as "skip"
}

// Writer: copy decoded RGB565 into output frame buffer (bounds guarded)
static bool jpg_write_cb(void* arg, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t* data) {
  jpg_ctx_t* ctx = (jpg_ctx_t*)arg;
  if (!ctx || !ctx->dst || !data) return false;
  if (w == 0 || h == 0) return true;

  if (x >= ctx->out_w || y >= ctx->out_h) return false;
  if (x + w > ctx->out_w) w = ctx->out_w - x;
  if (y + h > ctx->out_h) h = ctx->out_h - y;

  uint16_t* dst = ctx->dst + y * ctx->stride + x;
  for (uint16_t j = 0; j < h; ++j) {
    memcpy(dst + j * ctx->stride, data + j * w * 2, (size_t)w * 2);
  }
  return true;
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static inline uint16_t convert_to_bgr565(uint16_t rgb565) {
  uint8_t r = (rgb565 >> 11) & 0x1F;
  uint8_t g = (rgb565 >> 5) & 0x3F;
  uint8_t b = rgb565 & 0x1F;
  return (b << 11) | (g << 5) | r;
}

// ------------------------------
// 1. Color Bar (Red, Green, Blue...)
// ------------------------------
void lcd_test_color_bar(void) {
  uint16_t colors[] = {
      0xF800,  // Red   (RGB565)
      0x07E0,  // Green (RGB565)
      0x001F,  // Blue  (RGB565)
      0xFFE0,  // Yellow
      0xF81F,  // Magenta
      0x07FF,  // Cyan
      0xFFFF,  // White
      0x0000,  // Black
  };

#if LCD_COLOR_SPACE_BGR
  for (int i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
    colors[i] = convert_to_bgr565(colors[i]);
  }
#endif

  int num_colors = sizeof(colors) / sizeof(colors[0]);
  int bar_width = LCD_WIDTH / num_colors;

  for (int y = 0; y < LCD_HEIGHT; ++y) {
    for (int x = 0; x < LCD_WIDTH; ++x) {
      int idx = x / bar_width;
      lcd_buffer[y * LCD_WIDTH + x] = colors[idx];
    }
  }
  esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, lcd_buffer);
}

// ------------------------------
// 2. Grayscale Bar
// ------------------------------
void lcd_test_grayscale_bar(void) {
  for (int x = 0; x < LCD_WIDTH; ++x) {
    uint8_t gray = (x * 255) / LCD_WIDTH;
    uint16_t gray565 = ((gray >> 3) << 11) | ((gray >> 2) << 5) | (gray >> 3);
    for (int y = 0; y < LCD_HEIGHT; ++y) {
      lcd_buffer[y * LCD_WIDTH + x] = gray565;
    }
  }
  esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, lcd_buffer);
}

// ------------------------------
// 3. Grid Pattern (Every 20px)
// ------------------------------
void lcd_test_grid_pattern(void) {
  for (int y = 0; y < LCD_HEIGHT; ++y) {
    for (int x = 0; x < LCD_WIDTH; ++x) {
      bool is_line = (x % 20 == 0) || (y % 20 == 0);
      lcd_buffer[y * LCD_WIDTH + x] = is_line ? 0xFFFF : 0x0000;
    }
  }
  esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, lcd_buffer);
}

// ------------------------------
// 4. Geometric Shapes (Cross, Diagonal)
// ------------------------------
void lcd_test_geometry_shapes(void) {
  // Clear screen (black)
  for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; ++i) {
    lcd_buffer[i] = 0x0000;
  }

  // Horizontal line (green)
  for (int x = 0; x < LCD_WIDTH; ++x) {
    lcd_buffer[(LCD_HEIGHT / 2) * LCD_WIDTH + x] = 0x07E0;
  }

  // Vertical line (red)
  for (int y = 0; y < LCD_HEIGHT; ++y) {
    lcd_buffer[y * LCD_WIDTH + (LCD_WIDTH / 2)] = 0xF800;
  }

  // Diagonal line (blue and yellow)
  for (int i = 0; i < LCD_WIDTH && i < LCD_HEIGHT; ++i) {
    lcd_buffer[i * LCD_WIDTH + i] = 0x001F;
    lcd_buffer[(LCD_HEIGHT - 1 - i) * LCD_WIDTH + i] = 0xFFE0;
  }

  esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, lcd_buffer);
}

// ------------------------------
// 5. Red-Blue Gradient (X: Red, Y: Blue)
// ------------------------------
void lcd_test_gradient_pattern(void) {
  for (int y = 0; y < LCD_HEIGHT; ++y) {
    for (int x = 0; x < LCD_WIDTH; ++x) {
      uint8_t r = (x * 31) / LCD_WIDTH;
      uint8_t b = (y * 31) / LCD_HEIGHT;
      uint16_t color = (r << 11) | b;
      lcd_buffer[y * LCD_WIDTH + x] = color;
    }
  }
  esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, lcd_buffer);
}

// ------------------------------
// Test Runner
// ------------------------------
void lcd_test_all_patterns(void) {
  lcd_test_color_bar();
  vTaskDelay(pdMS_TO_TICKS(3000));

  lcd_test_grayscale_bar();
  vTaskDelay(pdMS_TO_TICKS(3000));

  lcd_test_grid_pattern();
  vTaskDelay(pdMS_TO_TICKS(3000));

  lcd_test_geometry_shapes();
  vTaskDelay(pdMS_TO_TICKS(3000));

  lcd_test_gradient_pattern();
  vTaskDelay(pdMS_TO_TICKS(3000));
}

static void lcd_test_rgb565_bit_pattern(void) {
  for (int bit = 0; bit < COLOR_BITS; bit++) {
    uint16_t color = 1 << bit;

    // Fill buffer with the test color
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
      lcd_buffer[i] = color;
    }

    ESP_LOGI(TAG, "Bit Test: bit %2d -> color = 0x%04X", bit, color);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, (uint8_t*)&color, sizeof(color), ESP_LOG_INFO);

    // Draw to screen
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, lcd_buffer);

    // Wait for observation
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

static esp_err_t lcd_test_swap_pattern(void) {
  const uint16_t w = LCD_WIDTH;
  const uint16_t h = LCD_HEIGHT;
  const uint16_t block_w = w / 4;
  const uint16_t block_h = h / 4;

  const struct {
    uint8_t r, g, b;
  } rgb[16] = {
      {255, 0, 0},      // Red
      {0, 255, 0},      // Green
      {0, 0, 255},      // Blue
      {255, 255, 255},  // White
      {0, 0, 0},        // Black
      {255, 255, 0},    // Yellow
      {255, 0, 255},    // Magenta
      {0, 255, 255},    // Cyan
      {127, 127, 127},  // Gray
      {64, 64, 64},     // Dark Gray
      {255, 128, 0},    // Orange
      {128, 0, 255},    // Purple
      {0, 128, 128},    // Teal
      {128, 128, 0},    // Olive
      {255, 192, 203},  // Pink
      {0, 0, 128}       // Navy
  };

  uint16_t* buf = malloc(block_w * block_h * sizeof(uint16_t));
  if (!buf) {
    ESP_LOGE(TAG, "Failed to allocate pattern buffer");
    return ESP_ERR_NO_MEM;
  }

  esp_err_t ret = ESP_OK;
  int idx = 0;
  for (uint16_t by = 0; by < 4; ++by) {
    for (uint16_t bx = 0; bx < 4; ++bx) {
      if (idx >= 16) break;

      uint16_t color = rgb565(rgb[idx].r, rgb[idx].g, rgb[idx].b);
#if LCD_SOFT_BYTE_SWAP
      color = swap16(color);
#endif
      for (uint32_t i = 0; i < block_w * block_h; ++i) {
        buf[i] = color;
      }

      uint16_t x_start = bx * block_w;
      uint16_t y_start = by * block_h;
      uint16_t x_end = x_start + block_w;
      uint16_t y_end = y_start + block_h;

      ret = esp_lcd_panel_draw_bitmap(panel_handle, x_start, y_start, x_end, y_end, buf);
      if (ret != ESP_OK) break;

      ++idx;
    }
    if (ret != ESP_OK) break;
  }

  free(buf);
  return ret;
}