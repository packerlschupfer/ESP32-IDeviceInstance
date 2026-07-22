/**
 * @file MockDeviceInstance.h
 * @brief Mock implementation of IDeviceInstance for unit testing
 * 
 * This mock class provides a configurable test double for IDeviceInstance
 * that can simulate various behaviors and error conditions.
 */

#ifndef MOCK_DEVICE_INSTANCE_H
#define MOCK_DEVICE_INSTANCE_H

#include "../src/IDeviceInstance.h"
#include <map>
#include <queue>
#include <chrono>

/**
 * @class MockDeviceInstance
 * @brief Mock implementation for testing IDeviceInstance consumers
 * 
 * Features:
 * - Configurable initialization behavior
 * - Simulated data acquisition with configurable delays
 * - Error injection capabilities
 * - Event notification testing
 * - Thread safety verification
 */
class MockDeviceInstance : public IDeviceInstance {
private:
    // State management
    bool initialized;
    bool dataRequested;
    bool dataProcessed;
    
    // Synchronization primitives
    SemaphoreHandle_t mutexInstance;
    SemaphoreHandle_t mutexInterface;
    EventGroupHandle_t eventGroup;
    
    // Test data storage
    std::map<DeviceDataType, std::vector<float>> testData;
    
    // Error injection
    ErrorCode nextError;
    bool shouldFailNext;
    
    // Callback storage
    std::vector<EventCallback> callbacks;
    bool eventsEnabled[5];  // For each EventType
    
    // Action tracking
    std::vector<std::pair<int, int>> performedActions;
    
    // Timing simulation
    TickType_t initDelay;
    TickType_t dataDelay;
    
    // Event bits
    static constexpr EventBits_t INIT_COMPLETE_BIT = BIT0;
    static constexpr EventBits_t DATA_READY_BIT = BIT1;
    static constexpr EventBits_t ERROR_BIT = BIT2;

public:
    /**
     * @brief Constructor with configurable delays
     * @param initDelayMs Initialization delay in milliseconds
     * @param dataDelayMs Data acquisition delay in milliseconds
     */
    MockDeviceInstance(uint32_t initDelayMs = 0, uint32_t dataDelayMs = 0) 
        : initialized(false), dataRequested(false), dataProcessed(false),
          nextError(ErrorCode::SUCCESS), shouldFailNext(false),
          initDelay(pdMS_TO_TICKS(initDelayMs)), 
          dataDelay(pdMS_TO_TICKS(dataDelayMs)) {
        
        mutexInstance = xSemaphoreCreateMutex();
        mutexInterface = xSemaphoreCreateMutex();
        eventGroup = xEventGroupCreate();
        
        // Initialize all events as enabled
        for (int i = 0; i < 5; i++) {
            eventsEnabled[i] = true;
        }
        
        IDEV_LOG_D("MockDeviceInstance created with init delay %u ms, data delay %u ms", 
                   initDelayMs, dataDelayMs);
    }
    
    ~MockDeviceInstance() {
        if (mutexInstance) vSemaphoreDelete(mutexInstance);
        if (mutexInterface) vSemaphoreDelete(mutexInterface);
        if (eventGroup) vEventGroupDelete(eventGroup);
    }
    
    // Core interface implementation
    void initialize() override {
        if (xSemaphoreTake(mutexInstance, portMAX_DELAY) == pdTRUE) {
            IDEV_LOG_I("MockDevice initializing...");
            
            // Simulate initialization delay
            if (initDelay > 0) {
                xSemaphoreGive(mutexInstance);
                vTaskDelay(initDelay);
                xSemaphoreTake(mutexInstance, portMAX_DELAY);
            }
            
            initialized = true;
            xEventGroupSetBits(eventGroup, INIT_COMPLETE_BIT);
            
            // Notify callbacks
            notifyEvent(EventType::INITIALIZED, ErrorCode::SUCCESS);
            
            IDEV_LOG_I("MockDevice initialized");
            xSemaphoreGive(mutexInstance);
        }
    }
    
    bool isInitialized() const noexcept override {
        return initialized;
    }
    
    void waitForInitialization() override {
        xEventGroupWaitBits(eventGroup, INIT_COMPLETE_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    }
    
    ErrorCode waitForInitialization(TickType_t xTicksToWait) override {
        EventBits_t bits = xEventGroupWaitBits(
            eventGroup, INIT_COMPLETE_BIT, pdFALSE, pdTRUE, xTicksToWait
        );
        return (bits & INIT_COMPLETE_BIT) ? ErrorCode::SUCCESS : ErrorCode::TIMEOUT;
    }
    
    bool requestData() override {
        if (!initialized) {
            IDEV_LOG_E("Cannot request data - not initialized");
            return false;
        }
        
        if (shouldFailNext) {
            shouldFailNext = false;
            notifyEvent(EventType::ERROR_OCCURRED, nextError);
            return false;
        }
        
        if (xSemaphoreTake(mutexInterface, portMAX_DELAY) == pdTRUE) {
            IDEV_LOG_D("Data request initiated");
            dataRequested = true;
            dataProcessed = false;
            
            // Clear previous data ready bit
            xEventGroupClearBits(eventGroup, DATA_READY_BIT);
            
            // Simulate async data acquisition
            if (dataDelay > 0) {
                xTaskCreate([](void* param) {
                    MockDeviceInstance* self = static_cast<MockDeviceInstance*>(param);
                    vTaskDelay(self->dataDelay);
                    xEventGroupSetBits(self->eventGroup, DATA_READY_BIT);
                    self->notifyEvent(EventType::DATA_READY, ErrorCode::SUCCESS);
                    vTaskDelete(nullptr);
                }, "MockDataAcq", 2048, this, 1, nullptr);
            } else {
                xEventGroupSetBits(eventGroup, DATA_READY_BIT);
                notifyEvent(EventType::DATA_READY, ErrorCode::SUCCESS);
            }
            
            xSemaphoreGive(mutexInterface);
            return true;
        }
        
        return false;
    }
    
    bool waitForData() override {
        EventBits_t bits = xEventGroupWaitBits(
            eventGroup, DATA_READY_BIT | ERROR_BIT, pdFALSE, pdFALSE, portMAX_DELAY
        );
        return (bits & DATA_READY_BIT) != 0;
    }
    
    ErrorCode waitForData(TickType_t xTicksToWait) override {
        EventBits_t bits = xEventGroupWaitBits(
            eventGroup, DATA_READY_BIT | ERROR_BIT, pdFALSE, pdFALSE, xTicksToWait
        );
        
        if (bits & DATA_READY_BIT) {
            return ErrorCode::SUCCESS;
        } else if (bits & ERROR_BIT) {
            return nextError;
        } else {
            return ErrorCode::TIMEOUT;
        }
    }
    
    void processData() override {
        if (xSemaphoreTake(mutexInstance, portMAX_DELAY) == pdTRUE) {
            IDEV_LOG_D("Processing data");
            dataProcessed = true;
            xSemaphoreGive(mutexInstance);
        }
    }
    
    DataResult getData(DeviceDataType dataType) override {
        DataResult result = {false, {}};
        
        if (xSemaphoreTake(mutexInstance, portMAX_DELAY) == pdTRUE) {
            if (!initialized) {
                IDEV_LOG_E("getData called on uninitialized device");
            } else if (!dataProcessed) {
                IDEV_LOG_W("getData called before processData");
            } else {
                auto it = testData.find(dataType);
                if (it != testData.end()) {
                    result.success = true;
                    result.values = it->second;
                    IDEV_LOG_D("Returning %zu values for data type %d", 
                               result.values.size(), static_cast<int>(dataType));
                } else {
                    IDEV_LOG_W("No test data configured for type %d", 
                               static_cast<int>(dataType));
                }
            }
            xSemaphoreGive(mutexInstance);
        }
        
        return result;
    }
    
    SemaphoreHandle_t getMutexInstance() const noexcept override {
        return mutexInstance;
    }
    
    SemaphoreHandle_t getMutexInterface() const noexcept override {
        return mutexInterface;
    }
    
    EventGroupHandle_t getEventGroup() const noexcept override {
        return eventGroup;
    }
    
    ErrorCode performAction(int actionId, int actionParam) override {
        if (!initialized) {
            return ErrorCode::NOT_INITIALIZED;
        }
        
        if (xSemaphoreTake(mutexInstance, portMAX_DELAY) == pdTRUE) {
            performedActions.push_back({actionId, actionParam});
            IDEV_LOG_I("Performed action %d with param %d", actionId, actionParam);
            
            notifyEvent(EventType::STATE_CHANGED, ErrorCode::SUCCESS, actionId);
            
            xSemaphoreGive(mutexInstance);
            return ErrorCode::SUCCESS;
        }
        
        return ErrorCode::MUTEX_ERROR;
    }
    
    ErrorCode registerCallback(EventCallback callback) override {
        if (xSemaphoreTake(mutexInstance, portMAX_DELAY) == pdTRUE) {
            callbacks.push_back(callback);
            xSemaphoreGive(mutexInstance);
            return ErrorCode::SUCCESS;
        }
        return ErrorCode::MUTEX_ERROR;
    }
    
    ErrorCode unregisterCallbacks() override {
        if (xSemaphoreTake(mutexInstance, portMAX_DELAY) == pdTRUE) {
            callbacks.clear();
            xSemaphoreGive(mutexInstance);
            return ErrorCode::SUCCESS;
        }
        return ErrorCode::MUTEX_ERROR;
    }
    
    ErrorCode setEventNotification(EventType eventType, bool enable) override {
        if (static_cast<int>(eventType) >= 5) {
            return ErrorCode::INVALID_PARAMETER;
        }
        
        eventsEnabled[static_cast<int>(eventType)] = enable;
        return ErrorCode::SUCCESS;
    }
    
    // Test helper methods
    
    /**
     * @brief Set test data for a specific data type
     */
    void setTestData(DeviceDataType dataType, const std::vector<float>& values) {
        if (xSemaphoreTake(mutexInstance, portMAX_DELAY) == pdTRUE) {
            testData[dataType] = values;
            xSemaphoreGive(mutexInstance);
        }
    }
    
    /**
     * @brief Configure next operation to fail with specific error
     */
    void injectError(ErrorCode error) {
        nextError = error;
        shouldFailNext = true;
    }
    
    /**
     * @brief Get list of performed actions for verification
     */
    std::vector<std::pair<int, int>> getPerformedActions() const {
        return performedActions;
    }
    
    /**
     * @brief Reset mock to initial state
     */
    void reset() {
        if (xSemaphoreTake(mutexInstance, portMAX_DELAY) == pdTRUE) {
            initialized = false;
            dataRequested = false;
            dataProcessed = false;
            testData.clear();
            performedActions.clear();
            nextError = ErrorCode::SUCCESS;
            shouldFailNext = false;
            
            xEventGroupClearBits(eventGroup, INIT_COMPLETE_BIT | DATA_READY_BIT | ERROR_BIT);
            
            xSemaphoreGive(mutexInstance);
        }
    }
    
    /**
     * @brief Get callback count for testing
     */
    size_t getCallbackCount() const {
        return callbacks.size();
    }

private:
    /**
     * @brief Notify all registered callbacks
     */
    void notifyEvent(EventType type, ErrorCode error, int customData = 0) {
        if (!eventsEnabled[static_cast<int>(type)]) {
            return;
        }
        
        EventNotification notification = {type, error, customData};
        
        // Create task to call callbacks to avoid blocking
        xTaskCreate([](void* param) {
            auto* data = static_cast<std::pair<MockDeviceInstance*, EventNotification>*>(param);
            MockDeviceInstance* self = data->first;
            EventNotification notif = data->second;
            
            for (const auto& callback : self->callbacks) {
                callback(notif);
            }
            
            delete data;
            vTaskDelete(nullptr);
        }, "MockNotify", 2048, new std::pair<MockDeviceInstance*, EventNotification>(this, notification), 1, nullptr);
    }
};

#endif // MOCK_DEVICE_INSTANCE_H