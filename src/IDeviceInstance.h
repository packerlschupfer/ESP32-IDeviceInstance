/**
 * @file IDeviceInstance.h
 * @brief Abstract interface for device instances in ESP32/FreeRTOS environment
 * 
 * This interface provides a standardized way to interact with various device types
 * in embedded systems, supporting asynchronous operations, thread-safe data access,
 * and extensible data type handling.
 * 
 * @version 4.0.0
 * @date 2025-01-06
 */

#ifndef IDEVICEINSTANCE_H
#define IDEVICEINSTANCE_H

#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include <vector>
#include <functional>

// Include common Result type
#include "Result.h"

// Include logging configuration
#include "IDeviceInstanceLogging.h"

/**
 * @class IDeviceInstance
 * @brief Abstract base class for device instance implementations
 * 
 * This interface defines the contract for device instances that need to:
 * - Initialize hardware and communication interfaces
 * - Request and process data asynchronously
 * - Provide thread-safe access to device data
 * - Support multiple data types through extensible enums
 * - Perform device-specific actions
 * 
 * @note Implementations must ensure thread safety for all public methods
 * @note Memory management must be explicit with no memory leaks
 */
class IDeviceInstance {
public:
    /**
     * @brief Virtual destructor ensures proper cleanup of derived classes
     */
    virtual ~IDeviceInstance() = default;

    /**
     * @enum DeviceError
     * @brief Standard error codes for device operations
     *
     * Unified error codes for all device operations. This enum contains
     * all error types needed by device implementations.
     */
    enum class DeviceError {
        SUCCESS = 0,            ///< Operation completed successfully
        NOT_INITIALIZED,        ///< Device not initialized
        TIMEOUT,                ///< Operation timed out
        MUTEX_ERROR,            ///< Failed to acquire mutex
        COMMUNICATION_ERROR,    ///< Communication with device failed
        INVALID_PARAMETER,      ///< Invalid parameter provided
        DATA_NOT_READY,         ///< Data not yet available
        MEMORY_ERROR,           ///< Memory allocation failed
        DEVICE_BUSY,            ///< Device is busy with another operation
        NOT_SUPPORTED,          ///< Operation not supported by device
        UNKNOWN_ERROR           ///< Unspecified error occurred
    };


    /**
     * @enum DeviceDataType
     * @brief Enumeration of supported device data types
     * 
     * This enum can be extended in derived classes to support additional
     * sensor types specific to the device implementation.
     */
    enum class DeviceDataType {
        TEMPERATURE,    ///< Temperature reading in degrees Celsius
        HUMIDITY,       ///< Relative humidity percentage (0-100)
        PRESSURE,       ///< Atmospheric pressure in hPa
        RELAY_STATE,    ///< Binary relay state (0=off, 1=on)
        // Add other sensor data types as needed

        NUM_TYPES       ///< Sentinel value - must remain last
    };

    /**
     * @brief Result type for device operations using common::Result
     * @tparam T The type of value returned on success
     *
     * Uses common::Result from LibraryCommon with DeviceError as error type.
     */
    template<typename T>
    using DeviceResult = common::Result<T, DeviceError>;

    /**
     * @enum EventType
     * @brief Types of events that can be notified via callbacks
     */
    enum class EventType {
        INITIALIZED,        ///< Device initialization complete
        DATA_READY,         ///< New data available
        ERROR_OCCURRED,     ///< An error occurred
        STATE_CHANGED,      ///< Device state changed
        CUSTOM_EVENT        ///< Device-specific event
    };
    
    /**
     * @brief Event notification structure
     */
    struct EventNotification {
        EventType type;         ///< Type of event
        DeviceError error;      ///< Associated error code (if applicable)
        int customData;         ///< Custom data for device-specific events
    };
    
    /**
     * @brief Callback function type for event notifications
     * @param notification The event notification data
     */
    using EventCallback = std::function<void(const EventNotification& notification)>;

    /**
     * @brief Initialize the device instance
     * 
     * This method should:
     * - Initialize hardware interfaces
     * - Create necessary mutexes and event groups
     * - Set up communication protocols
     * - Configure device-specific parameters
     * 
     * @return DeviceResult<void> with appropriate error code
     * @note Must be called before any other operations
     * @note Implementation should be idempotent
     */
    virtual DeviceResult<void> initialize() = 0;
    
    /**
     * @brief Check if the device is initialized and ready
     * @return true if initialized, false otherwise
     * @note Thread-safe
     * @note noexcept guarantee for performance-critical checks
     */
    virtual bool isInitialized() const noexcept = 0;
    
    /**
     * @brief Block until device initialization is complete
     * 
     * This method should wait indefinitely for initialization to complete.
     * Consider using the overloaded version with timeout for better control.
     * 
     * @note May block indefinitely if initialization fails
     * @deprecated Use waitForInitialization(TickType_t) instead
     */
    virtual void waitForInitialization() = 0;
    
    /**
     * @brief Wait for initialization with timeout
     *
     * @param xTicksToWait Maximum time to wait in FreeRTOS ticks
     *                     Use portMAX_DELAY for indefinite wait
     * @return DeviceError::SUCCESS if initialized, DeviceError::TIMEOUT if timed out
     *
     * @note Default implementation calls waitForInitialization() and returns SUCCESS
     */
    virtual DeviceError waitForInitialization(TickType_t xTicksToWait) {
        (void)xTicksToWait; // Suppress unused parameter warning
        waitForInitialization();
        return DeviceError::SUCCESS;
    }
    
    /**
     * @brief Wait for initialization to complete with timeout (modern API)
     * 
     * This method provides a DeviceResult-based API for waiting on initialization.
     * It's the preferred method for new implementations.
     * 
     * @param timeout Maximum time to wait in FreeRTOS ticks (default: portMAX_DELAY)
     * @return DeviceResult<void> with appropriate error code
     * 
     * @note Implementations should convert ErrorCode from waitForInitialization() if needed
     */
    virtual DeviceResult<void> waitForInitializationComplete(TickType_t timeout = portMAX_DELAY) = 0;
    
    /**
     * @brief Request data from the device
     * 
     * Initiates an asynchronous data request. Use waitForData() to
     * wait for completion and processData() to handle the results.
     * 
     * @return DeviceResult<void> with appropriate error code
     * @note Non-blocking operation
     */
    virtual DeviceResult<void> requestData() = 0;
    
    /**
     * @brief Wait for pending data request to complete
     * 
     * Blocks until data is available or an error occurs.
     * Consider using the overloaded version with timeout for better control.
     * 
     * @return true if data is ready, false on error
     * @deprecated Use waitForData(TickType_t) instead
     */
    virtual bool waitForData() = 0;
    
    /**
     * @brief Wait for data with timeout
     *
     * @param xTicksToWait Maximum time to wait in FreeRTOS ticks
     *                     Use portMAX_DELAY for indefinite wait
     * @return DeviceError indicating the result of the operation
     *
     * @note Default implementation calls waitForData() and converts result
     */
    virtual DeviceError waitForData(TickType_t xTicksToWait) {
        (void)xTicksToWait; // Suppress unused parameter warning
        return waitForData() ? DeviceError::SUCCESS : DeviceError::UNKNOWN_ERROR;
    }
    
    /**
     * @brief Process received data
     * 
     * Called after waitForData() returns successfully. This method should:
     * - Parse raw device data
     * - Update internal data structures
     * - Handle any data validation
     * 
     * @return DeviceResult<void> with appropriate error code
     * @note Must be called after successful waitForData()
     */
    virtual DeviceResult<void> processData() = 0;

    /**
     * @brief Retrieve data of the specified type
     * 
     * @param dataType The type of data to retrieve
     * @return DeviceResult<std::vector<float>> containing error code and values
     * 
     * @note Thread-safe - implementations must use appropriate locking
     * @note May return cached data or trigger new data acquisition
     */
    virtual DeviceResult<std::vector<float>> getData(DeviceDataType dataType) = 0;
    
    /**
     * @brief Get the instance-level mutex
     * 
     * This mutex protects instance-specific data and state.
     * 
     * @return Handle to the instance mutex
     * @note Caller must not delete the returned handle
     * @note noexcept as this should never throw
     */
    virtual SemaphoreHandle_t getMutexInstance() const noexcept = 0;
    
    /**
     * @brief Get the interface-level mutex
     * 
     * This mutex protects shared interface resources (e.g., I2C, SPI bus).
     * 
     * @return Handle to the interface mutex
     * @note Caller must not delete the returned handle
     * @note noexcept as this should never throw
     */
    virtual SemaphoreHandle_t getMutexInterface() const noexcept = 0;

    /**
     * @brief Get the event group handle for this device
     * 
     * Event groups are used for signaling various device states and events.
     * Implementations should document which event bits are used.
     * 
     * @return Handle to the device's event group
     * @note Caller must not delete the returned handle
     * @note noexcept as this should never throw
     */
    virtual EventGroupHandle_t getEventGroup() const noexcept = 0;

    /**
     * @brief Perform a device-specific action
     * 
     * This method provides a generic interface for device-specific operations
     * that don't fit the standard data acquisition model.
     * 
     * @param actionId Device-specific action identifier
     * @param actionParam Optional parameter for the action
     * @return DeviceResult<void> with appropriate error code
     * 
     * @note Implementations should document supported action IDs
     * @note Thread-safe - implementations must use appropriate locking
     */
    virtual DeviceResult<void> performAction(int actionId, int actionParam) = 0;

    /**
     * @brief Validate if a data type value is within valid range
     * 
     * @param dataType Integer representation of the data type
     * @return true if valid, false otherwise
     * 
     * @note Useful for validating user input or configuration values
     * @note constexpr for compile-time validation when possible
     */
    static constexpr bool isValidDataType(int dataType) noexcept {
        return dataType >= 0 && dataType < static_cast<int>(DeviceDataType::NUM_TYPES);
    }

    /**
     * @brief Convert enum value to its underlying type
     * 
     * @tparam EnumType The enum type to convert
     * @param enumVal The enum value to convert
     * @return The underlying integer value
     * 
     * @note Useful for serialization or when interfacing with C APIs
     */
    template<typename EnumType>
    static constexpr typename std::underlying_type<EnumType>::type toUnderlyingType(EnumType enumVal) noexcept {
        return static_cast<typename std::underlying_type<EnumType>::type>(enumVal);
    }
    
    /**
     * @brief Convert device error to string representation
     *
     * @param error The device error to convert
     * @return C-string representation of the error
     */
    static const char* deviceErrorToString(DeviceError error) noexcept {
        switch (error) {
            case DeviceError::SUCCESS:             return "Success";
            case DeviceError::NOT_INITIALIZED:     return "Not initialized";
            case DeviceError::TIMEOUT:             return "Timeout";
            case DeviceError::MUTEX_ERROR:         return "Mutex error";
            case DeviceError::COMMUNICATION_ERROR: return "Communication error";
            case DeviceError::INVALID_PARAMETER:   return "Invalid parameter";
            case DeviceError::DATA_NOT_READY:      return "Data not ready";
            case DeviceError::MEMORY_ERROR:        return "Memory error";
            case DeviceError::DEVICE_BUSY:         return "Device busy";
            case DeviceError::NOT_SUPPORTED:       return "Not supported";
            case DeviceError::UNKNOWN_ERROR:       return "Unknown error";
            default:                               return "Invalid error code";
        }
    }

    /**
     * @brief Convert error code to string representation (alias for deviceErrorToString)
     * @param error The error code to convert
     * @return C-string representation of the error
     */
    static const char* errorToString(DeviceError error) noexcept {
        return deviceErrorToString(error);
    }
    
    /**
     * @brief Register a callback for event notifications
     * 
     * @param callback The callback function to register
     * @return DeviceResult<void> with appropriate error code
     * 
     * @note Implementation should support multiple callbacks
     * @note Callbacks should be invoked from a separate task to avoid blocking
     * @note Implementations can return UNKNOWN_ERROR for unsupported operations
     */
    virtual DeviceResult<void> registerCallback(EventCallback callback) = 0;
    
    /**
     * @brief Unregister all callbacks
     * 
     * @return DeviceResult<void> with appropriate error code
     * @note Implementations can return UNKNOWN_ERROR for unsupported operations
     */
    virtual DeviceResult<void> unregisterCallbacks() = 0;
    
    /**
     * @brief Enable or disable event notifications
     * 
     * @param eventType The type of event to configure
     * @param enable true to enable, false to disable
     * @return DeviceResult<void> with appropriate error code
     * 
     * @note Allows fine-grained control over which events trigger callbacks
     * @note Implementations can return UNKNOWN_ERROR for unsupported operations
     */
    virtual DeviceResult<void> setEventNotification(EventType eventType, bool enable) = 0;

};

#endif // IDEVICEINSTANCE_H
