//======================================================================================================================
/// @file       main.c
/// @brief      main
/// @date       2024/7/2
//======================================================================================================================

//======================================================================================================================
// Include definition
//======================================================================================================================
#include <esp_log.h>
#include <esp_system.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "camera_driver.h"
#include "sdcard_fatfs_manager.h"

//======================================================================================================================
// Constant definition
//======================================================================================================================
#ifdef CONFIG_STREAM_MODE_ENABLED
#define STREAM_MODE_ENABLED (CONFIG_STREAM_MODE_ENABLED)
#else
#define STREAM_MODE_ENABLED (false)
#endif

//======================================================================================================================
// Private values definition
//======================================================================================================================
static const char* TAG = "MAIN";

//======================================================================================================================
// Public functions
//======================================================================================================================

//----------------------------------------------------------------------------------------------------------------------
/// @brief  main
//----------------------------------------------------------------------------------------------------------------------
void app_main(void) {
  // Initialize camera
  if (camera_init() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize camera");
    return;
  }

  // Initialize SD card
  if (sdcard_init() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SD card");
    return;
  }

  uint32_t saved_frame_count = 0;
  while (1) {
    ESP_LOGI(TAG, "Taking picture...");
    camera_fb_t* frame_buffer = esp_camera_fb_get();
    if (frame_buffer) {
      ESP_LOGI(TAG, "Picture taken, size: %zu bytes", frame_buffer->len);
      if (sdcard_save_picture(frame_buffer, saved_frame_count) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save picture to SD card");
      } else {
        ESP_LOGI(TAG, "Saved picture to SD card");
        saved_frame_count++;
      }
      esp_camera_fb_return(frame_buffer);
    } else {
      ESP_LOGE(TAG, "Failed to take picture");
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
