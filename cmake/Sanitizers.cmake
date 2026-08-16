function(gb_set_sanitizers target)
  if(NOT GB_SANITIZE)
    return()
  endif()
  # PUBLIC so the flags reach anything linking gbcore; a static library built
  # with sanitizers and consumed without them does not link.
  if(MSVC)
    target_compile_options(${target} PUBLIC /fsanitize=address)
  else()
    target_compile_options(${target} PUBLIC
      -fsanitize=address,undefined
      -fno-sanitize-recover=all
      -fno-omit-frame-pointer)
    target_link_options(${target} PUBLIC -fsanitize=address,undefined)
  endif()
endfunction()
