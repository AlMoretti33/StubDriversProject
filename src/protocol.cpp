#include "protocol.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>

Protocol::Protocol(ILogger& logger, uint32_t timeoutMs)
    : log_(logger), timeoutMs_(timeoutMs) {}

uint32_t Protocol::nowMs() {
    using namespace chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void Protocol::attach(ISerial& drv) {
    if (drivers_.size() >= 5) {
        log_.log("Protocol", "attach: max 5 interfaces reached, ignored");
        return;
    }
    drivers_.push_back(&drv);
    states_.emplace_back();
    log_.log("Protocol", string("attached ") + drv.name() +
        " (total=" + to_string(drivers_.size()) + ")");
}

void Protocol::on(uint8_t id, MessageHandler h) {
    handlers_[id] = move(h);
}

uint16_t Protocol::crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
            : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

vector<uint8_t> Protocol::buildFrame(uint8_t id,
    const vector<uint8_t>& payload) {
    vector<uint8_t> f;
    f.reserve(kHeaderSize + payload.size() + kCrcSize);
    f.push_back(SOF1);
    f.push_back(SOF2);
    f.push_back(id);
    f.push_back(static_cast<uint8_t>(payload.size() & 0xFF));
    f.push_back(static_cast<uint8_t>((payload.size() >> 8) & 0xFF));
    f.insert(f.end(), payload.begin(), payload.end());
    uint16_t crc = crc16(f.data() + 2, f.size() - 2);
    f.push_back(static_cast<uint8_t>(crc & 0xFF));
    f.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    return f;
}

void Protocol::resetChannel(size_t idx) {
    states_[idx].buf.clear();
    states_[idx].lastByteMs = nowMs();
}

void Protocol::poll() {
    uint8_t tmp[256];
    const uint32_t now = nowMs();

    for (size_t i = 0; i < drivers_.size(); ++i) {
        ISerial* d = drivers_[i];
        if (!d->isOpen()) continue;

        if (!states_[i].buf.empty() &&
            (now - states_[i].lastByteMs) > timeoutMs_) {
            log_.log("Protocol", string("timeout on ") + d->name() +
                ", drop " + to_string(states_[i].buf.size()) + " bytes");
            ++timeouts_;
            resetChannel(i);
        }

        size_t n;
        while ((n = d->read(tmp, sizeof(tmp))) > 0) {
            feed(i, tmp, n, now);
            tryParse(i);
        }
    }
}

void Protocol::feed(size_t idx, const uint8_t* data,
    size_t len, uint32_t now) {
    auto& buf = states_[idx].buf;
    buf.insert(buf.end(), data, data + len);
    states_[idx].lastByteMs = now;
}

void Protocol::tryParse(size_t idx) {
    auto& buf = states_[idx].buf;

    while (1) {
        size_t sof = string::npos;
        for (size_t i = 0; i + 1 < buf.size(); ++i) {
            if (buf[i] == SOF1 && buf[i + 1] == SOF2) { sof = i; break; }
        }
        if (sof == string::npos) {
            if (buf.size() > 1) {
                size_t drop = buf.size() - 1;
                log_.log("Protocol", "drop garbage " + to_string(drop));
                ++errors_;
                buf.erase(buf.begin(), buf.begin() + drop);
            }
            return;
        }
        if (sof > 0) {
            log_.log("Protocol", "drop pre-" + to_string(sof) + " bytes");
            ++errors_;
            buf.erase(buf.begin(), buf.begin() + sof);
        }
        if (buf.size() < kHeaderSize) return;

        uint8_t  id = buf[2];
        uint16_t len = static_cast<uint16_t>(buf[3]) |
            (static_cast<uint16_t>(buf[4]) << 8);

        if (len > kMaxPayload) {
            log_.log("Protocol", "bad len=" + to_string(len) + ", resync");
            ++errors_;
            buf.erase(buf.begin(), buf.begin() + 1); 
            continue;
        }

        const size_t total = kHeaderSize + len + kCrcSize;
        if (buf.size() < total) return;

        const uint8_t* body = buf.data() + 2;
        const size_t bodyLen = 3 + len; // ID(1) + LEN(2) + PAYLOAD
        uint16_t crcCalc = crc16(body, bodyLen);
        uint16_t crcGot = static_cast<uint16_t>(buf[kHeaderSize + len]) |
            (static_cast<uint16_t>(buf[kHeaderSize + len + 1]) << 8);
        if (crcCalc != crcGot) {
            log_.log("DBG", "bodyLen=" + to_string(bodyLen) +
                " body[0]=" + to_string(body[0]) +
                " body[1]=" + to_string(body[1]));
            log_.log("Protocol", "CRC mismatch (calc=" + to_string(crcCalc) +
                " got=" + to_string(crcGot) + "), resync");
            ++errors_;
            buf.erase(buf.begin(), buf.begin() + 1);
            continue;
        }

        vector<uint8_t> payload(buf.begin() + kHeaderSize,
            buf.begin() + kHeaderSize + len);
        buf.erase(buf.begin(), buf.begin() + total);
        dispatch(id, payload);
    }
}

void Protocol::dispatch(uint8_t id, const vector<uint8_t>& payload) {
    auto it = handlers_.find(id);
    if (it == handlers_.end()) {
        log_.log("Protocol", "no handler for id=" + to_string(id) +
            ", payload=" + to_string(payload.size()) + "B");
        return;
    }
    ++delivered_;
    it->second(id, payload);
}