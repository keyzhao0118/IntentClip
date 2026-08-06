cmake_minimum_required(VERSION 3.24)

# ============================================================
# 项目元信息
# ============================================================
project(IntentClip
    VERSION 0.1.0
    DESCRIPTION "IntentClip local intent assistant"
    LANGUAGES CXX
)

# CMake 策略：在新版 CMake 中启用向后兼容的新行为
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

# 提供 ${CMAKE_INSTALL_BINDIR} 等标准安装目录变量
include(GNUInstallDirs)

# ============================================================
# 编译标准
# ============================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# ============================================================
# 依赖
# ============================================================
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network Svg LinguistTools)
find_package(llama CONFIG REQUIRED)
qt_standard_project_setup(REQUIRES 6.5)
set(CMAKE_AUTORCC ON)

# ============================================================
# 捆绑模型（GGUF）
# 模型会被拷贝到可执行文件旁的 models/ 目录，使构建输出自包含。
# 运行时解析顺序：INTENTCLIP_MODEL_PATH 环境变量
#   -> <exe_dir>/models/Qwen3-0.6B-Q8_0.gguf -> 源码树回退路径。
# ============================================================
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

# ============================================================
# 主程序：IntentClip（GUI）
# ============================================================
qt_add_executable(IntentClip
    app/intent_prompt_config.cpp
    app/intent_prompt_config.h
    app/i18n.cpp
    app/i18n.h
    app/inference_client.cpp
    app/inference_client.h
    app/copy_gesture_monitor.cpp
    app/copy_gesture_monitor.h
    app/circular_spinner.h
    app/function_settings_dialog.cpp
    app/function_settings_dialog.h
    app/main.cpp
    app/main_window.cpp
    app/main_window.h
    app/prompt_settings_dialog.cpp
    app/prompt_settings_dialog.h
    app/rounded_menu.h
    app/resources.qrc
)

target_link_libraries(IntentClip
    PRIVATE
        Qt6::Widgets
        Qt6::Network
        Qt6::Svg
    )

if(WIN32)
    # Windows 上需要 user32 处理前台窗口/置顶等行为
    target_link_libraries(IntentClip PRIVATE user32)
endif()

# --- 构建后：部署默认功能配置到可执行文件旁的 config/ 目录 ---
# 先清理旧版本遗留的配置目录/文件，再写入当前默认注册表。
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
    VERBATIM
)

# --- 构建后：拷贝 GGUF 模型，让 build 目录可直接调试运行 ---
# copy_if_different 避免每次构建都重写 640MB 的大文件。
add_custom_command(TARGET IntentClip POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:IntentClip>/models"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${INTENTCLIP_MODEL_FILE}"
        "$<TARGET_FILE_DIR:IntentClip>/models/Qwen3-0.6B-Q8_0.gguf"
    COMMENT "Deploying the GGUF model next to the executable"
    VERBATIM
)

if(WIN32)
    # --- 构建后：用 windeployqt 把 Qt 运行库部署到可执行文件旁 ---
    add_custom_command(TARGET IntentClip POST_BUILD
        COMMAND "$<TARGET_FILE:Qt6::windeployqt>"
            --dir "$<TARGET_FILE_DIR:IntentClip>"
            "$<TARGET_FILE:IntentClip>"
        COMMENT "Deploying the Qt runtime with windeployqt"
        VERBATIM
    )
endif()

# ============================================================
# 推理子进程：IntentClipInference（由主程序拉起，加载 llama 模型）
# ============================================================
qt_add_executable(IntentClipInference
    worker/main.cpp
)

target_link_libraries(IntentClipInference
    PRIVATE
        Qt6::Core
        Qt6::Network
        llama
)

# 两个都是 GUI 程序：Windows 下不弹出控制台窗口
set_target_properties(IntentClip IntentClipInference PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# ============================================================
# 国际化：lupdate 扫描源码生成 .ts，lrelease 在构建时生成 .qm
# ============================================================
set(INTENTCLIP_TS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/translations/IntentClip_zh_CN.ts
    ${CMAKE_CURRENT_SOURCE_DIR}/translations/IntentClip_en.ts
)

qt_add_translations(IntentClip
    SOURCE_TARGETS IntentClip
    SOURCES
        worker/main.cpp
    TS_FILES ${INTENTCLIP_TS_FILES}
    QM_FILES_OUTPUT_VARIABLE intentclip_qm_files
    QM_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/translations
    LUPDATE_OPTIONS "-locations" "relative"
)
# 保证只构建 IntentClip 目标时也会先生成 .qm
add_dependencies(IntentClip IntentClip_lrelease)

# 构建后：把应用 .qm 复制到可执行文件旁的 translations/ 目录
add_custom_command(TARGET IntentClip POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:IntentClip>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${intentclip_qm_files}
        "$<TARGET_FILE_DIR:IntentClip>/translations"
    COMMENT "Deploying application translation catalogs (.qm)"
    VERBATIM
)

# ============================================================
# 安装规则
install(TARGETS IntentClip IntentClipInference
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 运行时 DLL（Qt、llama、ggml 及传递依赖）来自构建输出目录，
# 该目录已由 POST_BUILD 阶段的 windeployqt 补齐，安装时整目录拷贝。
install(DIRECTORY "$<TARGET_FILE_DIR:IntentClip>/"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    FILES_MATCHING
        PATTERN "*.dll"
)

# 翻译：应用 .qm 安装到可执行文件旁的 translations/ 目录
install(FILES ${intentclip_qm_files}
    DESTINATION ${CMAKE_INSTALL_BINDIR}/translations
)

# 配置与模型：安装到可执行文件旁的 config/ 与 models/
install(
    FILES config/functions.default.json
    DESTINATION ${CMAKE_INSTALL_BINDIR}/config
)
install(
    FILES "${INTENTCLIP_MODEL_FILE}"
    DESTINATION ${CMAKE_INSTALL_BINDIR}/models
)

# 安装阶段自动收集 Qt 运行时依赖（插件等）
qt_generate_deploy_app_script(
    TARGET IntentClip
    OUTPUT_SCRIPT intentclip_qt_deploy_script
)
install(CODE "set(QT_DEPLOY_PLUGINS_DIR \"${CMAKE_INSTALL_BINDIR}\")")
install(CODE "set(QT_DEPLOY_TRANSLATIONS_DIR \"${CMAKE_INSTALL_BINDIR}/translations\")")
install(SCRIPT ${intentclip_qt_deploy_script})
