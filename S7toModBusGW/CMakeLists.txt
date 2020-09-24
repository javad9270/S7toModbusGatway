cmake_minimum_required(VERSION 3.1)
project(S7toModbusGateway)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

include_directories(Include)
include_directories(modbus)
include_directories(snap7)
include_directories(wiringPi)
include_directories(rapidjson-master/include/rapidjson)

SET(SOURCES
    dataItem.cpp
    LED.cpp
    main.cpp
    ModbusServer.cpp
    PLC.cpp
    snap7/snap7.cpp
    )

add_executable(S7toModbusGateway ${SOURCES})

target_link_libraries(S7toModbusGateway m pthread rt modbus snap7 wiringPi)
            
