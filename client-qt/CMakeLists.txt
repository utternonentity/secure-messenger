cmake_minimum_required(VERSION 3.22)
project(sm_client LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 6.5 COMPONENTS Core Gui Qml Quick REQUIRED)

add_executable(sm_client
src/main.cpp
src/AppController.cpp
src/AppController.h
)

target_link_libraries(sm_client PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
install(TARGETS sm_client RUNTIME DESTINATION bin)

cmake_minimum_required(VERSION 3.21)
project(secure_messenger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets)

add_executable(secure_messenger
    src/main.cpp
)

target_link_libraries(secure_messenger
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::WebSockets
)

set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
