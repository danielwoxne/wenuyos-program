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
# WenuyGFX is supplied by WenuyOS (0.9); never vendor or copy the engine.
# Optional during migration, so older test hosts can still run the original SDL game.
option(AETHERIA_WENUYGFX "Use the system WenuyGFX 0.9 SDK when installed" ON)
if(AETHERIA_WENUYGFX)
  find_package(WenuyGFX 0.9 CONFIG QUIET)
endif()
add_executable(aetheria src/main.cpp src/Font.cpp src/Scene3D.cpp src/Sound.cpp)
if(WenuyGFX_FOUND)
  target_sources(aetheria PRIVATE src/GfxPresentation.cpp)
  target_compile_definitions(aetheria PRIVATE AETHERIA_WITH_WENUYGFX=1)
  target_link_libraries(aetheria PRIVATE WenuyGFX::wenuygfx)
  message(STATUS "Aetheria: WenuyGFX 0.9 enabled (F3 / --wenuygfx)")
else()
  message(STATUS "Aetheria: legacy SDL renderer (WenuyGFX SDK unavailable)")
endif()
target_link_libraries(aetheria PRIVATE aetheria-model ${SDL2_LIBRARY})
# WenuyOS Datatyper is a shared system codec registry, not bundled into Aetheria.
# Optional on non-WenuyOS development hosts; PNG/IFF/etc go through the
# installed codec when available, PPM screenshot fallback remains unchanged.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(AETHERIA_DT QUIET IMPORTED_TARGET wenuy-datatypes)
endif()
if(AETHERIA_DT_FOUND)
  target_sources(aetheria PRIVATE src/DatatypeCapture.cpp)
  target_compile_definitions(aetheria PRIVATE AETHERIA_WITH_DATATYPES=1)
  target_link_libraries(aetheria PRIVATE PkgConfig::AETHERIA_DT)
  message(STATUS "Aetheria: WenuyOS Datatyper enabled for image screenshots")
else()
  message(STATUS "Aetheria: Datatyper unavailable, keeping PPM screenshots")
endif()
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
# Verify the REAL Aetheria world geometry can go through the installed
# WenuyGFX 0.9 CPU depth-buffer renderer, without a GPU or a running window.
if(WenuyGFX_FOUND)
  add_executable(aetheria-wenuygfx-tests tests/test_wenuygfx.cpp src/Scene3D.cpp src/Font.cpp)
  target_include_directories(aetheria-wenuygfx-tests PRIVATE src)
  target_compile_definitions(aetheria-wenuygfx-tests PRIVATE AETHERIA_WITH_WENUYGFX=1)
  target_link_libraries(aetheria-wenuygfx-tests PRIVATE aetheria-model
                              WenuyGFX::wenuygfx ${SDL2_LIBRARY})
  add_test(NAME aetheria-wenuygfx-scene COMMAND aetheria-wenuygfx-tests)
  set_tests_properties(aetheria-wenuygfx-scene PROPERTIES
     ENVIRONMENT "WENUYOS_PERF=low;WENUYOS_GFX=soft")
endif()
# No SDL, GDK or network required to check the pixel conversion contract.
add_executable(aetheria-datatype-pixels-tests tests/test_datatype_pixels.cpp)
target_include_directories(aetheria-datatype-pixels-tests PRIVATE src)
add_test(NAME aetheria-datatype-pixels COMMAND aetheria-datatype-pixels-tests)

# Restored 0.6 gameplay: navigation/zoom/autosave, independent of SDL.
add_executable(aetheria-stage6-tests tests/test_stage6.cpp)
target_link_libraries(aetheria-stage6-tests PRIVATE aetheria-model)
target_compile_options(aetheria-stage6-tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
add_test(NAME aetheria-stage6-restored-controls COMMAND aetheria-stage6-tests)
