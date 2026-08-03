cmake_minimum_required(VERSION 3.24)

project(IntentClip
    VERSION 0.1.0
    DESCRIPTION "IntentClip local intent assistant"
    LANGUAGES CXX
)

include(GNUInstallDirs)

if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network)
find_package(llama CONFIG REQUIRED)

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
    app/rounded_menu.h
)

target_link_libraries(IntentClip
    PRIVATE
        Qt6::Widgets
        Qt6::Network
)

if(WIN32)
    target_link_libraries(IntentClip PRIVATE user32)
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

if(WIN32)
    add_custom_command(TARGET IntentClip POST_BUILD
        COMMAND "$<TARGET_FILE:Qt6::windeployqt>"
            --no-translations
            --dir "$<TARGET_FILE_DIR:IntentClip>"
            "$<TARGET_FILE:IntentClip>"
        COMMENT "Deploying the Qt runtime with windeployqt"
        VERBATIM
    )
endif()

install(TARGETS IntentClip IntentClipInference
    RUNTIME_DEPENDENCIES
        PRE_EXCLUDE_REGEXES
            "api-ms-.*"
            "ext-ms-.*"
        POST_EXCLUDE_REGEXES
            ".*[Ww]indows[/\\\\][Ss]ystem32[/\\\\].*\\.dll"
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    FILES config/functions.default.json
    DESTINATION ${CMAKE_INSTALL_BINDIR}/config
)

set(INTENTCLIP_MODEL_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/models/Qwen3-0.6B-Q8_0.gguf"
    CACHE FILEPATH
    "GGUF model bundled with IntentClip"
)

if(NOT EXISTS "${INTENTCLIP_MODEL_FILE}")
    message(FATAL_ERROR
        "IntentClip model not found: ${INTENTCLIP_MODEL_FILE}\n"
        "Download Qwen3-0.6B-Q8_0.gguf before configuring the package."
    )
endif()

install(
    FILES "${INTENTCLIP_MODEL_FILE}"
    DESTINATION ${CMAKE_INSTALL_BINDIR}/models
)

qt_generate_deploy_app_script(
    TARGET IntentClip
    OUTPUT_SCRIPT intentclip_qt_deploy_script
    NO_TRANSLATIONS
)

install(CODE "set(QT_DEPLOY_PLUGINS_DIR \"${CMAKE_INSTALL_BINDIR}\")")
install(SCRIPT ${intentclip_qt_deploy_script})
