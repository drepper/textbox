cmake_minimum_required(VERSION 3.24)


project(textbox VERSION 0.0 HOMEPAGE_URL "https://github.com/drepper/textbox" LANGUAGES C CXX)

# cmake-lint: disable=C0301

configure_file(config.hh.in config.hh)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "/usr")
endif()

set(debug_opts "-O0 -ggdb3 -D_GLIBCXX_DEBUG -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${debug_opts}")
set(CMAKE_C_FLAGS_DEBUG "${debug_opts}")

set(release_opts "-O3 -ggdb -flto=auto -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${release_opts}")
set(CMAKE_C_FLAGS_RELEASE "${release_opts}")

set(relwithdebinfo_opts "-O2 -ggdb -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${relwithdebinfo_opts}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${relwithdebinfo_opts}")

set(minsizerel_opts "-Os -ggdb -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${minsizerel_opts}")
set(CMAKE_C_FLAGS_MINSIZEREL "${minsizerel_opts}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(compiler_opts "-fsave-optimization-record -frecord-gcc-switches -fno-operator-names")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(compiler_opts "-Wno-unknown-warning-option")
else()
  set(compiler_opts "")
endif()
set(common_warnings "-Wall -Wextra -Wnull-dereference -Wdouble-promotion -Wshadow -Wformat=2 -Wcast-qual -Wcast-align -Wstrict-aliasing -Wpointer-arith -Winit-self -Wredundant-decls -Wundef -Wempty-body -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wrestrict")

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED True)
string(APPEND CMAKE_CXX_FLAGS " ${compiler_opts} ${common_warnings} -Wuseless-cast -Wsuggest-override -Weffc++")

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)
string(APPEND CMAKE_C_FLAGS " ${compiler_opts} ${common_warnings} -Wjump-misses-init -Wabsolute-value")

set(CMAKE_CXX_COMPILER_LAUNCHER ${CMAKE_SOURCE_DIR}/build-tools/launcher.sh)
set(CMAKE_CXX_LINKER_LAUNCHER ${CMAKE_SOURCE_DIR}/build-tools/launcher.sh)
set(CMAKE_C_COMPILER_LAUNCHER ${CMAKE_SOURCE_DIR}/build-tools/launcher.sh)
set(CMAKE_C_LINKER_LAUNCHER ${CMAKE_SOURCE_DIR}/build-tools/launcher.sh)
set(CMAKE_AR ${CMAKE_SOURCE_DIR}/build-tools/launcher-ar.sh)

set(CMAKE_SKIP_TEST_ALL_DEPENDENCY False)

set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

enable_testing()
cmake_policy(SET CMP0110 NEW)

if(NOT TARGET termdetect)
  add_subdirectory(termdetect)
endif()

add_library(textbox textbox.cc textbox.hh srchilite_map.cc srchilite_map.hh)
target_include_directories(textbox PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(textbox PUBLIC termdetect unistring source-highlight)
set_property(TARGET textbox PROPERTY POSITION_INDEPENDENT_CODE TRUE)

if(PROJECT_IS_TOP_LEVEL)
  add_executable(textbox_test textbox_test.cc config.hh)
  target_include_directories(textbox_test PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
  target_link_libraries(textbox_test PUBLIC textbox)
  add_test(NAME "textbox:basic" COMMAND textbox_test)

  add_executable(test_heading_simple test_heading_simple.cc config.hh)
  target_include_directories(test_heading_simple PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
  target_link_libraries(test_heading_simple PUBLIC textbox)

  add_executable(test_visual test_visual.cc config.hh)
  target_include_directories(test_visual PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
  target_link_libraries(test_visual PUBLIC textbox)

  add_executable(test_complete_features test_complete_features.cc config.hh)
  target_include_directories(test_complete_features PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
  target_link_libraries(test_complete_features PUBLIC textbox)

  add_executable(test_table test_table.cc config.hh)
  target_include_directories(test_table PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
  target_link_libraries(test_table PUBLIC textbox)

  add_executable(test_udiff test_udiff.cc config.hh)
  target_include_directories(test_udiff PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
  target_link_libraries(test_udiff PUBLIC textbox)
endif()
