#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

using namespace std;

class ISerial{
public:
    virtual ~ISerial() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual size_t read(uint8_t* buf, size_t maxLen) = 0;
    virtual size_t write(const uint8_t* buf, size_t len) = 0;

    virtual const char* name() const = 0;
};
