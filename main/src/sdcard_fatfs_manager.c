//======================================================================================================================
/// @file       sdcard_fatfs_manager.c
/// @brief      SD card FATFS manager
/// @date       2024/7/2
//======================================================================================================================

//======================================================================================================================
// Include definition
//======================================================================================================================
#include <esp_log.h>
#include "driver/sdmmc_defs.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "sdcard_fatfs_manager.h"

//======================================================================================================================
// Constant definition
//======================================================================================================================
#define MOUNT_POINT "/sdcard"
#define FILE_PATH_PREFIX "/sdcard/picture"
#define EXTENSION ".jpg"

//======================================================================================================================
// Private values definition
//======================================================================================================================
static const char* TAG = "SD CARD";

//======================================================================================================================
// Public functions
//======================================================================================================================

esp_err_t sdcard_init(void) {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  esp_err_t result;
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = 1;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
  };

  sdmmc_card_t* card;
  result = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
  if (result == ESP_ERR_TIMEOUT || result == ESP_ERR_NOT_FOUND) {
    ESP_LOGI(TAG, "SD card not inserted");
    return result;
  } else if (result != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount SD card (%s)", esp_err_to_name(result));
    return result;
  }

  ESP_LOGI(TAG, "Successfully mounted SD card");
  return ESP_OK;
}

esp_err_t sdcard_save_picture(camera_fb_t* picture_buffer, uint32_t save_file_index) {
  char save_jpeg_name[50];
  snprintf(save_jpeg_name, sizeof(save_jpeg_name), "%s%" PRIu32 "%s", FILE_PATH_PREFIX, save_file_index, EXTENSION);

  FILE* write_file = fopen(save_jpeg_name, "wb");
  if (write_file == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return ESP_FAIL;
  }
  fwrite(picture_buffer->buf, 1, picture_buffer->len, write_file);
  fclose(write_file);

  ESP_LOGI(TAG, "Successfully saved picture: %s", save_jpeg_name);
  return ESP_OK;
}
