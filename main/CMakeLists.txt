idf_component_register(
    SRCS
        "src/main.c"
        "src/camera_driver.c"
        "src/sd_card_driver.c"
        "src/lcd_driver.c"
        "src/uart_command_task.c"
        "src/camera_task.c"
    
    INCLUDE_DIRS 
        "."
    
    REQUIRES 
        esp32-camera
        esp_timer
        nvs_flash
        fatfs
        esp_lcd
)
