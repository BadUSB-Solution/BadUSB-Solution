cmake_minimum_required(VERSION 3.16)
project(BadUSBGuardianTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BADUSB_WITH_IMGUI "Enable ImGui GUI" OFF)
set(IMGUI_DIR "" CACHE PATH "Path to Dear ImGui")

add_executable(BadUSBGuardianTest
    src/main.cpp
    src/core/InputAnalyzer.cpp
    src/core/DefenseEngine.cpp
    src/core/Config.cpp
    src/core/Logger.cpp
    src/core/KeyboardHook.cpp
    src/gui/MainWindow.cpp
)

target_include_directories(BadUSBGuardianTest PRIVATE src)

target_link_libraries(BadUSBGuardianTest PRIVATE user32 d3d11 dxgi)

add_executable(BadUSBGuardianTests
    tests/DefenseEngineTests.cpp
    src/core/InputAnalyzer.cpp
    src/core/DefenseEngine.cpp
    src/core/Config.cpp
)

target_include_directories(BadUSBGuardianTests PRIVATE src)

if(BADUSB_WITH_IMGUI)
  if (NOT IMGUI_DIR)
    message(FATAL_ERROR "Set IMGUI_DIR to your Dear ImGui folder")
  endif()
  target_compile_definitions(BadUSBGuardianTest PRIVATE BADUSB_WITH_IMGUI)
  target_include_directories(BadUSBGuardianTest PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)
  target_sources(BadUSBGuardianTest PRIVATE
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
  )
endif()
