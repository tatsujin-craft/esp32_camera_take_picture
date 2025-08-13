//======================================================================================================================
/// @file       camera_task.c
/// @brief      camera task
/// @date       2025/8/13
//======================================================================================================================

//======================================================================================================================
// Include definition
//======================================================================================================================
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "camera_driver.h"
#include "lcd_driver.h"
#include "sd_card_driver.h"

//======================================================================================================================
// Constant definition
//======================================================================================================================
#define ENABLE_SD_SAVE (false)

#define GPIO_BTN_CAPTURE 0
#define GPIO_LED_CAPTURE 2

//======================================================================================================================
// Prototype
//======================================================================================================================
static void camera_task(void* arg);
static void camera_button_init(void);
static void camera_led_init(void);
static void take_picture(void);
static void display_picture(void);

//======================================================================================================================
// Private values definition
//======================================================================================================================
static const char* TAG = "CAMERA_TASK";

// Command queue for camera task
static QueueHandle_t camera_cmd_queue = NULL;

// Camera command types
typedef enum {
  CAMERA_CMD_TAKE_PICTURE,
  CAMERA_CMD_START_STREAM,
  CAMERA_CMD_STOP_STREAM
} camera_cmd_t;

static bool streaming_enabled = false;

//======================================================================================================================
// Public functions
//======================================================================================================================

void camera_task_start(void) {
  camera_cmd_queue = xQueueCreate(4, sizeof(camera_cmd_t));
  xTaskCreate(camera_task, "camera_task", 4096, NULL, 5, NULL);

#if ENABLE_SD_SAVE
  if (sdcard_init() == ESP_OK) {
    ESP_LOGI(TAG, "SD card initialized successfully");
  } else {
    ESP_LOGE(TAG, "Failed to initialize SD card");
  }
#endif

  camera_button_init();
  camera_led_init();
}

// Send capture command to camera task
void camera_task_trigger_capture(void) {
  camera_cmd_t cmd = CAMERA_CMD_TAKE_PICTURE;
  xQueueSend(camera_cmd_queue, &cmd, 0);
}

void camera_task_start_stream(void) {
  camera_cmd_t cmd = CAMERA_CMD_START_STREAM;
  xQueueSend(camera_cmd_queue, &cmd, 0);
}

void camera_task_stop_stream(void) {
  camera_cmd_t cmd = CAMERA_CMD_STOP_STREAM;
  xQueueSend(camera_cmd_queue, &cmd, 0);
}

//======================================================================================================================
// Private functions
//======================================================================================================================

//----------------------------------------------------------------------------------------------------------------------
/// @brief  camera task
//----------------------------------------------------------------------------------------------------------------------
static void camera_task(void* arg) {
  camera_cmd_t cmd;
  uint32_t saved_frame_count = 0;

  while (1) {
    if (xQueueReceive(camera_cmd_queue, &cmd,
                      pdMS_TO_TICKS(streaming_enabled ? 100 : portMAX_DELAY)) == pdTRUE) {
      if (cmd == CAMERA_CMD_TAKE_PICTURE) {
        take_picture();
      } else if (cmd == CAMERA_CMD_START_STREAM) {
        streaming_enabled = true;
      } else if (cmd == CAMERA_CMD_STOP_STREAM) {
        streaming_enabled = false;
      }
    }

    if (streaming_enabled) {
      display_picture();
    }
  }
}

static void IRAM_ATTR button_isr_handler(void* arg) { camera_task_trigger_capture(); }

static void camera_button_init(void) {
  gpio_config_t io_conf = {.intr_type = GPIO_INTR_NEGEDGE,
                           .mode = GPIO_MODE_INPUT,
                           .pin_bit_mask = 1ULL << GPIO_BTN_CAPTURE,
                           .pull_up_en = GPIO_PULLUP_ENABLE,
                           .pull_down_en = GPIO_PULLDOWN_DISABLE};
  gpio_config(&io_conf);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(GPIO_BTN_CAPTURE, button_isr_handler, NULL);

  ESP_LOGI(TAG, "Camera capture button initialized on GPIO %d", GPIO_BTN_CAPTURE);
}

static void camera_led_init(void) {
  gpio_config_t led_conf = {.intr_type = GPIO_INTR_DISABLE,
                            .mode = GPIO_MODE_OUTPUT,
                            .pin_bit_mask = 1ULL << GPIO_LED_CAPTURE,
                            .pull_up_en = GPIO_PULLUP_DISABLE,
                            .pull_down_en = GPIO_PULLDOWN_DISABLE};
  gpio_config(&led_conf);

  gpio_set_level(GPIO_LED_CAPTURE, 0);
  ESP_LOGI(TAG, "Capture LED initialized on GPIO%d", GPIO_LED_CAPTURE);
}

static void take_picture(void) {
  gpio_set_level(GPIO_LED_CAPTURE, 1);  // LED ON

  camera_fb_t* frame_buffer = esp_camera_fb_get();
  if (frame_buffer) {
    lcd_display_show(frame_buffer);

#if ENABLE_SD_SAVE
    static uint32_t saved_frame_count = 0;
    if (sdcard_save_picture(frame_buffer, saved_frame_count) == ESP_OK) {
      saved_frame_count++;
      ESP_LOGI(TAG, "Picture saved to SD");
    } else {
      ESP_LOGE(TAG, "Failed to save picture to SD");
    }
#endif

    esp_camera_fb_return(frame_buffer);
  } else {
    ESP_LOGE(TAG, "Failed to take picture");
  }

  gpio_set_level(GPIO_LED_CAPTURE, 0);  // LED OFF
}

static void display_picture(void) {
  camera_fb_t* frame_buffer = esp_camera_fb_get();
  if (frame_buffer) {
    lcd_display_show(frame_buffer);
    esp_camera_fb_return(frame_buffer);
  } else {
    ESP_LOGE(TAG, "Failed to capture frame");
  }
}
