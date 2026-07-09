# Reusable helper: apply the project's strict warning set to a target.
# Usage: maestro_set_warnings(<target>)
function(maestro_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Werror
      -Wconversion -Wshadow -Wnon-virtual-dtor)
  endif()
endfunction()
