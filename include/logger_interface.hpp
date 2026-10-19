#pragma once
#include <string>
#include <iostream>
#include <mutex>

class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(const ::string& tag, const ::string& msg) = 0;
};

class ConsoleLogger final : public ILogger {
public:
    void log(const ::string& tag, const ::string& msg) override {
        ::lock_guard<::mutex> lk(mtx_);
        ::cout << "[" << tag << "] " << msg << "\n";
    }
private:
    ::mutex mtx_;
};