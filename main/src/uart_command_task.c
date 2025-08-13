//==================================================================================================
/// @file       uart_command_task.c
/// @brief      UART command handler task
/// @date       2025/8/13
//==================================================================================================

//==================================================================================================
// Include definition
//==================================================================================================
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "camera_task.h"
#include "lcd_driver.h"
#include "uart_command_task.h"

//==================================================================================================
// Constant definition
//==================================================================================================
#define MAX_LINE_LEN 128

#ifdef CONFIG_FW_VERSION
#define FW_VERSION (CONFIG_FW_VERSION)
#endif

#ifdef CONFIG_PRODUCT_NAME
#define PRODUCT_NAME (CONFIG_PRODUCT_NAME)
#endif

//==================================================================================================
// Prototype
//==================================================================================================
static void cmd_help(const char* args);
static void cmd_version(const char* args);
// Camera command
static void cmd_take_picture(const char* args);
static void cmd_start_camera(const char* args);
static void cmd_stop_camera(const char* args);
// LCD command
static void cmd_test_pattern_color_bar(const char* args);
static void cmd_test_pattern_grayscale_bar(const char* args);
static void cmd_test_pattern_grid(const char* args);
static void cmd_test_pattern_geometry(const char* args);
static void cmd_test_pattern_gradient(const char* args);
static void cmd_test_pattern_all(const char* args);

static void uart_cmd_task(void* arg);

//==================================================================================================
// Private values definition
//==================================================================================================

static const char* TAG = "UART";
typedef void (*cmd_func_t)(const char* args);

typedef struct {
  const char* name;
  const char* help;
  cmd_func_t handler;
} command_entry_t;

static const command_entry_t command_table[] = {
    {"help", "Show this help message", cmd_help},
    {"h", "Show this help message", cmd_help},
    {"version", "Show firmware version", cmd_version},
    {"v", "Show firmware version", cmd_version},
    // Camaera
    {"take_picture", "Take picture and display on LCD (save if SD enabled)", cmd_take_picture},
    {"take", "Take picture and display on LCD (save if SD enabled)", cmd_take_picture},
    {"start_camera", "Start continuous camera capture every 100ms", cmd_start_camera},
    {"start", "Start continuous camera capture every 100ms", cmd_start_camera},
    {"stop_camera", "Stop continuous camera capture", cmd_stop_camera},
    {"stop", "Stop continuous camera capture", cmd_stop_camera},
    // LCD
    {"cmd_test_pattern_color_bar", "Display LCD color bar pattern", cmd_test_pattern_color_bar},
    {"tp1", "Display LCD color bar pattern", cmd_test_pattern_color_bar},
    {"cmd_test_pattern_grayscale_bar", "Display LCD grayscale bar pattern",
     cmd_test_pattern_grayscale_bar},
    {"tp2", "Display LCD grayscale bar pattern", cmd_test_pattern_grayscale_bar},
    {"cmd_test_pattern_grid", "Display LCD grid pattern", cmd_test_pattern_grid},
    {"tp3", "Display LCD grid pattern", cmd_test_pattern_grid},
    {"cmd_test_pattern_geometry", "Display LCD geometry shapes pattern", cmd_test_pattern_geometry},
    {"tp4", "Display LCD geometry shapes pattern", cmd_test_pattern_geometry},
    {"cmd_test_pattern_gradient", "Display LCD gradient pattern", cmd_test_pattern_gradient},
    {"tp5", "Display LCD gradient pattern", cmd_test_pattern_gradient},
    {"cmd_test_pattern_all", "Run all LCD test patterns sequentially", cmd_test_pattern_all},
    {"tp0", "Run all LCD test patterns sequentially", cmd_test_pattern_all},
};

//==================================================================================================
// Public functions
//==================================================================================================

void uart_command_task_start(void) {
  xTaskCreate(uart_cmd_task, "uart_cmd_task", 4096, NULL, 5, NULL);
}

//==================================================================================================
// Private functions
//==================================================================================================

static void cmd_help(const char* args) {
  printf("\nCommand List:\n");
  for (size_t i = 0; i < sizeof(command_table) / sizeof(command_table[0]); i++) {
    printf("  %-10s - %s\n", command_table[i].name, command_table[i].help);
  }
}

static void cmd_version(const char* args) {
  printf("\nProduct name: %s\nFW Version: %s\n", PRODUCT_NAME, FW_VERSION);
}

// Camera command
static void cmd_take_picture(const char* args) {
  ESP_LOGI(TAG, "Command: Take Picture");
  camera_task_trigger_capture();
}

static void cmd_start_camera(const char* args) {
  ESP_LOGI(TAG, "Command: Start Camera Streaming");
  camera_task_start_stream();
}

static void cmd_stop_camera(const char* args) {
  ESP_LOGI(TAG, "Command: Stop Camera Streaming");
  camera_task_stop_stream();
}

// LCD command
static void cmd_test_pattern_color_bar(const char* args) {
  ESP_LOGI(TAG, "Display: Color Bar Pattern");
  lcd_test_color_bar();
}

static void cmd_test_pattern_grayscale_bar(const char* args) {
  ESP_LOGI(TAG, "Display: Grayscale Bar Pattern");
  lcd_test_grayscale_bar();
}

static void cmd_test_pattern_grid(const char* args) {
  ESP_LOGI(TAG, "Display: Grid Pattern");
  lcd_test_grid_pattern();
}

static void cmd_test_pattern_geometry(const char* args) {
  ESP_LOGI(TAG, "Display: Geometry Shapes");
  lcd_test_geometry_shapes();
}

static void cmd_test_pattern_gradient(const char* args) {
  ESP_LOGI(TAG, "Display: Gradient Pattern");
  lcd_test_gradient_pattern();
}

static void cmd_test_pattern_all(const char* args) {
  ESP_LOGI(TAG, "Display: All Test Patterns");
  lcd_test_all_patterns();
}

//--------------------------------------------------------------------------------------------------
/// @brief  UART command handler task
//--------------------------------------------------------------------------------------------------
static void uart_cmd_task(void* arg) {
  char line[MAX_LINE_LEN];

  while (1) {
    // Read line from stdin
    if (fgets(line, sizeof(line), stdin) == NULL) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // Trim newline
    char* newline = strchr(line, '\n');
    if (newline) *newline = '\0';

    // Skip empty lines
    if (strlen(line) == 0) continue;

    // Extract command and args
    char* cmd = strtok(line, " \t");
    // get remaining part after first space
    char* args = strtok(NULL, "");

    if (!cmd) continue;  // safety check

    // Search and execute matching command
    bool matched = false;
    for (size_t i = 0; i < sizeof(command_table) / sizeof(command_table[0]); i++) {
      if (strcmp(cmd, command_table[i].name) == 0) {
        // Call handler with args (can be NULL)
        command_table[i].handler(args);
        matched = true;
        break;
      }
    }

    if (!matched) {
      printf("Unknown command: %s\n", cmd);
      cmd_help(NULL);
    }
  }
}
