# Enables clang-tidy static analysis on a target.
#
# For Ninja / Makefile generators (CI): uses CXX_CLANG_TIDY (CMake built-in),
#   so clang-tidy runs on every translation unit as part of the build.
# For Visual Studio generators (local): CXX_CLANG_TIDY is silently ignored by
#   CMake. Instead, sets EnableClangTidyCodeAnalysis and ClangTidyToolExe on the
#   VS project so the analyser is available on-demand via
#   Build → Run Code Analysis on Solution. VS picks up .clang-tidy automatically.
#
# Usage: enable_clang_tidy(<target>)
#
# Opt-in: clang-tidy is only activated when -DENABLE_CLANG_TIDY=ON is passed to
# cmake (or set in a preset). When OFF the function is a no-op so developers
# without LLVM installed can still configure and build normally.
option(ENABLE_CLANG_TIDY "Run clang-tidy on every translation unit during the build" OFF)

function(enable_clang_tidy target)
    if(NOT ENABLE_CLANG_TIDY)
        return()
    endif()

    find_program(CLANG_TIDY_EXE NAMES clang-tidy clang-tidy.exe)

    if(NOT CLANG_TIDY_EXE)
        message(FATAL_ERROR
            "ENABLE_CLANG_TIDY=ON but clang-tidy was not found on PATH. "
            "Install LLVM from https://releases.llvm.org and ensure it is on PATH."
        )
    endif()

    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        # VS native integration: registers clang-tidy as the code analyser for
        # this target. Analysis runs on-demand via
        # Build → Run Code Analysis on Solution
        # (output goes to the Code Analysis Results window / Error List).
        # RunCodeAnalysis is intentionally omitted so normal builds are unaffected.
        set_target_properties(${target} PROPERTIES
            VS_GLOBAL_EnableClangTidyCodeAnalysis "true"
            VS_GLOBAL_ClangTidyToolExe            "${CLANG_TIDY_EXE}"
        )
    else()
        # For Ninja/Makefile generators, CMake's __run_co_compile mechanism strips
        # the compiler executable (cl.exe) before invoking clang-tidy, so clang-tidy
        # cannot auto-detect cl-compatibility mode and /EHsc does not enable exceptions.
        # Instead, clang-tidy is run separately in CI via run-clang-tidy which reads
        # compile_commands.json (full "cl.exe ..." command) and auto-detects cl mode:
        #   run-clang-tidy -p build driver_files/src
        message(STATUS
            "clang-tidy (${CLANG_TIDY_EXE}): run separately via "
            "'run-clang-tidy -p build driver_files/src'")
    endif()

    message(STATUS "clang-tidy enabled for target '${target}': ${CLANG_TIDY_EXE}")
endfunction()
