cmake_minimum_required(VERSION 3.10)
project(StubDriversProject CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(proto STATIC
    src/protocol.cpp
    src/uart_stub_driver.cpp
)

target_include_directories(proto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(app src/main.cpp)
target_link_libraries(app PRIVATE proto)

add_executable(integration_test tests/tests.cpp)
target_link_libraries(integration_test PRIVATE proto)