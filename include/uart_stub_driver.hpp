#pragma once
#include "serial_interface.hpp"
#include "logger_interface.hpp"
#include <deque>
#include <vector>

class UartDriver : public ISerial {
public:
    UartDriver(string name,ILogger& logger);
    bool open() override;
    void close() override;
    bool isOpen() const override;
    size_t read(uint8_t* buf, size_t maxLen) override;
    size_t write(const uint8_t* buf, size_t len) override;
    const char* name() const override;
    void injectRx(const vector<uint8_t>& data);
    vector<uint8_t> takeTx();

protected:
    string name_;
    ILogger& log_;
    bool        open_ = false;
    deque<uint8_t> rx_;
    vector<uint8_t> tx_;
};