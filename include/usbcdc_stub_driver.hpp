#pragma once
#include "uart_stub_driver.hpp"

class UsbCdcDriver final : public UartDriver {
public:
    UsbCdcDriver(string name, ILogger& logger)
        : UartDriver(move(name), logger) {}

    size_t write(const uint8_t* buf, size_t len) override {
        log_.log(name_, "USB packet len=" + to_string(len));
        return UartDriver::write(buf, len);
    }
};