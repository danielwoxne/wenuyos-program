# Matte – lärande matematikprogram för WenuyOS (C++, GTK 4 + libadwaita + libwenuy).
# Byggs i Debian 13 (trixie) för amd64 och levereras som .wapp (se verktyg/bygg-wapp.sh).
# libwenuy följer med som källkod och länkas in statiskt – som Rita – så att Matte fungerar
# oavsett hur gammal WenuyOS-datorns libwenuy är.
cmake_minimum_required(VERSION 3.16)
project(matte VERSION 1.3.0 LANGUAGES CXX)
enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)   # funktionsnamnen kvar för `wenuyos prestanda`
endif()
add_compile_options(-Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter)

# Generate lesson data and fallback icons in the build tree (never in the source).
# Real icon assets take precedence over any placeholder generated below.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(MATTE_FALLBACK_ICONS "${CMAKE_CURRENT_BINARY_DIR}/matte-fallback-icons")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verktyg/generera-saknade-ikoner.py"
          "${CMAKE_CURRENT_SOURCE_DIR}" "${MATTE_FALLBACK_ICONS}"
  RESULT_VARIABLE MATTE_ICONS_RC
  OUTPUT_VARIABLE MATTE_ICONS_LOG
  ERROR_VARIABLE MATTE_ICONS_ERROR)
if(NOT MATTE_ICONS_RC EQUAL 0)
  message(FATAL_ERROR "Matte icon preparation failed: ${MATTE_ICONS_ERROR}")
endif()
message(STATUS "${MATTE_ICONS_LOG}")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verktyg/expandera.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/innehall" "${CMAKE_CURRENT_BINARY_DIR}/matte-share/se.wenuyos.app.matte"
          "${MATTE_FALLBACK_ICONS}"
  RESULT_VARIABLE MATTE_DATA_RC
  OUTPUT_VARIABLE MATTE_DATA_LOG
  ERROR_VARIABLE MATTE_DATA_ERROR)
if(NOT MATTE_DATA_RC EQUAL 0)
  message(FATAL_ERROR "Matte content preparation failed: ${MATTE_DATA_ERROR}")
endif()
message(STATUS "${MATTE_DATA_LOG}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(ADW REQUIRED IMPORTED_TARGET libadwaita-1 gtk4 gio-unix-2.0)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(SQLITE REQUIRED IMPORTED_TARGET sqlite3)
pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua5.4)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

# wenuy-lara: den ämnesoberoende grunden – kort och uppgifter, framsteg och repetition, logg, uppläsning.
# Ett nytt ämne (Svenska …) länkar mot den och lägger till sin egen rättning och sina figurer (docs/NYTT-AMNE.md).
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)
add_library(wenuy-lara STATIC src/Lara.cpp src/Text.cpp src/Json.cpp src/Data.cpp src/Framsteg.cpp src/Logg.cpp src/Uppl.cpp src/Flytt.cpp)
target_include_directories(wenuy-lara PUBLIC src)
target_link_libraries(wenuy-lara PUBLIC PkgConfig::GLIB PkgConfig::GIO)

# Matematiken: tal och bråk, uttryck, rättningen av svar – utan GTK (testas för sig)
add_library(matte-karna STATIC src/Tal.cpp src/Algebra.cpp src/Bedom.cpp)
target_link_libraries(matte-karna PUBLIC wenuy-lara)

# Figurerna: cairo + pango + gdk-pixbuf (utan GTK – testerna ritar dem till PNG)
pkg_check_modules(RITA REQUIRED IMPORTED_TARGET cairo pangocairo gdk-pixbuf-2.0)
add_library(matte-rita STATIC src/Rita.cpp)
target_link_libraries(matte-rita PUBLIC matte-karna PkgConfig::RITA)

# Utskrift: arbetsblad och regelbok som PDF (cairo) – också utan GTK
add_library(matte-utskrift STATIC src/Utskrift.cpp)
target_link_libraries(matte-utskrift PUBLIC matte-rita)

add_executable(matte-test src/Test.cpp)
target_link_libraries(matte-test PRIVATE matte-utskrift)
# Test actual answer checking, learning support, all lesson variants, and PDFs.
# Runs without a graphical display; can be invoked with ctest --test-dir build.
add_test(NAME matte-regression
  COMMAND $<TARGET_FILE:matte-test>
    "${CMAKE_CURRENT_SOURCE_DIR}/tester"
    "${CMAKE_CURRENT_BINARY_DIR}/matte-share/se.wenuyos.app.matte")
set_tests_properties(matte-regression PROPERTIES TIMEOUT 180)

# Regression: real clay icons must win and CMake must leave all source assets untouched.
add_test(NAME matte-icon-assets
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verktyg/test-ikoner.py"
          "${CMAKE_CURRENT_SOURCE_DIR}")
set_tests_properties(matte-icon-assets PROPERTIES TIMEOUT 90)


# libwenuy (WenuyOS bibliotek: fönstergrund, tema, mätning) – källkoden ligger i common/
add_library(wenuy-static STATIC  common/WenuyIcon.cpp common/WenuyWidgets.cpp common/WenuyDoc.cpp common/WenuyCanvas.cpp common/WenuyText.cpp common/WenuySheet.cpp common/WenuyTimeline.cpp common/WenuyWindow.cpp common/WenuyObjects.cpp common/WenuyData.cpp common/WenuyJobs.cpp common/WenuyTheme.cpp common/WenuyScript.cpp common/WenuyMotionIcon.cpp common/WenuyDiag.cpp common/WenuyBackdrop.cpp common/WenuyShell.cpp common/WenuySafe.cpp common/WenuyCalc.cpp common/WenuyMemory.cpp)
target_include_directories(wenuy-static PUBLIC common)
target_link_libraries(wenuy-static PUBLIC PkgConfig::ADW PRIVATE PkgConfig::SQLITE PkgConfig::LUA PkgConfig::SODIUM)

add_executable(matte src/Fonster.cpp src/Svar.cpp src/SvarNy.cpp src/Papper.cpp src/Brakmodell.cpp src/BrakmodellGtk.cpp src/LarandeUtforska.cpp src/LarandeUtforskaGtk.cpp src/SteghjalpGtk.cpp src/FramstegOversikt.cpp src/Hundraruta.cpp src/HundrarutaGtk.cpp src/LektionsytaGtk.cpp src/Mattekarta.cpp src/MattekartaGtk.cpp)
target_link_libraries(matte PRIVATE matte-utskrift wenuy-static)

include(GNUInstallDirs)
install(TARGETS matte RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/matte-share/" DESTINATION ${CMAKE_INSTALL_DATADIR})
