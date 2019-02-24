# File containing various utilities

# Returns a list of arguments that evaluate to true
function(count_true output_count_var)
  set(lst)
  foreach(option_var IN LISTS ARGN)
    if(${option_var})
      list(APPEND lst ${option_var})
    endif()
  endforeach()
  list(LENGTH lst lst_len)
  set(${output_count_var} ${lst_len} PARENT_SCOPE)
endfunction()

# Returns the ${CMAKE_C_COMPILER} prefix
function(c_compiler_prefix output_prefix)
  string(REGEX MATCH "[^/]+$" compiler_basename ${CMAKE_C_COMPILER})
  string(REGEX MATCH "[^-]+$" compiler_id ${compiler_basename})
  if(NOT ${compiler_basename} STREQUAL ${compiler_id})
    string(REPLACE "-${compiler_id}" "" compiler_prefix ${compiler_basename})
  endif()
  set(${output_prefix} ${compiler_prefix} PARENT_SCOPE)
endfunction()

# Returns the ${CMAKE_C_COMPILER} machine infomation
function(c_compiler_machine output_machine)
  execute_process(
    COMMAND ${CMAKE_C_COMPILER} -dumpmachine
    OUTPUT_VARIABLE compiler_machine
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  set(${output_machine} ${compiler_machine} PARENT_SCOPE)
endfunction()

# Get git commitid
function(get_commit_id output_commit_id)
  execute_process(COMMAND git describe --always --tags --long --dirty --abbrev=12
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE local_commit_id
    RESULT_VARIABLE local_command_result
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    )
  set(${output_commit_id} ${local_commit_id} PARENT_SCOPE)
endfunction()

# Get subdirectory list
macro(SUBDIRLIST result curdir)
  file(GLOB children RELATIVE ${curdir} ${curdir}/*)
  set(dirlist "")
  foreach(child ${children})
    if(IS_DIRECTORY ${curdir}/${child})
      list(APPEND dirlist ${child})
    endif()
  endforeach()
  set(${result} ${dirlist})
endmacro()

