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

add_executable(host_app
# host app sources
    src/main.cpp
    src/image.cpp
    src/image.hpp
    src/host_app.cpp
    src/host_app.h
    src/opengl.hpp
    src/loading.xpm
# RCRL sources
    src/rcrl/rcrl.h
    src/rcrl/rcrl.cpp
    src/rcrl/rcrl_parser.h
    src/rcrl/rcrl_parser.cpp
# imgui integration
    src/third_party/imgui/backends/imgui_impl_sdl.cpp
    src/third_party/imgui/backends/imgui_impl_opengl3.cpp
)


# add an include dir for ease of use
target_include_directories(host_app PUBLIC src)

# enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(host_app PRIVATE -Wall -Wextra)
else()
    target_compile_options(host_app PRIVATE /W4)
endif()

# defines needed for RCRL integration
target_compile_definitions(host_app PRIVATE "RCRL_PLUGIN_NAME=\"plugin\"")
target_compile_definitions(host_app PRIVATE "RCRL_EXTENSION=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\"")
if(${CMAKE_GENERATOR} MATCHES "Visual Studio" OR ${CMAKE_GENERATOR} MATCHES "Xcode")
    target_compile_definitions(host_app PRIVATE "RCRL_CONFIG=\"$<CONFIG>\"")
endif()
# unimportant - to construct a path to the fonts in the third party imgui folder
target_compile_definitions(host_app PRIVATE "CMAKE_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")

# so the host app exports symbols from its headers
target_compile_definitions(host_app PRIVATE "HOST_APP")

####################################################################################################
# third party libs
####################################################################################################

# imgui
add_library(imgui STATIC src/third_party/imgui/imgui.cpp src/third_party/imgui/imgui_draw.cpp src/third_party/imgui/imgui_widgets.cpp src/third_party/imgui/imgui_tables.cpp)
target_include_directories(imgui INTERFACE src/third_party/imgui)

# ImGuiColorTextEdit
add_library(ImGuiColorTextEdit STATIC src/third_party/ImGuiColorTextEdit/TextEditor.cpp src/third_party/ImGuiColorTextEdit/TextEditor.h)
target_link_libraries(ImGuiColorTextEdit PRIVATE imgui)

# folders for the third party libs
set_target_properties(imgui PROPERTIES FOLDER "third_party")
set_target_properties(ImGuiColorTextEdit PROPERTIES FOLDER "third_party")

# link host app to third party libs
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem)
find_package(Threads REQUIRED)
find_package(GLEW REQUIRED)
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package (LibClang REQUIRED)
target_include_directories(host_app PRIVATE ${Boost_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
string(REPLACE " " ";" __LIST ${LIBCLANG_CXXFLAGS})
target_compile_options(host_app PRIVATE ${__LIST})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DIMGUI_IMPL_OPENGL_LOADER_GLEW")
target_link_libraries(host_app PRIVATE imgui ImGuiColorTextEdit
  ${GLEW_LIBRARIES}
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
  ${Boost_LIBRARIES}
  ${LIBCLANG_LIBRARIES}
  ${SDL2_LIBRARIES}
  ${OPENGL_LIBRARIES})

####################################################################################################
# tests
####################################################################################################

option(RCRL_WITH_TESTS "Build tests for RCRL" OFF)
if(RCRL_WITH_TESTS)
    enable_testing()
	add_subdirectory(tests)
endif()
