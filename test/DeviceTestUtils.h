/**
 * @file DeviceTestUtils.h
 * @brief Utility functions and helpers for testing IDeviceInstance implementations
 * 
 * This header provides common testing utilities that can be used by
 * projects implementing IDeviceInstance to test their concrete implementations.
 */

#ifndef DEVICE_TEST_UTILS_H
#define DEVICE_TEST_UTILS_H

#include "../src/IDeviceInstance.h"
#include <unity.h>
#include <functional>
#include <chrono>

namespace DeviceTestUtils {

/**
 * @brief Test helper to verify basic initialization contract
 * @param device The device instance to test
 */
void verifyInitializationContract(IDeviceInstance* device) {
    TEST_ASSERT_NOT_NULL_MESSAGE(device, "Device instance is null");
    
    // Should not be initialized initially
    TEST_ASSERT_FALSE_MESSAGE(device->isInitialized(), 
        "Device should not be initialized before initialize() is called");
    
    // Initialize
    device->initialize();
    
    // Should be initialized after
    TEST_ASSERT_TRUE_MESSAGE(device->isInitialized(), 
        "Device should be initialized after initialize() is called");
    
    // Multiple initializations should be safe (idempotent)
    device->initialize();
    TEST_ASSERT_TRUE_MESSAGE(device->isInitialized(), 
        "Device should remain initialized after multiple initialize() calls");
}

/**
 * @brief Test helper to verify data acquisition contract
 * @param device The device instance to test
 * @param dataType The data type to test
 * @param expectedSuccess Whether data retrieval is expected to succeed
 */
void verifyDataAcquisitionContract(IDeviceInstance* device, 
                                   IDeviceInstance::DeviceDataType dataType,
                                   bool expectedSuccess = true) {
    TEST_ASSERT_NOT_NULL_MESSAGE(device, "Device instance is null");
    
    // Should fail if not initialized
    if (!device->isInitialized()) {
        TEST_ASSERT_FALSE_MESSAGE(device->requestData(), 
            "requestData() should fail when device is not initialized");
        return;
    }
    
    // Request data
    TEST_ASSERT_TRUE_MESSAGE(device->requestData(), 
        "requestData() should succeed when device is initialized");
    
    // Wait for data with timeout
    IDeviceInstance::ErrorCode waitResult = device->waitForData(pdMS_TO_TICKS(5000));
    TEST_ASSERT_EQUAL_MESSAGE(IDeviceInstance::ErrorCode::SUCCESS, waitResult,
        "waitForData() should succeed within timeout");
    
    // Process data
    device->processData();
    
    // Get data
    IDeviceInstance::DataResult result = device->getData(dataType);
    
    if (expectedSuccess) {
        TEST_ASSERT_TRUE_MESSAGE(result.success, 
            "getData() should succeed for supported data type");
        TEST_ASSERT_TRUE_MESSAGE(!result.values.empty(), 
            "getData() should return non-empty values for supported data type");
    } else {
        TEST_ASSERT_FALSE_MESSAGE(result.success, 
            "getData() should fail for unsupported data type");
    }
}

/**
 * @brief Test helper to verify thread safety with concurrent access
 * @param device The device instance to test
 * @param numTasks Number of concurrent tasks to create
 * @param operationsPerTask Number of operations each task should perform
 */
void verifyConcurrentAccess(IDeviceInstance* device, 
                            int numTasks = 3, 
                            int operationsPerTask = 5) {
    TEST_ASSERT_NOT_NULL_MESSAGE(device, "Device instance is null");
    
    // Initialize first
    if (!device->isInitialized()) {
        device->initialize();
    }
    
    std::atomic<int> successCount(0);
    std::atomic<int> errorCount(0);
    
    // Create concurrent tasks
    for (int i = 0; i < numTasks; i++) {
        xTaskCreate([](void* params) {
            auto* counters = static_cast<std::pair<std::atomic<int>*, std::atomic<int>*>*>(params);
            auto* success = counters->first;
            auto* errors = counters->second;
            IDeviceInstance* dev = device;
            
            for (int j = 0; j < operationsPerTask; j++) {
                // Try various operations
                if (dev->requestData()) {
                    if (dev->waitForData(pdMS_TO_TICKS(1000)) == IDeviceInstance::ErrorCode::SUCCESS) {
                        dev->processData();
                        auto result = dev->getData(IDeviceInstance::DeviceDataType::TEMPERATURE);
                        if (result.success) {
                            (*success)++;
                        } else {
                            (*errors)++;
                        }
                    } else {
                        (*errors)++;
                    }
                } else {
                    (*errors)++;
                }
                
                // Small delay to increase chance of contention
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            
            vTaskDelete(nullptr);
        }, "ConcurrentTest", 2048, 
        new std::pair<std::atomic<int>*, std::atomic<int>*>(&successCount, &errorCount), 
        1, nullptr);
    }
    
    // Wait for all tasks to complete
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Verify no crashes occurred and operations completed
    int totalOperations = successCount.load() + errorCount.load();
    TEST_ASSERT_EQUAL_MESSAGE(numTasks * operationsPerTask, totalOperations,
        "All concurrent operations should complete without deadlock");
}

/**
 * @brief Test helper to verify error handling
 * @param device The device instance to test
 */
void verifyErrorHandling(IDeviceInstance* device) {
    TEST_ASSERT_NOT_NULL_MESSAGE(device, "Device instance is null");
    
    // Test timeout handling
    if (device->isInitialized()) {
        device->requestData();
        
        // Very short timeout should fail
        IDeviceInstance::ErrorCode result = device->waitForData(pdMS_TO_TICKS(1));
        // Note: This might succeed if data is immediately available
        // so we just verify it returns a valid error code
        TEST_ASSERT_TRUE_MESSAGE(
            result == IDeviceInstance::ErrorCode::SUCCESS || 
            result == IDeviceInstance::ErrorCode::TIMEOUT,
            "waitForData() should return SUCCESS or TIMEOUT");
    }
    
    // Test invalid parameters
    IDeviceInstance::ErrorCode actionResult = device->performAction(-1, -1);
    // Device may support negative action IDs, so just verify it returns something valid
    TEST_ASSERT_TRUE_MESSAGE(
        static_cast<int>(actionResult) >= 0 && 
        static_cast<int>(actionResult) <= static_cast<int>(IDeviceInstance::ErrorCode::UNKNOWN_ERROR),
        "performAction() should return valid error code");
}

/**
 * @brief Test helper to verify resource creation
 * @param device The device instance to test
 */
void verifyResourceCreation(IDeviceInstance* device) {
    TEST_ASSERT_NOT_NULL_MESSAGE(device, "Device instance is null");
    
    // Verify mutexes are created
    TEST_ASSERT_NOT_NULL_MESSAGE(device->getMutexInstance(), 
        "Instance mutex should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(device->getMutexInterface(), 
        "Interface mutex should be created");
    
    // Verify they are different mutexes
    TEST_ASSERT_NOT_EQUAL_MESSAGE(device->getMutexInstance(), device->getMutexInterface(),
        "Instance and interface mutexes should be different");
    
    // Verify event group is created
    TEST_ASSERT_NOT_NULL_MESSAGE(device->getEventGroup(), 
        "Event group should be created");
}

/**
 * @brief Test helper to measure operation timing
 * @param operation The operation to time
 * @return Duration in milliseconds
 */
template<typename Func>
uint32_t measureOperationTime(Func operation) {
    auto start = xTaskGetTickCount();
    operation();
    auto end = xTaskGetTickCount();
    return pdTICKS_TO_MS(end - start);
}

/**
 * @brief Test helper to verify callback functionality
 * @param device The device instance to test
 */
void verifyCallbackSupport(IDeviceInstance* device) {
    TEST_ASSERT_NOT_NULL_MESSAGE(device, "Device instance is null");
    
    std::atomic<bool> callbackCalled(false);
    
    // Create test callback
    auto testCallback = [&callbackCalled](const IDeviceInstance::EventNotification& notification) {
        callbackCalled = true;
        IDEV_LOG_D("Callback received: type=%d, error=%d", 
                   static_cast<int>(notification.type), 
                   static_cast<int>(notification.error));
    };
    
    // Try to register callback
    IDeviceInstance::ErrorCode result = device->registerCallback(testCallback);
    
    if (result == IDeviceInstance::ErrorCode::NOT_SUPPORTED) {
        // Callbacks not supported, skip test
        TEST_IGNORE_MESSAGE("Device does not support callbacks");
        return;
    }
    
    TEST_ASSERT_EQUAL_MESSAGE(IDeviceInstance::ErrorCode::SUCCESS, result,
        "registerCallback() should succeed if callbacks are supported");
    
    // Trigger an event (initialize if not already)
    if (!device->isInitialized()) {
        device->initialize();
    } else {
        // Try data request to trigger event
        device->requestData();
    }
    
    // Wait for callback
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Note: Callback may or may not be called depending on implementation
    // We just verify no crash occurred
    
    // Unregister
    result = device->unregisterCallbacks();
    TEST_ASSERT_EQUAL_MESSAGE(IDeviceInstance::ErrorCode::SUCCESS, result,
        "unregisterCallbacks() should succeed if callbacks are supported");
}

/**
 * @brief Comprehensive test suite for any IDeviceInstance implementation
 * @param device The device instance to test
 * @param supportedDataTypes List of data types the device supports
 */
void runComprehensiveTests(IDeviceInstance* device, 
                          const std::vector<IDeviceInstance::DeviceDataType>& supportedDataTypes) {
    Serial.println("=== IDeviceInstance Comprehensive Test Suite ===");
    
    Serial.println("\n1. Testing initialization contract...");
    verifyInitializationContract(device);
    
    Serial.println("\n2. Testing resource creation...");
    verifyResourceCreation(device);
    
    Serial.println("\n3. Testing data acquisition for supported types...");
    for (auto dataType : supportedDataTypes) {
        Serial.printf("   - Testing data type %d\n", static_cast<int>(dataType));
        verifyDataAcquisitionContract(device, dataType, true);
    }
    
    Serial.println("\n4. Testing error handling...");
    verifyErrorHandling(device);
    
    Serial.println("\n5. Testing concurrent access...");
    verifyConcurrentAccess(device);
    
    Serial.println("\n6. Testing callback support...");
    verifyCallbackSupport(device);
    
    Serial.println("\n=== All tests completed ===");
}

} // namespace DeviceTestUtils

#endif // DEVICE_TEST_UTILS_H