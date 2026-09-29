cmake_minimum_required(VERSION 3.10)

project(kiosk-launcher CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(IMGUI_IMPL_OPENGL_ES3)

find_package(SDL3 REQUIRED)
find_package(nlohmann_json REQUIRED)

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/imgui")

add_executable(${PROJECT_NAME}
    main.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 nlohmann_json::nlohmann_json GLESv2)

install(TARGETS kiosk-launcher RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
