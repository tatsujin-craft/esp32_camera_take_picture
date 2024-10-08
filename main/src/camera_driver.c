//======================================================================================================================
/// @file       camera_driver.c
/// @brief      camera driver
/// @date       2024/7/2
//======================================================================================================================

//======================================================================================================================
// Include definition
//======================================================================================================================
#include <esp_log.h>
#include "esp_camera.h"

#include "camera_driver.h"

//======================================================================================================================
// Constant definition
//======================================================================================================================
#define BOARD_WROVER_KIT 1

// WROVER-KIT PIN Map
#ifdef BOARD_WROVER_KIT
#define CAM_PIN_PWDN -1   // power down is not used
#define CAM_PIN_RESET -1  // software reset will be performed
#define CAM_PIN_XCLK 21
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27
#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 19
#define CAM_PIN_D2 18
#define CAM_PIN_D1 5
#define CAM_PIN_D0 4
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22
#endif

//======================================================================================================================
// Private values definition
//======================================================================================================================
static const char* TAG = "CAMERA";

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,
    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

//======================================================================================================================
// Public functions
//======================================================================================================================

esp_err_t camera_init(void) {
  esp_err_t result = esp_camera_init(&camera_config);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize camera (%s)", esp_err_to_name(result));
    return result;
  }
  return ESP_OK;
}
