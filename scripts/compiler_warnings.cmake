# Sets strict compiler warnings on a target (MSVC / Windows only).
# Usage: set_compiler_warnings(<target>)
function(set_compiler_warnings target)
    target_compile_options(${target} PRIVATE
        /W4           # High warning level
        /WX           # Warnings as errors
        /permissive-  # Strict standards conformance
        /external:W0  # Suppress warnings from system/third-party headers
    )
endfunction()
