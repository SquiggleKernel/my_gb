include(CheckCXXCompilerFlag)

if(NOT MSVC)
  # -Wunknown-warning-option is fatal under -Werror, so probe before using it.
  check_cxx_compiler_flag(-Wno-c2y-extensions GB_HAS_WNO_C2Y)
endif()

function(gb_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    if(GB_WERROR)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic
      -Wshadow -Wconversion -Wsign-conversion
      -Wold-style-cast -Wnon-virtual-dtor -Wcast-align
      -Wunused -Woverloaded-virtual -Wdouble-promotion)
    # Catch2's TEST_CASE expands __COUNTER__, which clang 22 reports as a C2y
    # extension under -Wpedantic. Older clang does not know the flag at all.
    if(GB_HAS_WNO_C2Y)
      target_compile_options(${target} PRIVATE -Wno-c2y-extensions)
    endif()
    if(GB_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()
