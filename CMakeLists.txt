cmake_minimum_required(VERSION 3.16)

if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

project(AICopilotLogger CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(XPLM_SDK "${CMAKE_SOURCE_DIR}/sdk/SDK" CACHE PATH "Lokasi X-Plane SDK")

add_library(AICopilotLogger MODULE src/logger.cpp)
target_include_directories(AICopilotLogger PRIVATE "${XPLM_SDK}/CHeaders/XPLM")
target_compile_definitions(AICopilotLogger PRIVATE XPLM200=1 XPLM210=1 XPLM300=1)
set_target_properties(AICopilotLogger PROPERTIES PREFIX "" SUFFIX ".xpl")

if(WIN32)
  target_compile_definitions(AICopilotLogger PRIVATE IBM=1)
  target_link_libraries(AICopilotLogger PRIVATE "${XPLM_SDK}/Libraries/Win/XPLM_64.lib")
elseif(APPLE)
  target_compile_definitions(AICopilotLogger PRIVATE APL=1)
  target_compile_options(AICopilotLogger PRIVATE -fvisibility=hidden)
  target_link_options(AICopilotLogger PRIVATE
    "-F${XPLM_SDK}/Libraries/Mac" -framework XPLM -undefined dynamic_lookup)
else()
  target_compile_definitions(AICopilotLogger PRIVATE LIN=1)
  target_compile_options(AICopilotLogger PRIVATE -fvisibility=hidden)
  target_link_options(AICopilotLogger PRIVATE -static-libgcc -static-libstdc++)
endif()
