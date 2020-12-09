# this file should be used from the top CMakeLists.txt of the repository and assumes:
# - relative paths are correct
# - tiny-process-library is present
# - the plugin_file variable is defined

# parser tests
add_executable(rcrl_parser_tests ../src/rcrl/rcrl_parser.cpp parser_tests.cpp)
add_test(NAME rcrl_parser_tests COMMAND rcrl_parser_tests)

# compiler tests
add_executable(rcrl_compiler_tests ../src/rcrl/rcrl.cpp ../src/rcrl/rcrl_parser.cpp compiler_tests.cpp)
# needed defines
target_compile_definitions(rcrl_compiler_tests PRIVATE "RCRL_PLUGIN_FILE=\"${plugin_file}\"")
target_compile_definitions(rcrl_compiler_tests PRIVATE "RCRL_PLUGIN_NAME=\"test_plugin\"")
target_compile_definitions(rcrl_compiler_tests PRIVATE "RCRL_BUILD_FOLDER=\"${PROJECT_BINARY_DIR}\"")
target_compile_definitions(rcrl_compiler_tests PRIVATE "RCRL_BIN_FOLDER=\"$<TARGET_FILE_DIR:rcrl_compiler_tests>/\"")
target_compile_definitions(rcrl_compiler_tests PRIVATE "RCRL_EXTENSION=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\"")
if(${CMAKE_GENERATOR} MATCHES "Visual Studio" OR ${CMAKE_GENERATOR} MATCHES "Xcode")
	target_compile_definitions(rcrl_compiler_tests PRIVATE "RCRL_CONFIG=\"$<CONFIG>\"")
endif()
# link to clang
target_link_libraries(rcrl_compiler_tests PRIVATE ${LIBCLANG_LIBRARIES})
target_compile_options(rcrl_compiler_tests  PRIVATE ${LIBCLANG_CXXFLAGS})

# link to dl for dlopen() and dlclose()
if(UNIX AND NOT APPLE)
    target_link_libraries(rcrl_compiler_tests PRIVATE dl clang ${LIBCLANG_LIBRARIES})
endif()

# export symbols from executable
set_target_properties(rcrl_compiler_tests PROPERTIES ENABLE_EXPORTS ON)
# add include directory
target_include_directories(rcrl_compiler_tests PUBLIC ../src)

add_test(NAME rcrl_compiler_tests COMMAND rcrl_compiler_tests)

# folders for the third party libs
set_target_properties(rcrl_parser_tests PROPERTIES FOLDER "tests")
set_target_properties(rcrl_compiler_tests PROPERTIES FOLDER "tests")
