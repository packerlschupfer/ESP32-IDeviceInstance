/**
 * @file test_IDeviceInstance.cpp
 * @brief Comprehensive unit tests for IDeviceInstance interface
 * 
 * These tests verify the contract and expected behavior of IDeviceInstance
 * implementations using the MockDeviceInstance test double.
 */

#include <unity.h>
#include "MockDeviceInstance.h"
#include <vector>
#include <atomic>

// Test fixtures
static MockDeviceInstance* device = nullptr;
static std::atomic<int> callbackCounter(0);
static std::vector<IDeviceInstance::EventNotification> receivedNotifications;

void setUp() {
    // Create fresh mock instance for each test
    device = new MockDeviceInstance(10, 20); // 10ms init, 20ms data delay
    callbackCounter = 0;
    receivedNotifications.clear();
}

void tearDown() {
    delete device;
    device = nullptr;
}

// Helper callback for testing
void testCallback(const IDeviceInstance::EventNotification& notification) {
    callbackCounter++;
    receivedNotifications.push_back(notification);
}

// Test Cases

void test_initialization_state() {
    TEST_ASSERT_FALSE(device->isInitialized());
    
    device->initialize();
    
    TEST_ASSERT_TRUE(device->isInitialized());
}

void test_initialization_wait_with_timeout() {
    // Start initialization in background
    xTaskCreate([](void* param) {
        vTaskDelay(pdMS_TO_TICKS(50));
        static_cast<MockDeviceInstance*>(param)->initialize();
        vTaskDelete(nullptr);
    }, "InitTask", 2048, device, 1, nullptr);
    
    // Should timeout before initialization
    IDeviceInstance::ErrorCode result = device->waitForInitialization(pdMS_TO_TICKS(30));
    TEST_ASSERT_EQUAL(IDeviceInstance::ErrorCode::TIMEOUT, result);
    
    // Should succeed with longer timeout
    result = device->waitForInitialization(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(IDeviceInstance::ErrorCode::SUCCESS, result);
    TEST_ASSERT_TRUE(device->isInitialized());
}

void test_data_acquisition_flow() {
    // Must initialize first
    TEST_ASSERT_FALSE(device->requestData());
    
    device->initialize();
    TEST_ASSERT_TRUE(device->isInitialized());
    
    // Request data
    TEST_ASSERT_TRUE(device->requestData());
    
    // Wait for data
    IDeviceInstance::ErrorCode result = device->waitForData(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(IDeviceInstance::ErrorCode::SUCCESS, result);
    
    // Process data
    device->processData();
    
    // Verify no data without configuration
    IDeviceInstance::DataResult dataResult = device->getData(IDeviceInstance::DeviceDataType::TEMPERATURE);
    TEST_ASSERT_FALSE(dataResult.success);
    TEST_ASSERT_TRUE(dataResult.values.empty());
}

void test_data_retrieval_with_values() {
    device->initialize();
    
    // Configure test data
    std::vector<float> testValues = {25.5f, 26.0f, 25.8f};
    device->setTestData(IDeviceInstance::DeviceDataType::TEMPERATURE, testValues);
    
    // Acquire and process
    device->requestData();
    device->waitForData(pdMS_TO_TICKS(50));
    device->processData();
    
    // Retrieve data
    IDeviceInstance::DataResult result = device->getData(IDeviceInstance::DeviceDataType::TEMPERATURE);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL(testValues.size(), result.values.size());
    
    for (size_t i = 0; i < testValues.size(); i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, testValues[i], result.values[i]);
    }
}

void test_multiple_data_types() {
    device->initialize();
    
    // Configure multiple data types
    device->setTestData(IDeviceInstance::DeviceDataType::TEMPERATURE, {22.5f});
    device->setTestData(IDeviceInstance::DeviceDataType::HUMIDITY, {65.0f});
    device->setTestData(IDeviceInstance::DeviceDataType::PRESSURE, {1013.25f});
    
    device->requestData();
    device->waitForData(pdMS_TO_TICKS(50));
    device->processData();
    
    // Test each type
    auto tempResult = device->getData(IDeviceInstance::DeviceDataType::TEMPERATURE);
    TEST_ASSERT_TRUE(tempResult.success);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.5f, tempResult.values[0]);
    
    auto humResult = device->getData(IDeviceInstance::DeviceDataType::HUMIDITY);
    TEST_ASSERT_TRUE(humResult.success);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 65.0f, humResult.values[0]);
    
    auto pressResult = device->getData(IDeviceInstance::DeviceDataType::PRESSURE);
    TEST_ASSERT_TRUE(pressResult.success);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1013.25f, pressResult.values[0]);
}

void test_error_injection() {
    device->initialize();
    
    // Inject error for next operation
    device->injectError(IDeviceInstance::ErrorCode::COMMUNICATION_ERROR);
    
    // Request should fail
    TEST_ASSERT_FALSE(device->requestData());
}

void test_perform_action() {
    // Should fail when not initialized
    IDeviceInstance::ErrorCode result = device->performAction(1, 100);
    TEST_ASSERT_EQUAL(IDeviceInstance::ErrorCode::NOT_INITIALIZED, result);
    
    device->initialize();
    
    // Should succeed when initialized
    result = device->performAction(1, 100);
    TEST_ASSERT_EQUAL(IDeviceInstance::ErrorCode::SUCCESS, result);
    
    result = device->performAction(2, 200);
    TEST_ASSERT_EQUAL(IDeviceInstance::ErrorCode::SUCCESS, result);
    
    // Verify actions were recorded
    auto actions = device->getPerformedActions();
    TEST_ASSERT_EQUAL(2, actions.size());
    TEST_ASSERT_EQUAL(1, actions[0].first);
    TEST_ASSERT_EQUAL(100, actions[0].second);
    TEST_ASSERT_EQUAL(2, actions[1].first);
    TEST_ASSERT_EQUAL(200, actions[1].second);
}

void test_callbacks_basic() {
    // Register callback
    IDeviceInstance::ErrorCode result = device->registerCallback(testCallback);
    TEST_ASSERT_EQUAL(IDeviceInstance::ErrorCode::SUCCESS, result);
    TEST_ASSERT_EQUAL(1, device->getCallbackCount());
    
    // Initialize should trigger callback
    device->initialize();
    vTaskDelay(pdMS_TO_TICKS(50)); // Allow callback task to run
    
    TEST_ASSERT_EQUAL(1, callbackCounter.load());
    TEST_ASSERT_EQUAL(1, receivedNotifications.size());
    TEST_ASSERT_EQUAL(IDeviceInstance::EventType::INITIALIZED, receivedNotifications[0].type);
    TEST_ASSERT_EQUAL(IDeviceInstance::ErrorCode::SUCCESS, receivedNotifications[0].error);
}

void test_callbacks_multiple_events() {
    device->registerCallback(testCallback);
    device->initialize();
    
    // Request data should trigger DATA_READY
    device->requestData();
    vTaskDelay(pdMS_TO_TICKS(50)); // Wait for async operation
    
    // Perform action should trigger STATE_CHANGED
    device->performAction(42, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Should have received 3 notifications (INITIALIZED, DATA_READY, STATE_CHANGED)
    TEST_ASSERT_EQUAL(3, callbackCounter.load());
    TEST_ASSERT_EQUAL(3, receivedNotifications.size());
    
    // Verify notification types
    TEST_ASSERT_EQUAL(IDeviceInstance::EventType::INITIALIZED, receivedNotifications[0].type);
    TEST_ASSERT_EQUAL(IDeviceInstance::EventType::DATA_READY, receivedNotifications[1].type);
    TEST_ASSERT_EQUAL(IDeviceInstance::EventType::STATE_CHANGED, receivedNotifications[2].type);
    TEST_ASSERT_EQUAL(42, receivedNotifications[2].customData);
}

void test_event_notification_control() {
    device->registerCallback(testCallback);
    
    // Disable initialization events
    IDeviceInstance::ErrorCode result = device->setEventNotification(
        IDeviceInstance::EventType::INITIALIZED, false
    );
    TEST_ASSERT_EQUAL(IDeviceInstance::ErrorCode::SUCCESS, result);
    
    device->initialize();
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Should not receive initialization callback
    TEST_ASSERT_EQUAL(0, callbackCounter.load());
    
    // Enable and trigger data event
    device->setEventNotification(IDeviceInstance::EventType::DATA_READY, true);
    device->requestData();
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Should receive data ready callback
    TEST_ASSERT_EQUAL(1, callbackCounter.load());
    TEST_ASSERT_EQUAL(IDeviceInstance::EventType::DATA_READY, receivedNotifications[0].type);
}

void test_unregister_callbacks() {
    // Register multiple callbacks
    device->registerCallback(testCallback);
    device->registerCallback(testCallback);
    TEST_ASSERT_EQUAL(2, device->getCallbackCount());
    
    // Unregister all
    IDeviceInstance::ErrorCode result = device->unregisterCallbacks();
    TEST_ASSERT_EQUAL(IDeviceInstance::ErrorCode::SUCCESS, result);
    TEST_ASSERT_EQUAL(0, device->getCallbackCount());
    
    // Events should not trigger callbacks
    device->initialize();
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(0, callbackCounter.load());
}

void test_mutex_handles() {
    // Verify mutexes are created
    TEST_ASSERT_NOT_NULL(device->getMutexInstance());
    TEST_ASSERT_NOT_NULL(device->getMutexInterface());
    TEST_ASSERT_NOT_EQUAL(device->getMutexInstance(), device->getMutexInterface());
}

void test_event_group_handle() {
    TEST_ASSERT_NOT_NULL(device->getEventGroup());
}

void test_concurrent_access() {
    device->initialize();
    device->setTestData(IDeviceInstance::DeviceDataType::TEMPERATURE, {25.0f});
    
    std::atomic<int> successCount(0);
    const int numTasks = 5;
    
    // Create multiple tasks accessing the device
    for (int i = 0; i < numTasks; i++) {
        xTaskCreate([](void* param) {
            auto* counter = static_cast<std::atomic<int>*>(param);
            MockDeviceInstance* dev = device;
            
            // Perform concurrent operations
            for (int j = 0; j < 10; j++) {
                if (dev->requestData()) {
                    if (dev->waitForData(pdMS_TO_TICKS(100)) == IDeviceInstance::ErrorCode::SUCCESS) {
                        dev->processData();
                        auto result = dev->getData(IDeviceInstance::DeviceDataType::TEMPERATURE);
                        if (result.success) {
                            (*counter)++;
                        }
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            
            vTaskDelete(nullptr);
        }, "ConcurrentTask", 2048, &successCount, 1, nullptr);
    }
    
    // Wait for all tasks to complete
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // All operations should have succeeded
    TEST_ASSERT_EQUAL(numTasks * 10, successCount.load());
}

void test_error_to_string() {
    // Test all error codes
    TEST_ASSERT_EQUAL_STRING("Success", 
        IDeviceInstance::errorToString(IDeviceInstance::ErrorCode::SUCCESS));
    TEST_ASSERT_EQUAL_STRING("Not initialized", 
        IDeviceInstance::errorToString(IDeviceInstance::ErrorCode::NOT_INITIALIZED));
    TEST_ASSERT_EQUAL_STRING("Timeout", 
        IDeviceInstance::errorToString(IDeviceInstance::ErrorCode::TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("Invalid parameter", 
        IDeviceInstance::errorToString(IDeviceInstance::ErrorCode::INVALID_PARAMETER));
    TEST_ASSERT_EQUAL_STRING("Communication error", 
        IDeviceInstance::errorToString(IDeviceInstance::ErrorCode::COMMUNICATION_ERROR));
    TEST_ASSERT_EQUAL_STRING("Data not ready", 
        IDeviceInstance::errorToString(IDeviceInstance::ErrorCode::DATA_NOT_READY));
    TEST_ASSERT_EQUAL_STRING("Mutex error", 
        IDeviceInstance::errorToString(IDeviceInstance::ErrorCode::MUTEX_ERROR));
    TEST_ASSERT_EQUAL_STRING("Memory error", 
        IDeviceInstance::errorToString(IDeviceInstance::ErrorCode::MEMORY_ERROR));
    TEST_ASSERT_EQUAL_STRING("Device busy", 
        IDeviceInstance::errorToString(IDeviceInstance::ErrorCode::DEVICE_BUSY));
    TEST_ASSERT_EQUAL_STRING("Not supported", 
        IDeviceInstance::errorToString(IDeviceInstance::ErrorCode::NOT_SUPPORTED));
    TEST_ASSERT_EQUAL_STRING("Unknown error", 
        IDeviceInstance::errorToString(IDeviceInstance::ErrorCode::UNKNOWN_ERROR));
    
    // Test invalid error code
    TEST_ASSERT_EQUAL_STRING("Invalid error code", 
        IDeviceInstance::errorToString(static_cast<IDeviceInstance::ErrorCode>(999)));
}

void test_is_valid_data_type() {
    // Valid types
    TEST_ASSERT_TRUE(IDeviceInstance::isValidDataType(
        static_cast<int>(IDeviceInstance::DeviceDataType::TEMPERATURE)));
    TEST_ASSERT_TRUE(IDeviceInstance::isValidDataType(
        static_cast<int>(IDeviceInstance::DeviceDataType::HUMIDITY)));
    TEST_ASSERT_TRUE(IDeviceInstance::isValidDataType(
        static_cast<int>(IDeviceInstance::DeviceDataType::PRESSURE)));
    TEST_ASSERT_TRUE(IDeviceInstance::isValidDataType(
        static_cast<int>(IDeviceInstance::DeviceDataType::RELAY_STATE)));
    
    // Invalid types
    TEST_ASSERT_FALSE(IDeviceInstance::isValidDataType(-1));
    TEST_ASSERT_FALSE(IDeviceInstance::isValidDataType(
        static_cast<int>(IDeviceInstance::DeviceDataType::NUM_TYPES)));
    TEST_ASSERT_FALSE(IDeviceInstance::isValidDataType(999));
}

void test_to_underlying_type() {
    // Test enum conversion
    auto tempValue = IDeviceInstance::toUnderlyingType(IDeviceInstance::DeviceDataType::TEMPERATURE);
    TEST_ASSERT_EQUAL(0, tempValue);
    
    auto errorValue = IDeviceInstance::toUnderlyingType(IDeviceInstance::ErrorCode::TIMEOUT);
    TEST_ASSERT_EQUAL(2, errorValue);
    
    auto eventValue = IDeviceInstance::toUnderlyingType(IDeviceInstance::EventType::DATA_READY);
    TEST_ASSERT_EQUAL(1, eventValue);
}

// Test runner
void runIDeviceInstanceTests() {
    UNITY_BEGIN();
    
    // Basic functionality tests
    RUN_TEST(test_initialization_state);
    RUN_TEST(test_initialization_wait_with_timeout);
    RUN_TEST(test_data_acquisition_flow);
    RUN_TEST(test_data_retrieval_with_values);
    RUN_TEST(test_multiple_data_types);
    
    // Error handling tests
    RUN_TEST(test_error_injection);
    RUN_TEST(test_error_to_string);
    
    // Action tests
    RUN_TEST(test_perform_action);
    
    // Callback tests
    RUN_TEST(test_callbacks_basic);
    RUN_TEST(test_callbacks_multiple_events);
    RUN_TEST(test_event_notification_control);
    RUN_TEST(test_unregister_callbacks);
    
    // Resource tests
    RUN_TEST(test_mutex_handles);
    RUN_TEST(test_event_group_handle);
    
    // Concurrency tests
    RUN_TEST(test_concurrent_access);
    
    // Utility tests
    RUN_TEST(test_is_valid_data_type);
    RUN_TEST(test_to_underlying_type);
    
    UNITY_END();
}

// For PlatformIO
#ifdef ARDUINO
void setup() {
    delay(2000); // Wait for serial
    runIDeviceInstanceTests();
}

void loop() {
    // Empty
}
#else
// For native testing
int main() {
    runIDeviceInstanceTests();
    return 0;
}
#endif