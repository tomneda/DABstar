
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
