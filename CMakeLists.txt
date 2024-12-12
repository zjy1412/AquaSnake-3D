cmake_minimum_required(VERSION 3.16)

project(AquaSnake3D VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加这些配置来禁用Vulkan相关检查
set(QT_FEATURE_vulkan OFF)
set(BUILD_WITH_VULKAN OFF)

find_package(Qt6 COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets REQUIRED)
find_package(OpenGL REQUIRED)

set(SOURCES
    main.cpp
    gamewidget.cpp
    snake.cpp
    obstacle.cpp
    food.cpp    # 添加这行
)

set(HEADERS
    gamewidget.h
    snake.h
    obstacle.h
    food.h      # 添加这行
)

set(GLEW_INCLUDE_DIR "D:/libraries/glew-2.2.0/include")
set(GLEW_LIBRARY "D:/libraries/glew-2.2.0/lib/Release/x64/glew32.lib")
# 添加 GLEW DLL 路径
set(GLEW_DLL "D:/libraries/glew-2.2.0/bin/Release/x64/glew32.dll")

# 修改 GLM 的包含路径，指向正确的目录
set(GLM_INCLUDE_DIR "D:/libraries")  # 修改这行，因为glm.hpp在D:/libraries/glm/glm.hpp
include_directories(${GLM_INCLUDE_DIR})

include_directories(${GLEW_INCLUDE_DIR})

set(CMAKE_AUTOMOC ON)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Qt6::Core 
    Qt6::Gui 
    Qt6::Widgets 
    Qt6::OpenGL 
    Qt6::OpenGLWidgets
    opengl32 
    ${GLEW_LIBRARY} 
    glu32
)

# 添加后构建命令，复制 DLL 到输出目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${GLEW_DLL}
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)