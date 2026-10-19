#pragma once
#include "serial_interface.hpp"
#include "logger_interface.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

class Protocol {
public:
    static constexpr uint8_t  SOF1 = 0xAA;
    static constexpr uint8_t  SOF2 = 0x55;
    static constexpr size_t   kHeaderSize = 5;   // SOF1 SOF2 ID LEN(2)
    static constexpr size_t   kCrcSize = 2;
    static constexpr size_t   kMaxPayload = 1024;
    static constexpr uint32_t kDefaultTimeoutMs = 50;

    using MessageHandler = function<void(uint8_t id,
        const vector<uint8_t>& payload)>;

    Protocol(ILogger& logger, uint32_t timeoutMs = kDefaultTimeoutMs);

    void attach(ISerial& drv);
    size_t interfaceCount() const { return drivers_.size(); }

    // регистрация обработчика по ID
    void on(uint8_t id, MessageHandler h);
    void poll();

    // для тестов
    size_t deliveredCount() const { return delivered_; }
    size_t errorCount()     const { return errors_; }
    size_t timeoutCount()   const { return timeouts_; }

    static vector<uint8_t> buildFrame(uint8_t id,
        const vector<uint8_t>& payload);
    static uint16_t crc16(const uint8_t* data, size_t len);

private:
    struct ChannelState {
        vector<uint8_t> buf;
        uint32_t lastByteMs = 0;
    };

    void feed(size_t idx, const uint8_t* data, size_t len, uint32_t now);
    void tryParse(size_t idx);
    void resetChannel(size_t idx);
    void dispatch(uint8_t id, const vector<uint8_t>& payload);

    ILogger& log_;
    uint32_t timeoutMs_;
    vector<ISerial*> drivers_;
    vector<ChannelState>   states_;
    map<uint8_t, MessageHandler> handlers_;

    size_t delivered_ = 0;
    size_t errors_ = 0;
    size_t timeouts_ = 0;

    uint32_t nowMs();
};
