cmake_minimum_required(VERSION 3.24)

project(IntentClip
    VERSION 0.1.0
    DESCRIPTION "IntentClip local intent assistant"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network)
find_package(llama CONFIG REQUIRED)

if(WIN32)
    find_package(Qt6QWindowsIntegrationPlugin CONFIG REQUIRED
        HINTS "${Qt6Gui_DIR}"
        NO_DEFAULT_PATH
    )
endif()

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(IntentClip
    app/intent_prompt_config.cpp
    app/intent_prompt_config.h
    app/inference_client.cpp
    app/inference_client.h
    app/copy_gesture_monitor.cpp
    app/copy_gesture_monitor.h
    app/main.cpp
    app/main_window.cpp
    app/main_window.h
    app/prompt_settings_dialog.cpp
    app/prompt_settings_dialog.h
)

target_link_libraries(IntentClip
    PRIVATE
        Qt6::Widgets
        Qt6::Network
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


add_custom_command(TARGET IntentClip POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "$<TARGET_FILE_DIR:IntentClip>/config/intents"
    COMMAND ${CMAKE_COMMAND} -E rm -f
        "$<TARGET_FILE_DIR:IntentClip>/config/intent_prompt.default.json"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:IntentClip>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/functions.default.json"
        "$<TARGET_FILE_DIR:IntentClip>/config/functions.default.json"
    COMMENT "Deploying the default function registry"
)
qt_add_executable(IntentClipInference
    worker/main.cpp
)

target_link_libraries(IntentClipInference
    PRIVATE
        Qt6::Core
        Qt6::Network
        llama
)

set_target_properties(IntentClipInference PROPERTIES
    WIN32_EXECUTABLE TRUE
)
set_target_properties(IntentClip PROPERTIES
    WIN32_EXECUTABLE TRUE
)

install(TARGETS IntentClip IntentClipInference
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

install(FILES config/functions.default.json DESTINATION bin/config)
