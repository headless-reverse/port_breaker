cmake_minimum_required(VERSION 3.16)

project(port_breaker VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

add_executable(${PROJECT_NAME}
    main.cpp
    mainwindow.cpp
    PortBreaker.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui)

set_property(TARGET ${PROJECT_NAME} PROPERTY AUTOMOC ON)

if(UNIX)
    find_library(UDEV_LIBRARY udev)
    if(UDEV_LIBRARY)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${UDEV_LIBRARY})
    endif()
endif()
