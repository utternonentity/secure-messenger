cmake_minimum_required(VERSION 3.22)

project(secure_messenger_clients LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Автогенерация для Qt (обязательно для Q_OBJECT)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Если используешь системный Qt 6.4.2 — просим 6.4.
# Если поставишь 6.5+, можно поднять тут версию.
find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core Gui
    Qml Quick         # для sm_client
    Widgets Network WebSockets  # для secure_messenger
)

# ------------------------
# sm_client (QML/Quick)
# ------------------------
add_executable(sm_client
    src/main.cpp
    src/AppController.cpp
    src/AppController.h   # чтобы AUTOMOC увидел Q_OBJECT
    qml/Main.qml          # можно не добавлять, если подключаешь через ресурс
)

target_link_libraries(sm_client PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    Qt6::Network Qt6::WebSockets   # если будешь дёргать сеть из клиента
)

install(TARGETS sm_client RUNTIME DESTINATION bin)

# ------------------------
# secure_messenger (Widgets/WebSockets)
# ВНИМАНИЕ: у этой цели должен быть СВОЙ main,
# например src/secure_main.cpp (не тот же самый, что у sm_client)
# ------------------------
add_executable(secure_messenger
    src/secure_main.cpp
    # добавляй сюда .cpp/.h этой цели
)

target_link_libraries(secure_messenger PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets
    Qt6::Network Qt6::WebSockets
)

install(TARGETS secure_messenger RUNTIME DESTINATION bin)

# (Опционально) тайминг компиляции
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
