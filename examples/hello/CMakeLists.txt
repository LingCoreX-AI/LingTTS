# LingTTS 最小例子：引擎 API（lingtts_engine.h），几句文本 → I2S 播放
cmake_minimum_required(VERSION 3.16)
# SDK 发布包里用预编译组件 components/lingtts 和 assets/；在研发仓库里用 lingtts/ 源码。
# GitHub 公开仓库只有示例源码，要在解压后的 SDK 包里构建。
set(LT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
if(EXISTS "${LT_ROOT}/components/lingtts")
    set(EXTRA_COMPONENT_DIRS "${LT_ROOT}/components/lingtts")
    set(LT_ASSETS "${LT_ROOT}/assets")
elseif(EXISTS "${LT_ROOT}/lingtts")
    set(EXTRA_COMPONENT_DIRS "${LT_ROOT}/lingtts" "${LT_ROOT}/third_party/esp-nn")
    set(LT_ASSETS "${LT_ROOT}/firmware/lingtts_bench/assets")
else()
    message(FATAL_ERROR "找不到 LingTTS 组件。请从 GitHub Releases 下载 SDK 包（lingtts-sdk-*.tar.gz），"
                        "解压后在包里的 examples/ 目录构建本示例。")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lingtts_hello)

# idf.py flash 时把模型和词典写进 lingtts_model / lingtts_fe 分区；
# -D LINGTTS_LICENSE=/绝对路径/license.bin 时把授权写进 lingtts_lic，社区版默认不读取该分区；无需传入此参数
set(LINGTTS_ASSET_DIR "${LT_ASSETS}" CACHE PATH "lingtts.bin / lingtts_fe.bin 所在目录")
set(LINGTTS_LICENSE "" CACHE FILEPATH "设备授权 license.bin")
lingtts_flash_assets("${LINGTTS_ASSET_DIR}" "${LINGTTS_LICENSE}")
