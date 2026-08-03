cmake_minimum_required(VERSION 3.24)

project(IntentClip
    VERSION 0.1.0
    DESCRIPTION "IntentClip local intent assistant"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)

if(WIN32)
    find_package(Qt6QWindowsIntegrationPlugin CONFIG REQUIRED
        HINTS "${Qt6Gui_DIR}"
        NO_DEFAULT_PATH
    )
endif()

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(IntentClip
    app/copy_gesture_monitor.cpp
    app/copy_gesture_monitor.h
    app/main.cpp
    app/main_window.cpp
    app/main_window.h
)

target_link_libraries(IntentClip
    PRIVATE
        Qt6::Widgets
)

if(WIN32)
    target_link_libraries(IntentClip PRIVATE user32)

    add_custom_command(TARGET IntentClip POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:IntentClip>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:IntentClip>/platforms/$<TARGET_FILE_NAME:Qt6::QWindowsIntegrationPlugin>"
        COMMENT "Deploying the Qt Windows platform plugin"
    )
endif()

set_target_properties(IntentClip PROPERTIES
    WIN32_EXECUTABLE TRUE
)

install(TARGETS IntentClip
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
