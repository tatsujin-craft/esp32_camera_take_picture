//======================================================================================================================
/// @file       camera_driver.h
/// @brief      camera driver
/// @date       2024/7/2
//======================================================================================================================

#pragma once

//======================================================================================================================
// Include definition
//======================================================================================================================
#include <stdint.h>
#include "esp_err.h"

//======================================================================================================================
// Public function
//======================================================================================================================
#ifdef __cplusplus
extern "C" {
#endif

esp_err_t camera_init(void);

#ifdef __cplusplus
}
#endif
