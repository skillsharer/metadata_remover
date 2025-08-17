cmake_minimum_required(VERSION 3.19)
project(cleanmeta LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add Homebrew prefix to CMAKE_PREFIX_PATH for finding packages
if(APPLE)
    execute_process(COMMAND brew --prefix OUTPUT_VARIABLE HOMEBREW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX};${CMAKE_PREFIX_PATH}")
endif()

# Find required packages
find_package(exiv2 REQUIRED)

# Find FLTK using fltk-config
find_program(FLTK_CONFIG fltk-config)
if(NOT FLTK_CONFIG)
    message(FATAL_ERROR "fltk-config not found. Please install FLTK.")
endif()

# Get FLTK compile and link flags
execute_process(COMMAND ${FLTK_CONFIG} --cxxflags OUTPUT_VARIABLE FLTK_CXX_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${FLTK_CONFIG} --ldflags OUTPUT_VARIABLE FLTK_LD_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)

# Parse FLTK flags - extract libraries and frameworks separately
string(REPLACE " " ";" FLTK_CXX_FLAGS_LIST ${FLTK_CXX_FLAGS})

# Extract library directories and libraries
string(REGEX MATCHALL "-L[^ ]+" FLTK_LIB_DIRS ${FLTK_LD_FLAGS})
string(REGEX MATCHALL "-l[^ ]+" FLTK_LIBS ${FLTK_LD_FLAGS})

# Find frameworks
find_library(COCOA_FRAMEWORK Cocoa)
find_library(UNIFORMTYPEIDENTIFIERS_FRAMEWORK UniformTypeIdentifiers)
find_library(SCREENCAPTUREKIT_FRAMEWORK ScreenCaptureKit)

# CLI version
add_executable(cleanmeta src/main.cpp)
target_link_libraries(cleanmeta PRIVATE Exiv2::exiv2lib)

# GUI version using FLTK
add_executable(cleanmeta-gui src/gui_main.cpp)
target_link_libraries(cleanmeta-gui PRIVATE Exiv2::exiv2lib)
target_compile_options(cleanmeta-gui PRIVATE ${FLTK_CXX_FLAGS_LIST})
target_include_directories(cleanmeta-gui PRIVATE src)

# Add FLTK library directories
foreach(lib_dir ${FLTK_LIB_DIRS})
    string(SUBSTRING ${lib_dir} 2 -1 lib_path)
    target_link_directories(cleanmeta-gui PRIVATE ${lib_path})
endforeach()

# Add FLTK libraries
foreach(lib ${FLTK_LIBS})
    string(SUBSTRING ${lib} 2 -1 lib_name)
    target_link_libraries(cleanmeta-gui PRIVATE ${lib_name})
endforeach()

# Add frameworks
if(COCOA_FRAMEWORK)
    target_link_libraries(cleanmeta-gui PRIVATE ${COCOA_FRAMEWORK})
endif()
if(UNIFORMTYPEIDENTIFIERS_FRAMEWORK)
    target_link_libraries(cleanmeta-gui PRIVATE ${UNIFORMTYPEIDENTIFIERS_FRAMEWORK})
endif()
if(SCREENCAPTUREKIT_FRAMEWORK)
    target_link_libraries(cleanmeta-gui PRIVATE ${SCREENCAPTUREKIT_FRAMEWORK})
endif()

# Add threading support
find_package(Threads REQUIRED)
target_link_libraries(cleanmeta-gui PRIVATE Threads::Threads)

# Install both versions
install(TARGETS cleanmeta cleanmeta-gui RUNTIME DESTINATION .)

# Where to "install" the portable bundle
include(GNUInstallDirs)

# Put the CLI at the root of the bundle folder
install(TARGETS cleanmeta RUNTIME DESTINATION .)

# Bundle qpdf binary into ./bin/qpdf at install time
find_program(QPDF_BIN qpdf)
if (NOT QPDF_BIN)
  message(FATAL_ERROR "qpdf not found. On macOS: brew install qpdf")
endif()
install(PROGRAMS ${QPDF_BIN} DESTINATION bin)

# Make runtime lookup friendly when running from the folder
set(CMAKE_INSTALL_RPATH "@executable_path;@executable_path/../lib")

# After install, copy dependent dylibs and rewrite their install names
install(CODE "
  include(\"${CMAKE_CURRENT_LIST_DIR}/cmake/Bundle.cmake\")
  message(STATUS \"Fixing up bundle (dylibs + rpaths)...\")
  fixup_cli_bundle(\"${CMAKE_INSTALL_PREFIX}/cleanmeta\")
")
