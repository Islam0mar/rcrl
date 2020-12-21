cmake_minimum_required(VERSION 3.0.0)

project(RCRL)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


####################################################################################################
# global flags and directories setup
####################################################################################################

# directories - everything goes in the same place
set(OUTPUT_DIR ${PROJECT_BINARY_DIR}/bin/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR})

# latest c++ standards
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb3 -O0 -std=c++17 -fvisibility=hidden")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
endif()

####################################################################################################
# the main executable
####################################################################################################

add_executable(cli_app
# host app sources
    src/main.cpp
# RCRL sources
    src/rcrl/rcrl.cpp
    src/rcrl/rcrl_parser.cpp
)


# add an include dir for ease of use
target_include_directories(cli_app PUBLIC src)

# enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cli_app PRIVATE -Wall -Wextra)
else()
    target_compile_options(cli_app PRIVATE /W4)
endif()

# defines needed for RCRL integration
target_compile_definitions(cli_app PRIVATE "RCRL_PLUGIN_FILE=\"${plugin_file}\"")
target_compile_definitions(cli_app PRIVATE "RCRL_PLUGIN_NAME=\"plugin\"")
target_compile_definitions(cli_app PRIVATE "RCRL_BUILD_FOLDER=\"${PROJECT_BINARY_DIR}\"")
target_compile_definitions(cli_app PRIVATE "RCRL_BIN_FOLDER=\"$<TARGET_FILE_DIR:cli_app>/\"")
target_compile_definitions(cli_app PRIVATE "RCRL_EXTENSION=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\"")
if(${CMAKE_GENERATOR} MATCHES "Visual Studio" OR ${CMAKE_GENERATOR} MATCHES "Xcode")
    target_compile_definitions(cli_app PRIVATE "RCRL_CONFIG=\"$<CONFIG>\"")
endif()

# link host app to third party libs
find_package(Boost REQUIRED COMPONENTS filesystem)
find_package(Threads REQUIRED)
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package (LibClang REQUIRED)
find_package (ReadLine REQUIRED)
target_include_directories(cli_app PRIVATE ${Boost_INCLUDE_DIRS} ${READLINE_INCLUDE_DIR})
string(REPLACE " " ";" __LIST ${LIBCLANG_CXXFLAGS})
target_compile_options(cli_app PRIVATE ${__LIST})
target_link_libraries(cli_app PRIVATE
  ${READLINE_LIBRARY}
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
  ${Boost_LIBRARIES}
  ${LIBCLANG_LIBRARIES})

####################################################################################################
# tests
####################################################################################################

option(RCRL_WITH_TESTS "Build tests for RCRL" ON)
if(RCRL_WITH_TESTS)
    enable_testing()
	add_subdirectory(tests)
endif()
