// include/Rs485Driver.hpp
#pragma once
#include "uart_stub_driver.hpp"

class Rs485Driver final : public UartDriver {
public:
    Rs485Driver(std::string name, ILogger& logger)
        : UartDriver(std::move(name), logger) {}

    // RS485 специфика: эмуляция переключения направления
    std::size_t write(const std::uint8_t* buf, std::size_t len) override {
        log_.log(name_, "RS485 DE=TX");
        auto n = UartDriver::write(buf, len);
        log_.log(name_, "RS485 DE=RX");
        return n;
    }
};
