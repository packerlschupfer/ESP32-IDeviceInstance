# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-12-04

### Added
- Initial public release
- Abstract IDeviceInstance interface for standardized device interaction
- DeviceError enum with comprehensive error codes
- DeviceResult<T> type using common::Result for error handling
- Device lifecycle methods (initialize, waitForInitialization)
- Async data operations (requestData, processData, getData)
- Action execution interface (performAction)
- Thread safety primitives (getMutexInstance, getMutexInterface)
- Event group support (getEventGroup) for FreeRTOS synchronization
- DeviceDataType enum for type-safe data retrieval

Platform: ESP32 (Arduino/ESP-IDF)
License: MIT
Dependencies: LibraryCommon

### Notes
- Production-tested as base interface for MB8ART, RYN4, ANDRTF3 device drivers
- Provides abstraction layer for Modbus and other device types
- Previous internal versions (v1.x-v4.x) not publicly released
- Reset to v0.1.0 for clean public release start
