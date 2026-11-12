cmake_minimum_required(VERSION 3.16)
project(aetheria VERSION 0.5.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
add_compile_options(-Wall -Wextra -Wpedantic)

add_library(aetheria-model STATIC src/Model.cpp)
target_include_directories(aetheria-model PUBLIC src)

# Supports systems without the optional development headers, where libSDL2 is
# already installed as a runtime. Prefer the official SDL2 headers when present.
find_path(SDL2_HEADER SDL2/SDL.h)
find_library(SDL2_LIBRARY NAMES SDL2 SDL2-2.0)
if(NOT SDL2_LIBRARY)
  find_file(SDL2_LIBRARY NAMES libSDL2-2.0.so.0
    PATHS /lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu /lib /usr/lib)
endif()
if(NOT SDL2_LIBRARY)
  message(FATAL_ERROR "SDL2 runtime missing: install libsdl2-2.0-0 (and ideally libsdl2-dev)")
endif()
add_executable(aetheria src/main.cpp src/Font.cpp src/Scene3D.cpp src/Sound.cpp)
target_link_libraries(aetheria PRIVATE aetheria-model ${SDL2_LIBRARY})
if(SDL2_HEADER)
  target_include_directories(aetheria PRIVATE ${SDL2_HEADER})
  target_compile_definitions(aetheria PRIVATE WENUY_USE_SYSTEM_SDL=1)
endif()
install(TARGETS aetheria RUNTIME DESTINATION bin)

enable_testing()
add_executable(aetheria-model-tests tests/test_model.cpp)
target_link_libraries(aetheria-model-tests PRIVATE aetheria-model)
target_compile_options(aetheria-model-tests PRIVATE -UNDEBUG)
add_test(NAME aetheria-model COMMAND aetheria-model-tests)

add_executable(aetheria-stage4-tests tests/test_stage4.cpp)
target_link_libraries(aetheria-stage4-tests PRIVATE aetheria-model)
target_compile_options(aetheria-stage4-tests PRIVATE -UNDEBUG)
add_test(NAME aetheria-stage4 COMMAND aetheria-stage4-tests)

add_executable(aetheria-stage5-tests tests/test_stage5.cpp)
target_link_libraries(aetheria-stage5-tests PRIVATE aetheria-model)
target_compile_options(aetheria-stage5-tests PRIVATE -UNDEBUG)
add_test(NAME aetheria-stage5 COMMAND aetheria-stage5-tests)

# Pure headless UI geometry and mouse interaction regressions (C++17, no SDL).
add_executable(aetheria-ui-layout-tests tests/test_ui_layout.cpp)
target_include_directories(aetheria-ui-layout-tests PRIVATE src)
add_test(NAME aetheria-ui-layout COMMAND aetheria-ui-layout-tests)
