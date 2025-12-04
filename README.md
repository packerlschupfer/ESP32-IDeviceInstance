# IDeviceInstance Library

A thread-safe abstract interface for device instances in ESP32/FreeRTOS environments. This library provides a standardized way to interact with various hardware devices while ensuring proper synchronization and error handling.

## Features

- **Thread-Safe Operations**: All public methods are designed with thread safety in mind
- **Async Data Acquisition**: Non-blocking request/wait/process pattern for device data
- **Type-Safe Error Handling**: DeviceResult<T> template for robust error management (v2.0.0+)
- **Comprehensive Error Handling**: Rich error codes with string representations
- **Timeout Support**: Configurable timeouts for all blocking operations
- **Extensible Data Types**: Support for various sensor types with easy extension
- **FreeRTOS Integration**: Native support for mutexes and event groups
- **Performance Optimized**: `noexcept` specifications for critical operations
- **Production-Ready Logging**: Zero-overhead debug logging with ESP-IDF/custom Logger support (v1.5.0+)

## Requirements

- ESP32 with ESP-IDF or Arduino framework
- FreeRTOS (included with ESP32)
- C++11 or later

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps = 
    https://github.com/your-username/IDeviceInstance
```

### Manual Installation

1. Clone this repository into your project's lib folder
2. Include the header file in your code

## Usage

### Basic Implementation

Create a concrete implementation of the interface:

```cpp
#include "IDeviceInstance.h"

class TemperatureSensor : public IDeviceInstance {
private:
    SemaphoreHandle_t mutexInstance;
    SemaphoreHandle_t mutexInterface;
    EventGroupHandle_t eventGroup;
    bool initialized;
    float lastTemperature;
    
    static constexpr EventBits_t INIT_COMPLETE_BIT = BIT0;
    static constexpr EventBits_t DATA_READY_BIT = BIT1;

public:
    TemperatureSensor() : initialized(false), lastTemperature(0.0f) {
        mutexInstance = xSemaphoreCreateMutex();
        mutexInterface = xSemaphoreCreateMutex();
        eventGroup = xEventGroupCreate();
    }
    
    ~TemperatureSensor() {
        if (mutexInstance) vSemaphoreDelete(mutexInstance);
        if (mutexInterface) vSemaphoreDelete(mutexInterface);
        if (eventGroup) vEventGroupDelete(eventGroup);
    }
    
    DeviceResult<void> initialize() override {
        if (xSemaphoreTake(mutexInstance, portMAX_DELAY) == pdTRUE) {
            // Initialize hardware (I2C, SPI, etc.)
            // ...
            initialized = true;
            xEventGroupSetBits(eventGroup, INIT_COMPLETE_BIT);
            xSemaphoreGive(mutexInstance);
            return {DeviceError::SUCCESS};
        }
        return {DeviceError::MUTEX_ERROR};
    }
    
    bool isInitialized() const noexcept override {
        return initialized;
    }
    
    ErrorCode waitForInitialization(TickType_t xTicksToWait) override {
        EventBits_t bits = xEventGroupWaitBits(
            eventGroup,
            INIT_COMPLETE_BIT,
            pdFALSE,
            pdTRUE,
            xTicksToWait
        );
        return (bits & INIT_COMPLETE_BIT) ? ErrorCode::SUCCESS : ErrorCode::TIMEOUT;
    }
    
    DeviceResult<void> requestData() override {
        if (!initialized) return {DeviceError::NOT_INITIALIZED};
        
        if (xSemaphoreTake(mutexInterface, portMAX_DELAY) == pdTRUE) {
            // Start async data acquisition
            // ...
            xSemaphoreGive(mutexInterface);
            return {DeviceError::SUCCESS};
        }
        return {DeviceError::TIMEOUT};
    }
    
    ErrorCode waitForData(TickType_t xTicksToWait) override {
        EventBits_t bits = xEventGroupWaitBits(
            eventGroup,
            DATA_READY_BIT,
            pdTRUE,  // Clear bit on exit
            pdTRUE,
            xTicksToWait
        );
        return (bits & DATA_READY_BIT) ? ErrorCode::SUCCESS : ErrorCode::TIMEOUT;
    }
    
    DeviceResult<void> processData() override {
        if (xSemaphoreTake(mutexInstance, portMAX_DELAY) == pdTRUE) {
            // Process raw data into temperature
            lastTemperature = 25.5f; // Example
            xSemaphoreGive(mutexInstance);
            return {DeviceError::SUCCESS};
        }
        return {DeviceError::MUTEX_ERROR};
    }
    
    DeviceResult<std::vector<float>> getData(DeviceDataType dataType) override {
        if (dataType != DeviceDataType::TEMPERATURE) {
            return {DeviceError::INVALID_PARAMETER};
        }
        
        if (xSemaphoreTake(mutexInstance, portMAX_DELAY) == pdTRUE) {
            std::vector<float> values;
            values.push_back(lastTemperature);
            xSemaphoreGive(mutexInstance);
            return {DeviceError::SUCCESS, values};
        }
        
        return {DeviceError::MUTEX_ERROR};
    }
    
    // ... implement remaining pure virtual methods
};
```

### Modern Error Handling with DeviceResult (v2.0.0+)

The library now provides a type-safe `DeviceResult<T>` template for better error handling:

```cpp
// Example implementation using DeviceResult
class ModernSensor : public IDeviceInstance {
    DeviceResult<float> readTemperature() {
        if (!initialized) {
            return {DeviceError::NOT_INITIALIZED};
        }
        
        if (xSemaphoreTake(mutexInterface, pdMS_TO_TICKS(100)) != pdTRUE) {
            return {DeviceError::TIMEOUT};
        }
        
        float temp = readSensorValue();
        xSemaphoreGive(mutexInterface);
        
        return {DeviceError::SUCCESS, temp};
    }
    
    DeviceResult<void> configure(uint8_t mode) {
        if (mode > 3) {
            return {DeviceError::INVALID_PARAMETER};
        }
        
        // Configuration logic...
        return {DeviceError::SUCCESS};
    }
};

// Usage
ModernSensor sensor;
auto result = sensor.readTemperature();

if (result.isOk()) {
    IDEV_LOG_I("Temperature: %.2f°C", result.value);
} else {
    IDEV_LOG_E("Failed to read: %s", 
               IDeviceInstance::deviceErrorToString(result.error));
}

// Or use the bool operator
if (auto config = sensor.configure(2)) {
    IDEV_LOG_I("Configuration successful");
}
```

### Using the Device

```cpp
void temperatureTask(void* pvParameters) {
    auto* sensor = static_cast<IDeviceInstance*>(pvParameters);
    
    // Modern API - wait for initialization with timeout
    auto initResult = sensor->waitForInitializationComplete(pdMS_TO_TICKS(5000));
    if (!initResult) {
        IDEV_LOG_E("Initialization failed: %s", 
                   IDeviceInstance::deviceErrorToString(initResult.error));
        vTaskDelete(NULL);
        return;
    }
    
    // Initialize and wait
    sensor->initialize();
    ErrorCode err = sensor->waitForInitialization(pdMS_TO_TICKS(5000));
    if (err != IDeviceInstance::ErrorCode::SUCCESS) {
        ESP_LOGE("TASK", "Init failed: %s", IDeviceInstance::errorToString(err));
        vTaskDelete(NULL);
    }
    
    while (true) {
        // Request data
        if (sensor->requestData()) {
            // Wait with timeout
            err = sensor->waitForData(pdMS_TO_TICKS(1000));
            if (err == IDeviceInstance::ErrorCode::SUCCESS) {
                sensor->processData();
                
                // Get temperature
                auto result = sensor->getData(IDeviceInstance::DeviceDataType::TEMPERATURE);
                if (result.success && !result.values.empty()) {
                    ESP_LOGI("TASK", "Temperature: %.2f°C", result.values[0]);
                }
            } else {
                ESP_LOGW("TASK", "Data timeout: %s", IDeviceInstance::errorToString(err));
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(30000)); // Read every 30 seconds
    }
}

void app_main() {
    TemperatureSensor sensor;
    xTaskCreate(temperatureTask, "temp_task", 4096, &sensor, 5, NULL);
}
```

### Advanced Features

#### Custom Actions

```cpp
enum CustomActions {
    ACTION_CALIBRATE = 1,
    ACTION_RESET = 2,
    ACTION_SET_INTERVAL = 3
};

ErrorCode performAction(int actionId, int actionParam) override {
    switch (actionId) {
        case ACTION_CALIBRATE:
            return calibrateSensor();
        case ACTION_SET_INTERVAL:
            return setReadInterval(actionParam);
        default:
            return ErrorCode::NOT_SUPPORTED;
    }
}
```

#### Multiple Data Types

```cpp
class MultiSensor : public IDeviceInstance {
    DataResult getData(DeviceDataType dataType) override {
        DataResult result = {false, {}};
        
        if (xSemaphoreTake(mutexInstance, portMAX_DELAY) == pdTRUE) {
            switch (dataType) {
                case DeviceDataType::TEMPERATURE:
                    result.success = true;
                    result.values.push_back(temperature);
                    break;
                case DeviceDataType::HUMIDITY:
                    result.success = true;
                    result.values.push_back(humidity);
                    break;
                default:
                    break;
            }
            xSemaphoreGive(mutexInstance);
        }
        
        return result;
    }
};
```

### Logging Configuration (v1.5.0+)

This library supports flexible logging configuration with true zero overhead for debug logging in production builds.

```cpp
// Using logging in your implementation
class MySensor : public IDeviceInstance {
    void initialize() override {
        IDEV_LOG_I("Initializing sensor");
        
        if (!hardwareReady) {
            IDEV_LOG_E("Hardware not ready");
            return;
        }
        
        IDEV_LOG_D("Configuration complete");  // Suppressed without IDEVICEINSTANCE_DEBUG
        IDEV_LOG_V("Detailed trace info");     // Suppressed without IDEVICEINSTANCE_DEBUG
        
        // Advanced debug features
        IDEV_TIME_START();
        // ... initialization code ...
        IDEV_TIME_END("sensor initialization");
        
        IDEV_LOG_STATE("uninitialized", "ready");
    }
    
    void processData(uint8_t* buffer, size_t len) {
        IDEV_DUMP_DATA("Received data", buffer, len);
        // ... process data ...
    }
};
```

#### Using ESP-IDF Logging (Default)

No configuration needed. The library will use ESP-IDF logging:

```cpp
#include "IDeviceInstance.h"

// Your implementation will log to ESP-IDF
class MyDevice : public IDeviceInstance {
    // ... implementation ...
};
```

#### Using Custom Logger

Define `USE_CUSTOM_LOGGER` in your build flags:

```ini
build_flags = -DUSE_CUSTOM_LOGGER
```

#### Debug Logging

To enable debug/verbose logging for this library:

```ini
build_flags = -DIDEVICEINSTANCE_DEBUG
```

#### Complete Example

```ini
[env:debug]
build_flags = 
    -DUSE_CUSTOM_LOGGER     ; Use custom logger
    -DIDEVICEINSTANCE_DEBUG ; Enable debug for this library
```

#### Available Logging Macros

##### Basic Logging
- `IDEV_LOG_E(...)` - Error level (always enabled)
- `IDEV_LOG_W(...)` - Warning level (always enabled)
- `IDEV_LOG_I(...)` - Info level (always enabled)
- `IDEV_LOG_D(...)` - Debug level (only with IDEVICEINSTANCE_DEBUG)
- `IDEV_LOG_V(...)` - Verbose level (only with IDEVICEINSTANCE_DEBUG)

##### Advanced Debug Features
When `IDEVICEINSTANCE_DEBUG` is defined:
- `IDEV_TIME_START()` / `IDEV_TIME_END(msg)` - Performance timing
- `IDEV_LOG_STATE(from, to)` - State transition logging
- `IDEV_DUMP_DATA(msg, data, len)` - Data buffer dumps

#### Production vs Debug Builds

##### Production Build (default)
- Only Error, Warning, and Info logs are compiled
- Debug and Verbose logs are completely removed (zero overhead)
- No performance impact from debug code

##### Debug Build (with IDEVICEINSTANCE_DEBUG)
- All log levels are active
- Performance timing available
- State transitions logged
- Data dumps enabled

#### Benefits

- **True Zero Overhead**: Debug logs completely removed in production
- **Flexible Backend**: Works with ESP-IDF or custom Logger
- **Granular Control**: Enable debug per library
- **Advanced Debugging**: Built-in timing, state, and data dump helpers
- **Production Ready**: No debug overhead in release builds

## Best Practices

1. **Always Check Initialization**: Verify device is initialized before operations
2. **Use Timeouts**: Prefer timeout versions of wait methods to avoid deadlocks
3. **Handle Errors**: Check error codes and handle failures gracefully
4. **Mutex Discipline**: Always acquire mutexes with timeouts in production
5. **Event Group Bits**: Document which bits your implementation uses
6. **Memory Management**: Ensure proper cleanup in destructors

## Thread Safety Guidelines

- All public methods must be thread-safe
- Use `mutexInstance` for protecting instance data
- Use `mutexInterface` for protecting shared hardware resources
- Event groups are thread-safe by design
- Consider priority inheritance mutexes for real-time requirements

## Performance Considerations

- Methods marked `noexcept` are optimized for performance
- `constexpr` methods enable compile-time optimizations
- Minimize mutex hold times
- Use event groups for efficient waiting
- Consider DMA for high-frequency data acquisition

## API Reference

See the header file documentation for detailed API reference. The interface is fully documented with Doxygen comments.

## Contributing

1. Follow the existing code style
2. Ensure thread safety in all implementations
3. Add comprehensive error checking
4. Update documentation for new features
5. Test with multiple concurrent tasks

## Testing

The library includes comprehensive unit tests and utilities for testing implementations:

### Running Tests

For PlatformIO projects:

```ini
[env:test]
platform = espressif32
framework = arduino
lib_deps = 
    Unity
    IDeviceInstance
test_build_src = yes
```

Run tests with:
```bash
pio test
```

### Using Test Utilities

The library provides test utilities for your implementations:

```cpp
#include "test/DeviceTestUtils.h"
#include "MyTemperatureSensor.h" // Your implementation

void test_my_sensor() {
    MyTemperatureSensor sensor;
    
    // Run comprehensive test suite
    std::vector<IDeviceInstance::DeviceDataType> supportedTypes = {
        IDeviceInstance::DeviceDataType::TEMPERATURE,
        IDeviceInstance::DeviceDataType::HUMIDITY
    };
    
    DeviceTestUtils::runComprehensiveTests(&sensor, supportedTypes);
}
```

### Mock Implementation

Use `MockDeviceInstance` for testing code that depends on IDeviceInstance:

```cpp
#include "test/MockDeviceInstance.h"

void test_my_application() {
    // Create mock with 10ms init delay, 20ms data delay
    MockDeviceInstance mockDevice(10, 20);
    
    // Configure test data
    mockDevice.setTestData(IDeviceInstance::DeviceDataType::TEMPERATURE, {25.5f});
    
    // Use in your application
    MyApplication app(&mockDevice);
    app.run();
    
    // Verify interactions
    auto actions = mockDevice.getPerformedActions();
    TEST_ASSERT_EQUAL(1, actions.size());
}
```

### Test Coverage

The provided tests cover:
- Initialization and state management
- Data acquisition flow
- Thread safety and concurrent access
- Error handling and timeouts
- Callback/event notifications
- Resource management
- Performance characteristics

## License

This library is released under the MIT License. See LICENSE file for details.

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for version history and changes.