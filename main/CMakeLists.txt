idf_component_register(
    SRCS
        "src/main.c"
        "src/camera_driver.c"
        "src/sdcard_fatfs_manager.c"
    
    INCLUDE_DIRS 
        "."
    
    REQUIRES 
        esp32-camera
        esp_timer
        nvs_flash
        fatfs
)
