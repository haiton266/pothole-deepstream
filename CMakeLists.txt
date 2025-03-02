# add_subdirectory(yaml-cpp)

cmake_minimum_required(VERSION 3.2.1)

project(MyGStreamerProject LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define DeepStream version and paths
set(DEEPSTREAM_VERSION 6.0)
set(DEEPSTREAM_DIR /opt/nvidia/deepstream/deepstream-${DEEPSTREAM_VERSION})
set(DEEPSTREAM_LIBRARIES_DIR ${DEEPSTREAM_DIR}/lib)
set(DEEPSTREAM_SOURCES_INCLUDES_DIR ${DEEPSTREAM_DIR}/sources/includes)
set(DEEPSTREAM_APPS_COMMON_INCLUDES_DIR ${DEEPSTREAM_DIR}/sources/apps/apps-common/includes)

# Find required packages
find_package(PkgConfig REQUIRED)
# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "/usr/local/include/yaml-cpp")
# find_package(yaml-cpp REQUIRED)

pkg_check_modules(GST REQUIRED gstreamer-1.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GSTRTSP REQUIRED gstreamer-rtsp-server-1.0)

# Display configuration details
message(STATUS "GStreamer RTSP Server version: ${GSTRTSP_VERSION}")
message(STATUS "GStreamer RTSP Server libraries: ${GSTRTSP_LIBRARIES}")
message(STATUS "GStreamer RTSP Server include dirs: ${GSTRTSP_INCLUDE_DIRS}")
message(STATUS "DEEPSTREAM libraries dir: ${DEEPSTREAM_LIBRARIES_DIR}")
message(STATUS "DEEPSTREAM include dirs: ${DEEPSTREAM_SOURCES_INCLUDES_DIR}")
message(STATUS "DEEPSTREAM app common include dirs: ${DEEPSTREAM_APPS_COMMON_INCLUDES_DIR}")

# Include directories
include_directories(
    # ${YAML_CPP_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/utils
    ${CMAKE_SOURCE_DIR}
    ${GST_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${DEEPSTREAM_SOURCES_INCLUDES_DIR}
    ${DEEPSTREAM_APPS_COMMON_INCLUDES_DIR}
    ${GSTRTSP_INCLUDE_DIRS}
)

# Source files
file(GLOB_RECURSE SRC_FILES
    main2.cpp
    utils/*.cpp
)

# Define executable
add_executable(
    main2
    ${SRC_FILES}
)

# Link libraries
target_link_libraries(
    main2 PRIVATE yaml-cpp
    # ${YAML_CPP_LIBRARIES}
    ${GST_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GSTRTSP_LIBRARIES}
    ${DEEPSTREAM_LIBRARIES_DIR}/libnvds_meta.so
    ${DEEPSTREAM_LIBRARIES_DIR}/libnvdsgst_meta.so
    ${DEEPSTREAM_LIBRARIES_DIR}/libnvdsgst_helper.so
    ${DEEPSTREAM_LIBRARIES_DIR}/libnvds_batch_jpegenc.so
    ${DEEPSTREAM_LIBRARIES_DIR}/libnvbufsurface.so
)

# Compiler options
target_compile_options(
    main2
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# Final status message
message(STATUS "Configuration complete. Ready to build ${PROJECT_NAME}")
