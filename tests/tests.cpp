#include "uart_stub_driver.hpp"
#include "rs485_stub_driver.hpp"
#include "usbcdc_stub_driver.hpp"
#include "Protocol.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>


// CHECK + счётчик провалов
static int g_fail = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            cerr << "FAIL: " #cond " @ line " << __LINE__ << "\n";    \
            ++g_fail;                                                      \
        }                                                                  \
    } while (0)

#define CHECK(cond) do { if (!(cond)) { \
    cerr << "FAIL: " #cond " @ line " << __LINE__ << "\n"; \
    for (auto& s : L.lines_) cerr << "   log: " << s << "\n"; \
    ++g_fail; } } while(0)

#define RUN_TEST(func)                                                    \
    do {                                                                  \
        cout << "[ RUN      ] " #func << "\n";                       \
        int fail_before = g_fail;                                         \
        func();                                                           \
        if (g_fail > fail_before) {                                       \
            cout << "[  FAILED  ] " #func << "\n\n";                 \
        } else {                                                          \
            cout << "[       OK ] " #func << "\n\n";                 \
        }                                                                 \
    } while (0)


// Логгер
class CollectLogger : public ILogger {
public:
    void log(const string& tag, const string& msg) override {
        lines_.push_back(tag + ": " + msg);
    }

    size_t count() const { return lines_.size(); }

    bool contains(const string& needle) const {
        for (const auto& s : lines_)
            if (s.find(needle) != string::npos) return true;
        return false;
    }

    vector<string> lines_;
};

// Test 1
static void test_attach_limits() {
    CollectLogger L;
    Protocol p(L);

    UartDriver   a("A", L), b("B", L), c("C", L);
    Rs485Driver  d("D", L), e("E", L), f("F", L);

    a.open(); b.open(); c.open(); d.open(); e.open(); f.open();

    p.attach(a); p.attach(b); p.attach(c); p.attach(d); p.attach(e); p.attach(f);

    CHECK(p.interfaceCount() == 5);
    CHECK(L.contains("max 5 interfaces reached"));
}

// Test 2 
static void test_single_valid_frame() {
    CollectLogger L;
    Protocol p(L);
    UartDriver u("U", L);
    u.open();
    p.attach(u);

    int got = 0;
    vector<uint8_t> payload;
    p.on(0x10, [&](uint8_t, const vector<uint8_t>& d) {
        ++got;
        payload = d;
        });
    u.injectRx(Protocol::buildFrame(0x10, { 1, 2, 3, 4, 5 }));
    p.poll();

    CHECK(got == 1);
    CHECK(payload == (vector<uint8_t>{1, 2, 3, 4, 5}));
    CHECK(p.deliveredCount() == 1);
    CHECK(p.errorCount() == 0);
    CHECK(p.timeoutCount() == 0);
}

// Test 3
static void test_multiple_lengths() {
    CollectLogger L;
    Protocol p(L);
    UartDriver u("U", L);
    u.open();
    p.attach(u);

    vector<size_t> sizes;
    p.on(0x01, [&](uint8_t, const vector<uint8_t>& d) {
        sizes.push_back(d.size());
        });

    const vector<size_t> expected{ 0, 1, 5, 33, 200, 600 };
    vector<uint8_t> all;
    for (size_t n : expected) {
        auto f = Protocol::buildFrame(0x01, vector<uint8_t>(n, 0xCC));
        all.insert(all.end(), f.begin(), f.end());
    }

    u.injectRx(all);
    p.poll();

    CHECK(sizes == expected);
    CHECK(p.deliveredCount() == expected.size());
    CHECK(p.errorCount() == 0);
}

// Test 4
static void test_garbage_and_resync() {
    CollectLogger L;
    Protocol p(L);
    UartDriver u("U", L);
    u.open();
    p.attach(u);

    int got = 0;
    p.on(0x05, [&](uint8_t, const vector<uint8_t>&) { ++got; });

    vector<uint8_t> stream = { 0x00, 0xFF, 0x11, 0x22, 0x33 };
    auto f = Protocol::buildFrame(0x05, { 9, 8, 7 });
    stream.insert(stream.end(), f.begin(), f.end());
    stream.push_back(0xAA);

    u.injectRx(stream);
    p.poll();

    CHECK(got == 1);
    CHECK(p.errorCount() > 0);
    CHECK(p.deliveredCount() == 1);
}

// Test 5
static void test_crc_error_rejected() {
    CollectLogger L;
    Protocol p(L);
    UartDriver u("U", L);
    u.open();
    p.attach(u);

    int got = 0;
    p.on(0x07, [&](uint8_t, const vector<uint8_t>&) { ++got; });

    auto f = Protocol::buildFrame(0x07, { 1, 2, 3 });
    f.back() ^= 0xFF;
    u.injectRx(f);
    p.poll();

    CHECK(got == 0);
    CHECK(p.deliveredCount() == 0);
    CHECK(p.errorCount() > 0);
    CHECK(L.contains("CRC mismatch"));
}

// Test 6
static void test_split_frame() {
    CollectLogger L;
    Protocol p(L);
    UartDriver u("U", L);
    u.open();
    p.attach(u);

    int got = 0;
    vector<uint8_t> payload;
    p.on(0x42, [&](uint8_t, const vector<uint8_t>& d) {
        ++got;
        payload = d;
        });

    auto f = Protocol::buildFrame(0x42, vector<uint8_t>(250, 0xAB));
    for (size_t i = 0; i < f.size(); i += 17) {
        vector<uint8_t> chunk(
            f.begin() + i,
            f.begin() + min(f.size(), i + 17));
        u.injectRx(chunk);
        p.poll();
    }

    CHECK(got == 1);
    CHECK(payload.size() == 250);
    CHECK(p.deliveredCount() == 1);
}

// Test 7
static void test_multiple_interfaces_mixed() {
    CollectLogger L;
    Protocol p(L);

    UartDriver   a("A", L);
    Rs485Driver  b("B", L);
    UsbCdcDriver c("C", L);
    a.open(); b.open(); c.open();

    p.attach(a); p.attach(b); p.attach(c);

    vector<int> ids;
    for (uint8_t id : {0x01, 0x02, 0x03}) {
        p.on(id, [&](uint8_t i, const vector<uint8_t>&) {
            ids.push_back(i);
            });
    }

    a.injectRx(Protocol::buildFrame(0x01, { 1 }));
    b.injectRx(Protocol::buildFrame(0x02, { 2, 2 }));
    c.injectRx(Protocol::buildFrame(0x03, { 3, 3, 3 }));

    p.poll();

    CHECK(ids.size() == 3);
    CHECK((ids == vector<int>{1, 2, 3}));
    CHECK(p.deliveredCount() == 3);
}

// Test 8
static void test_driver_logging() {
    CollectLogger L;

    UartDriver u("UART-X", L);
    u.open();
    u.injectRx({ 1, 2, 3 });

    uint8_t buf[8];
    (void)u.read(buf, sizeof(buf));

    const uint8_t tx[] = { 0xAA };
    (void)u.write(tx, 1);

    u.close();

    CHECK(L.contains("UART-X: open()"));
    CHECK(L.contains("UART-X: injectRx(3)"));
    CHECK(L.contains("UART-X: read("));
    CHECK(L.contains("UART-X: write("));
    CHECK(L.contains("UART-X: close()"));

    Rs485Driver r("RS485-X", L);
    r.open();
    (void)r.write(tx, 1);
    CHECK(L.contains("RS485-X: RS485 DE=TX"));
    CHECK(L.contains("RS485-X: RS485 DE=RX"));

    UsbCdcDriver c("USB-X", L);
    c.open();
    (void)c.write(tx, 1);
    CHECK(L.contains("USB-X: USB packet len=1"));
}

// Test 9
static void test_timeout_drop() {
    CollectLogger L;
    Protocol p(L, 10);

    UartDriver u("U", L);
    u.open();
    p.attach(u);

    auto f = Protocol::buildFrame(0xAA, vector<uint8_t>(100, 0x00));
    vector<uint8_t> head(f.begin(), f.begin() + 6);
    u.injectRx(head);
    p.poll();

    this_thread::sleep_for(chrono::milliseconds(30));
    p.poll();

    CHECK(p.timeoutCount() >= 1);
    CHECK(p.deliveredCount() == 0);
}

// Test 10
static void test_no_handler_for_id() {
    CollectLogger L;
    Protocol p(L);
    UartDriver u("U", L);
    u.open();
    p.attach(u);

    int got1 = 0;
    p.on(0x01, [&](uint8_t, const vector<uint8_t>&) { ++got1; });

    u.injectRx(Protocol::buildFrame(0x02, { 1, 2, 3 }));
    p.poll();

    CHECK(got1 == 0);
    CHECK(p.deliveredCount() == 0);
    CHECK(p.errorCount() == 0);
    CHECK(L.contains("no handler for id=2"));
}

// Test 11
static void test_empty_payload() {
    CollectLogger L;
    Protocol p(L);
    UartDriver u("U", L);
    u.open();
    p.attach(u);

    int got = 0;
    size_t len = 999;
    p.on(0x00, [&](uint8_t, const vector<uint8_t>& d) {
        ++got;
        len = d.size();
        });

    u.injectRx(Protocol::buildFrame(0x00, {}));
    p.poll();

    CHECK(got == 1);
    CHECK(len == 0);
    CHECK(p.errorCount() == 0);
}

// Test 12
static void test_max_payload() {
    CollectLogger L;
    Protocol p(L);
    UartDriver u("U", L);
    u.open();
    p.attach(u);

    int got = 0;
    size_t len = 0;
    p.on(0x33, [&](uint8_t, const vector<uint8_t>& d) {
        ++got;
        len = d.size();
        });

    vector<uint8_t> big(Protocol::kMaxPayload, 0x5A);
    u.injectRx(Protocol::buildFrame(0x33, big));
    p.poll();

    CHECK(got == 1);
    CHECK(len == Protocol::kMaxPayload);
    CHECK(p.errorCount() == 0);
}

// Test 13
static void test_oversized_rejected() {
    CollectLogger L;
    Protocol p(L);
    UartDriver u("U", L);
    u.open();
    p.attach(u);

    int got = 0;
    p.on(0x44, [&](uint8_t, const vector<uint8_t>&) { ++got; });

    vector<uint8_t> bad = {
        Protocol::SOF1, Protocol::SOF2,
        0x44,
        static_cast<uint8_t>((Protocol::kMaxPayload + 1) & 0xFF),
        static_cast<uint8_t>(((Protocol::kMaxPayload + 1) >> 8) & 0xFF),
        0x00, 0x00, 0x00
    };
    u.injectRx(bad);
    p.poll();

    CHECK(got == 0);
    CHECK(p.errorCount() > 0);
    CHECK(L.contains("bad len="));
}

// Test 14
static void test_two_frames_in_one_stream() {
    CollectLogger L;
    Protocol p(L);
    UartDriver u("U", L);
    u.open();
    p.attach(u);

    vector<int> ids;
    p.on(0x01, [&](uint8_t i, const vector<uint8_t>&) { ids.push_back(i); });
    p.on(0x02, [&](uint8_t i, const vector<uint8_t>&) { ids.push_back(i); });

    auto f1 = Protocol::buildFrame(0x01, { 1 });
    auto f2 = Protocol::buildFrame(0x02, { 2, 2 });
    vector<uint8_t> stream = f1;
    stream.insert(stream.end(), f2.begin(), f2.end());

    u.injectRx(stream);
    p.poll();

    CHECK((ids == vector<int>{1, 2}));
    CHECK(p.deliveredCount() == 2);
    CHECK(p.errorCount() == 0);
}

int main() {

    RUN_TEST(test_attach_limits);
    RUN_TEST(test_single_valid_frame);
    RUN_TEST(test_multiple_lengths);
    RUN_TEST(test_garbage_and_resync);
    RUN_TEST(test_crc_error_rejected);
    RUN_TEST(test_split_frame);
    RUN_TEST(test_multiple_interfaces_mixed);
    RUN_TEST(test_driver_logging);
    RUN_TEST(test_timeout_drop);
    RUN_TEST(test_no_handler_for_id);
    RUN_TEST(test_empty_payload);
    RUN_TEST(test_max_payload);
    RUN_TEST(test_oversized_rejected);
    RUN_TEST(test_two_frames_in_one_stream);
    if (g_fail == 0) {
        cout << "ALL I-TESTS PASSED\n";
        return 0;
    }
    cerr << g_fail << " TEST(S) FAILED\n";
    return 1;
}