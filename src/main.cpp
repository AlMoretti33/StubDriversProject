#include "uart_stub_driver.hpp"
#include "rs485_stub_driver.hpp"
#include "usbcdc_stub_driver.hpp"
#include "Protocol.hpp"
#include <iostream>

int main() {
    ConsoleLogger logger;
    UartDriver   uart("UART1", logger);
    Rs485Driver  rs485("RS485-1", logger);
    UsbCdcDriver usb("USB-CDC", logger);

    uart.open();
    rs485.open();
    usb.open();

    Protocol proto(logger, 30);
    proto.attach(uart);
    proto.attach(rs485);
    proto.attach(usb);

    proto.on(0x01, [](uint8_t id, const vector<uint8_t>& p) {
        cout << "  >> handler id=0x" << hex << (int)id
            << " len=" << dec << p.size() << "\n";
        });
    proto.on(0x02, [](uint8_t id, const vector<uint8_t>& p) {
        cout << "  >> handler id=0x" << hex << (int)id
            << " len=" << dec << p.size() << "\n";
        });

    // «Отправка»: имитируем приход валидных фреймов + мусора
    uart.injectRx(Protocol::buildFrame(0x01, { 1,2,3 }));
    rs485.injectRx({ 0xDE, 0xAD }); // мусор
    rs485.injectRx(Protocol::buildFrame(0x02, vector<uint8_t>(100, 0xAB)));
    auto big = Protocol::buildFrame(0x01, vector<uint8_t>(300, 0x7F));
    // разбиваем большой фрейм на два куска
    usb.injectRx({ big.begin(), big.begin() + 120 });
    usb.injectRx({ big.begin() + 120, big.end() });

    proto.poll();

    cout << "delivered=" << proto.deliveredCount()
        << " errors=" << proto.errorCount()
        << " timeouts=" << proto.timeoutCount() << "\n";
    return 0;
}