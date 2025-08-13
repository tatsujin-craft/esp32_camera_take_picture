#pragma once
#include "esp_err.h"
#include "esp_camera.h"

esp_err_t lcd_display_init(void);
esp_err_t lcd_display_show(camera_fb_t *frame);

// LCD test patterns
void lcd_test_color_bar(void);
void lcd_test_grayscale_bar(void);
void lcd_test_grid_pattern(void);
void lcd_test_geometry_shapes(void);
void lcd_test_gradient_pattern(void);
void lcd_test_all_patterns(void);
