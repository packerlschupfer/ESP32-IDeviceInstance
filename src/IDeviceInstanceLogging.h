/**
 * @file IDeviceInstanceLogging.h
 * @brief Logging configuration for IDeviceInstance library
 * 
 * This header provides flexible logging configuration with support for:
 * - ESP-IDF logging (default)
 * - Custom Logger via LogInterface (when USE_CUSTOM_LOGGER is defined)
 * - Debug level control (when IDEVICEINSTANCE_DEBUG is defined)
 * 
 * @version 1.5.0
 * @date 2025-01-21
 */

#ifndef IDEVICEINSTANCE_LOGGING_H
#define IDEVICEINSTANCE_LOGGING_H

#define IDEV_LOG_TAG "IDev"

// Define log levels based on debug flag
#ifdef IDEVICEINSTANCE_DEBUG
    // Debug mode: Show all levels
    #define IDEV_LOG_LEVEL_E ESP_LOG_ERROR
    #define IDEV_LOG_LEVEL_W ESP_LOG_WARN
    #define IDEV_LOG_LEVEL_I ESP_LOG_INFO
    #define IDEV_LOG_LEVEL_D ESP_LOG_DEBUG
    #define IDEV_LOG_LEVEL_V ESP_LOG_VERBOSE
#else
    // Release mode: Only Error, Warn, Info
    #define IDEV_LOG_LEVEL_E ESP_LOG_ERROR
    #define IDEV_LOG_LEVEL_W ESP_LOG_WARN
    #define IDEV_LOG_LEVEL_I ESP_LOG_INFO
    #define IDEV_LOG_LEVEL_D ESP_LOG_NONE  // Suppress
    #define IDEV_LOG_LEVEL_V ESP_LOG_NONE  // Suppress
#endif

// Route to custom logger or ESP-IDF
#ifdef USE_CUSTOM_LOGGER
    #include <LogInterface.h>
    #define IDEV_LOG_E(...) LOG_WRITE(IDEV_LOG_LEVEL_E, IDEV_LOG_TAG, __VA_ARGS__)
    #define IDEV_LOG_W(...) LOG_WRITE(IDEV_LOG_LEVEL_W, IDEV_LOG_TAG, __VA_ARGS__)
    #define IDEV_LOG_I(...) LOG_WRITE(IDEV_LOG_LEVEL_I, IDEV_LOG_TAG, __VA_ARGS__)
    #define IDEV_LOG_D(...) LOG_WRITE(IDEV_LOG_LEVEL_D, IDEV_LOG_TAG, __VA_ARGS__)
    #define IDEV_LOG_V(...) LOG_WRITE(IDEV_LOG_LEVEL_V, IDEV_LOG_TAG, __VA_ARGS__)
#else
    // ESP-IDF logging with compile-time suppression
    #include <esp_log.h>
    #define IDEV_LOG_E(...) ESP_LOGE(IDEV_LOG_TAG, __VA_ARGS__)
    #define IDEV_LOG_W(...) ESP_LOGW(IDEV_LOG_TAG, __VA_ARGS__)
    #define IDEV_LOG_I(...) ESP_LOGI(IDEV_LOG_TAG, __VA_ARGS__)
    #ifdef IDEVICEINSTANCE_DEBUG
        #define IDEV_LOG_D(...) ESP_LOGD(IDEV_LOG_TAG, __VA_ARGS__)
        #define IDEV_LOG_V(...) ESP_LOGV(IDEV_LOG_TAG, __VA_ARGS__)
    #else
        #define IDEV_LOG_D(...) ((void)0)
        #define IDEV_LOG_V(...) ((void)0)
    #endif
#endif

// Convenience macro for critical debug info (always uses debug level)
#define IDEV_LOG_DEBUG(...) IDEV_LOG_D(__VA_ARGS__)

// Advanced debug patterns for future implementation use

// Feature-specific debug flags (can be enabled individually)
#ifdef IDEVICEINSTANCE_DEBUG
    // Enable all debug features by default
    #define IDEVICEINSTANCE_DEBUG_TIMING     // Performance timing
    #define IDEVICEINSTANCE_DEBUG_STATE      // State transitions
    #define IDEVICEINSTANCE_DEBUG_DATA       // Data dumps
#endif

// Performance timing macros
#ifdef IDEVICEINSTANCE_DEBUG_TIMING
    #define IDEV_TIME_START() unsigned long _idev_start = millis()
    #define IDEV_TIME_END(msg) IDEV_LOG_D("Timing: %s took %lu ms", msg, millis() - _idev_start)
#else
    #define IDEV_TIME_START() ((void)0)
    #define IDEV_TIME_END(msg) ((void)0)
#endif

// State transition logging
#ifdef IDEVICEINSTANCE_DEBUG_STATE
    #define IDEV_LOG_STATE(from, to) IDEV_LOG_D("State transition: %s -> %s", from, to)
#else
    #define IDEV_LOG_STATE(from, to) ((void)0)
#endif

// Data dump macro
#ifdef IDEVICEINSTANCE_DEBUG_DATA
    #define IDEV_DUMP_DATA(msg, data, len) do { \
        IDEV_LOG_D("%s (%d bytes):", msg, len); \
        for (int _i = 0; _i < len && _i < 32; _i++) { \
            IDEV_LOG_D("  [%02d] = 0x%02X", _i, data[_i]); \
        } \
        if (len > 32) IDEV_LOG_D("  ... (%d more bytes)", len - 32); \
    } while(0)
#else
    #define IDEV_DUMP_DATA(msg, data, len) ((void)0)
#endif

#endif // IDEVICEINSTANCE_LOGGING_H