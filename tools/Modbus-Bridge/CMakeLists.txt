cmake_minimum_required(VERSION 3.16)
project(modbus2mqtt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Mqtt)

qt_standard_project_setup()

qt_add_executable(modbus2mqtt
    src/main.cpp
    src/modbustcp.h
    src/modbustcp.cpp
)

target_link_libraries(modbus2mqtt
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        Qt6::Mqtt
)

install(TARGETS modbus2mqtt
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET modbus2mqtt
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(tcp_parse_test
        test/tcp_parse_test.cpp
        src/modbustcp.cpp
        src/modbustcp.h
    )
    target_include_directories(tcp_parse_test PRIVATE src)
    target_link_libraries(tcp_parse_test
        PRIVATE
            Qt6::Core
            Qt6::Test
    )
    add_test(NAME tcp_parse COMMAND tcp_parse_test)
endif()