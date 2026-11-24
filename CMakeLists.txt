cmake_minimum_required(VERSION 3.24)
project(DRAGON_AC LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(EMBEDDED_ASSETS "${GENERATED_DIR}/embedded_assets.h")
file(MAKE_DIRECTORY "${GENERATED_DIR}")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_assets.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Splash.gif"
          "${CMAKE_CURRENT_SOURCE_DIR}/Assets/FoRgE.ttf"
          "${EMBEDDED_ASSETS}"
  RESULT_VARIABLE EMBED_RESULT
)
if(NOT EMBED_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to convert and embed splash assets")
endif()
add_executable(DRAGON-AC WIN32 src/main.cpp "${EMBEDDED_ASSETS}")
target_include_directories(DRAGON-AC PRIVATE "${GENERATED_DIR}")
target_compile_features(DRAGON-AC PRIVATE cxx_std_17)
target_compile_definitions(DRAGON-AC PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(DRAGON-AC PRIVATE gdiplus msimg32 ole32)
set_target_properties(DRAGON-AC PROPERTIES OUTPUT_NAME "DRAGON-AC")
if(MSVC)
  target_compile_options(DRAGON-AC PRIVATE /W4 /permissive- /utf-8 /EHsc)
  set_property(TARGET DRAGON-AC PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
