cmake_minimum_required(VERSION 3.16)
project(EnvironmentMonitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Mqtt
    Charts
    Sql
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mqttclient.cpp
    src/sensordata.cpp
    src/databasemanager.cpp
    src/chartwidget.cpp
    src/switchcontrolwidget.cpp
    src/thresholdeditorwidget.cpp
    src/sensordisplaywidget.cpp
    src/logindialog.cpp
    src/historydialog.cpp
)

set(UI_FILES
    ui/mainwindow.ui
)

set(RESOURCE_FILES
    resources/icons.qrc
    resources/styles.qrc
)

add_executable(EnvironmentMonitor ${SOURCES} ${UI_FILES} ${RESOURCE_FILES}
    src/chartwidget.h src/databasemanager.h src/historydialog.h src/logindialog.h src/mainwindow.h src/mqttclient.h src/sensordata.h src/sensordisplaywidget.h src/switchcontrolwidget.h src/thresholdeditorwidget.h)

target_link_libraries(EnvironmentMonitor PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Mqtt
    Qt6::Charts
    Qt6::Sql
)

install(TARGETS EnvironmentMonitor DESTINATION bin)

