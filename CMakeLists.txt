cmake_minimum_required(VERSION 3.25)
project(Fishy)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 设置FBX SDK路径
#set(FBXSDK_DIR "${PROJECT_SOURCE_DIR}/x64/fbxsdk")
# 设置Qt路径
set(CMAKE_PREFIX_PATH "E:\\Qt\\QtEdu\\6.6.2\\msvc2019_64")

find_package(assimp CONFIG REQUIRED)
find_package(OpenMP REQUIRED)

find_package(Qt6 COMPONENTS REQUIRED
        Core
        Gui
        Widgets
        3DCore
        3dextras
        3drender
        3DInput
        )

file(GLOB_RECURSE HEADERS
        "src/cameras/*.h"
        "src/core/*.h"
        "src/materials/*.h"
        "src/lights/*.h"
        "src/samplers/*.h"
        "src/shapes/*.h"
        "src/fishy/*.h"
        "src/utilities/*.h"
        "src/Accelerator/*.h"
)

add_executable(Fishy
        src/Fishy.qrc
        src/Fishy.ui
        ${HEADERS}
        main.cpp
        src/FishyRenderer.cpp
        src/shapes/Sphere.cpp
        src/core/Integrator.cpp
        src/core/Film.cpp
        src/core/BSDF.cpp
        src/shapes/Triangle.cpp
        src/shapes/ModelLoader.cpp
        src/fishy/SceneManager.cpp
        src/shapes/Plane.cpp
        src/Accelerator/BVHAccel.cpp
)

#target_include_directories(Fishy PRIVATE "${FBXSDK_DIR}/include")
#link_directories("${FBXSDK_DIR}/debug")

target_link_libraries(Fishy
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::3DCore
        Qt::3DInput
        Qt::3DExtras
        Qt::3DRender
        assimp::assimp
        OpenMP::OpenMP_CXX
)


if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()
    foreach (QT_LIB Core Gui Widgets)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach (QT_LIB)
endif ()
