
function(print)
  foreach (var ${ARGN})
    message("${var} = ${${var}}")
  endforeach ()
endfunction()

function(print_env)
  foreach (var ${ARGN})
    message("${var} = $ENV{${var}}")
  endforeach ()
endfunction()

function(get_git_commit_hash)
  if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
            COMMAND git rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE ARG_GIT_COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(GIT_COMMIT_HASH ${ARG_GIT_COMMIT_HASH} PARENT_SCOPE)
  endif ()
endfunction()
