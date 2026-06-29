cmake_minimum_required(VERSION 3.16)
project(odmr VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ODMR_ENABLE_ASAN "Enable AddressSanitizer (Debug builds)" OFF)
if(ODMR_ENABLE_ASAN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

find_package(Python3 3.10 REQUIRED COMPONENTS Development)
message(STATUS "Python3: ${Python3_EXECUTABLE} (${Python3_VERSION})")

add_subdirectory(rigol_driver)
add_subdirectory(packet_collector)

# inipp — header-only; фиксируем коммит с cmake_minimum_required >= 3.10 (совместим с CMake 4).
include(FetchContent)
FetchContent_Declare(
    inipp
    GIT_REPOSITORY https://github.com/mcmtroffaes/inipp.git
    GIT_TAG 3f224f1eed7a67d5d7e5fc8cab72de02a056b966
)
FetchContent_MakeAvailable(inipp)

add_executable(cv_odmr
    cv_odmr.cpp
    config_parser/config_parser.cpp
)
add_executable(rabi
    rabi.cpp
    config_parser/config_parser.cpp
)
add_executable(impulse_odmr
    impulse_odmr.cpp
    config_parser/config_parser.cpp
)
add_executable(packet_capture
    packet_capture.cpp
    config_parser/config_parser.cpp
)

set(ODMR_APP_INCLUDES
    ${Python3_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/packet_collector
    ${CMAKE_CURRENT_SOURCE_DIR}/config_parser
    ${inipp_SOURCE_DIR}
)

target_include_directories(cv_odmr PRIVATE ${ODMR_APP_INCLUDES})
target_link_libraries(cv_odmr PRIVATE rigol_driver packet_collector Python3::Python)

target_include_directories(rabi PRIVATE ${ODMR_APP_INCLUDES})
target_link_libraries(rabi PRIVATE rigol_driver packet_collector Python3::Python)

target_include_directories(impulse_odmr PRIVATE ${ODMR_APP_INCLUDES})
target_link_libraries(impulse_odmr PRIVATE rigol_driver packet_collector Python3::Python)

target_include_directories(packet_capture
    PRIVATE
        ${ODMR_APP_INCLUDES}
)
target_link_libraries(packet_capture PRIVATE packet_collector)
