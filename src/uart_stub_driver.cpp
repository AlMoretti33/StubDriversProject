#include "uart_stub_driver.hpp"
#include <algorithm>

UartDriver::UartDriver(std::string name, ILogger& logger)
    : name_(std::move(name)), log_(logger) {}

bool UartDriver::open() {
    open_ = true;
    log_.log(name_, "open() -> true");
    return true;
}

void UartDriver::close() {
    open_ = false;
    log_.log(name_, "close()");
}

bool UartDriver::isOpen() const { return open_; }

std::size_t UartDriver::read(std::uint8_t* buf, std::size_t maxLen) {
    if (!open_ || maxLen == 0) return 0;
    std::size_t n = std::min(maxLen, rx_.size());
    for (std::size_t i = 0; i < n; ++i) { buf[i] = rx_.front(); rx_.pop_front(); }
    log_.log(name_, "read(" + std::to_string(maxLen) + ") -> " + std::to_string(n));
    return n;
}

std::size_t UartDriver::write(const std::uint8_t* buf, std::size_t len) {
    if (!open_) return 0;
    // эмуляция частичной записи: за раз пишем не больше 64 байт
    std::size_t n = std::min<std::size_t>(len, 64);
    tx_.insert(tx_.end(), buf, buf + n);
    log_.log(name_, "write(" + std::to_string(len) + ") -> " + std::to_string(n));
    return n;
}

const char* UartDriver::name() const { return name_.c_str(); }

void UartDriver::injectRx(const std::vector<std::uint8_t>& data) {
    rx_.insert(rx_.end(), data.begin(), data.end());
    log_.log(name_, "injectRx(" + std::to_string(data.size()) + ")");
}

std::vector<std::uint8_t> UartDriver::takeTx() {
    auto out = tx_;
    tx_.clear();
    log_.log(name_, "takeTx(" + std::to_string(out.size()) + ")");
    return out;
}