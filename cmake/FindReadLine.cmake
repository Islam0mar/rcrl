# find Readline (terminal input library) includes and library
#
# READLINE_INCLUDE_DIR - where the directory containing the READLINE headers can
# be found READLINE_LIBRARY     - full path to the READLINE library
# READLINE_FOUND       - TRUE if READLINE was found
find_path(READLINE_INCLUDE_DIR readline/readline.h)
find_library(READLINE_LIBRARY NAMES readline)

if(READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
  set(READLINE_FOUND TRUE)
  message(STATUS "Found Readline library: ${READLINE_LIBRARY}")
  message(STATUS "Include dir is: ${READLINE_INCLUDE_DIR}")
  include_directories(${READLINE_INCLUDE_DIR})
else(READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
  set(READLINE_FOUND FALSE)
  message(
    FATAL_ERROR
      "** Readline library not found!\n** Your distro may provide a binary for Readline e.g. for ubuntu try apt-get install libreadline5-dev"
  )
endif(READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
