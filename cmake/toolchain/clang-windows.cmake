# Clang + Ninja toolchain for Windows (MSVC ABI, links prebuilt .lib from CPP_LIBRARY_ROOT).
# Requires: LLVM/Clang, Windows SDK, Ninja.
# C++ standard library: auto-detected by Clang from MSVC Build Tools if installed.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(NOT DEFINED LLVM_ROOT)
    if(DEFINED ENV{LLVM_ROOT})
        set(LLVM_ROOT "$ENV{LLVM_ROOT}")
    else()
        set(LLVM_ROOT "C:/Program Files/LLVM")
    endif()
endif()

set(_clang "${LLVM_ROOT}/bin/clang.exe")
set(_clangxx "${LLVM_ROOT}/bin/clang++.exe")
set(_rc "${LLVM_ROOT}/bin/llvm-rc.exe")

if(NOT EXISTS "${_clang}")
    message(FATAL_ERROR "Clang not found at ${_clang}. Install LLVM or set LLVM_ROOT.")
endif()

set(CMAKE_C_COMPILER "${_clang}" CACHE FILEPATH "C compiler" FORCE)
set(CMAKE_CXX_COMPILER "${_clangxx}" CACHE FILEPATH "C++ compiler" FORCE)
if(EXISTS "${_rc}")
    set(CMAKE_RC_COMPILER "${_rc}" CACHE FILEPATH "Resource compiler" FORCE)
endif()

# Verify MSVC STL is available (required for prebuilt third-party .lib files).
set(_vswhere "C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe")
if(NOT EXISTS "${_vswhere}")
    set(_vswhere "C:/Program Files/Microsoft Visual Studio/Installer/vswhere.exe")
endif()

set(_has_vc_tools FALSE)
if(EXISTS "${_vswhere}")
    execute_process(
        COMMAND "${_vswhere}" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64
                -property installationPath
        OUTPUT_VARIABLE _vs_install
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(_vs_install AND EXISTS "${_vs_install}/VC/Tools/MSVC")
        set(_has_vc_tools TRUE)
    endif()
endif()

if(NOT _has_vc_tools)
    message(FATAL_ERROR
        "MSVC C++ runtime not found.\n"
        "Install \"Microsoft C++ Build Tools\" with the Desktop C++ workload (IDE not required).\n"
        "Prebuilt third-party .lib files in CPP_LIBRARY_ROOT use the MSVC ABI.")
endif()

set(CMAKE_C_FLAGS_INIT "-target x86_64-pc-windows-msvc -finput-charset=UTF-8")
set(CMAKE_CXX_FLAGS_INIT "-target x86_64-pc-windows-msvc -finput-charset=UTF-8")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-target x86_64-pc-windows-msvc -fuse-ld=lld-link")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-target x86_64-pc-windows-msvc -fuse-ld=lld-link")
