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
    src/host_app.cpp
    src/host_app.h
# RCRL sources
    src/rcrl/rcrl.h
    src/rcrl/rcrl.cpp
    src/rcrl/rcrl_parser.h
    src/rcrl/rcrl_parser.cpp
# imgui integration
    src/third_party/imgui/examples/opengl2_example/imgui_impl_glfw_gl2.cpp
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
target_compile_definitions(host_app PRIVATE "RCRL_PLUGIN_FILE=\"${plugin_file}\"")
target_compile_definitions(host_app PRIVATE "RCRL_PLUGIN_NAME=\"plugin\"")
target_compile_definitions(host_app PRIVATE "RCRL_BUILD_FOLDER=\"${PROJECT_BINARY_DIR}\"")
target_compile_definitions(host_app PRIVATE "RCRL_BIN_FOLDER=\"$<TARGET_FILE_DIR:host_app>/\"")
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
add_library(imgui STATIC src/third_party/imgui/imgui.cpp src/third_party/imgui/imgui_draw.cpp)
target_include_directories(imgui INTERFACE src/third_party/imgui)

# ImGuiColorTextEdit
add_library(ImGuiColorTextEdit STATIC src/third_party/ImGuiColorTextEdit/TextEditor.cpp src/third_party/ImGuiColorTextEdit/TextEditor.h)
target_link_libraries(ImGuiColorTextEdit PRIVATE imgui)

# glfw
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
add_subdirectory(src/third_party/glfw)

# folders for the third party libs
set_target_properties(glfw PROPERTIES FOLDER "third_party")
set_target_properties(imgui PROPERTIES FOLDER "third_party")
set_target_properties(ImGuiColorTextEdit PROPERTIES FOLDER "third_party")

# link host app to third party libs
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem)
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package (LibClang REQUIRED)
target_include_directories(host_app PRIVATE ${Boost_INCLUDE_DIR})
string(REPLACE " " ";" __LIST ${LIBCLANG_CXXFLAGS})
target_compile_options(host_app PRIVATE ${__LIST})
target_link_libraries(host_app PRIVATE imgui glfw ImGuiColorTextEdit
  ${Boost_LIBRARIES}
  ${LIBCLANG_LIBRARIES}
  ${OPENGL_LIBRARIES})

####################################################################################################
# tests
####################################################################################################

option(RCRL_WITH_TESTS "Build tests for RCRL" ON)
if(RCRL_WITH_TESTS)
    enable_testing()
	add_subdirectory(tests)
endif()
