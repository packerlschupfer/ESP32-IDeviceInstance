/**
 * @file SmokeTest.ino
 * @brief Minimal compile-smoke for the ESP32-IDeviceInstance interface.
 *
 * IDeviceInstance is a pure abstract base class. This smoke defines a tiny
 * concrete subclass (DummyDevice) that overrides every pure virtual, then
 * instantiates it and exercises a few methods. The goal is purely to prove
 * the header compiles and the interface contract is implementable.
 */

#include <Arduino.h>
#include <IDeviceInstance.h>

/**
 * @brief Minimal concrete IDeviceInstance implementation for the smoke test.
 *
 * Every pure-virtual method is overridden with a trivial, valid body.
 * No real hardware is touched.
 */
class DummyDevice : public IDeviceInstance {
public:
    DeviceResult<void> initialize() override {
        initialized_ = true;
        return DeviceResult<void>();  // success
    }

    bool isInitialized() const noexcept override {
        return initialized_;
    }

    void waitForInitialization() override {
        // no-op
    }

    DeviceResult<void> waitForInitializationComplete(TickType_t timeout = portMAX_DELAY) override {
        (void)timeout;
        return DeviceResult<void>();  // success
    }

    DeviceResult<void> requestData() override {
        return DeviceResult<void>();
    }

    bool waitForData() override {
        return true;
    }

    DeviceResult<void> processData() override {
        return DeviceResult<void>();
    }

    DeviceResult<std::vector<float>> getData(DeviceDataType dataType) override {
        (void)dataType;
        return DeviceResult<std::vector<float>>(std::vector<float>{0.0f});
    }

    SemaphoreHandle_t getMutexInstance() const noexcept override {
        return nullptr;
    }

    SemaphoreHandle_t getMutexInterface() const noexcept override {
        return nullptr;
    }

    EventGroupHandle_t getEventGroup() const noexcept override {
        return nullptr;
    }

    DeviceResult<void> performAction(int actionId, int actionParam) override {
        (void)actionId;
        (void)actionParam;
        return DeviceResult<void>();
    }

    DeviceResult<void> registerCallback(EventCallback callback) override {
        (void)callback;
        return DeviceResult<void>();
    }

    DeviceResult<void> unregisterCallbacks() override {
        return DeviceResult<void>();
    }

    DeviceResult<void> setEventNotification(EventType eventType, bool enable) override {
        (void)eventType;
        (void)enable;
        return DeviceResult<void>();
    }

private:
    bool initialized_ = false;
};

static DummyDevice g_device;

void setup() {
    Serial.begin(115200);

    // Exercise the interface through the abstract base pointer.
    IDeviceInstance* dev = &g_device;

    auto initResult = dev->initialize();
    Serial.printf("initialize() ok=%d, isInitialized=%d\n",
                  static_cast<int>(initResult.isOk()),
                  static_cast<int>(dev->isInitialized()));

    if (dev->requestData().isOk() && dev->waitForData()) {
        (void)dev->processData();
        auto data = dev->getData(IDeviceInstance::DeviceDataType::TEMPERATURE);
        Serial.printf("getData ok=%d\n", static_cast<int>(data.isOk()));
    }

    // Static helpers from the interface.
    Serial.printf("isValidDataType(0)=%d err=%s\n",
                  static_cast<int>(IDeviceInstance::isValidDataType(0)),
                  IDeviceInstance::deviceErrorToString(IDeviceInstance::DeviceError::SUCCESS));
}

void loop() {
    delay(1000);
}
