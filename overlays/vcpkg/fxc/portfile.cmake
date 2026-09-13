# Overlay port. The upstream carbonengine/vcpkg-registry fxc port downloads from
# https://vcpkg-prebuilt-sdks.ccpgames.com which does not resolve for external
# developers. FXC is the Microsoft HLSL compiler already shipped in the Windows SDK.

set(_fxc_candidates
  "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe"
  "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/fxc.exe"
  "C:/Program Files (x86)/Windows Kits/10/bin/10.0.19041.0/x64/fxc.exe"
  "C:/Program Files (x86)/Windows Kits/10/bin/10.0.18362.0/x64/fxc.exe"
  "C:/Program Files (x86)/Windows Kits/10/bin/10.0.17763.0/x64/fxc.exe"
)

set(FXC_EXE "")
foreach(_candidate IN LISTS _fxc_candidates)
  if(EXISTS "${_candidate}")
    set(FXC_EXE "${_candidate}")
    break()
  endif()
endforeach()

if(NOT FXC_EXE)
  message(FATAL_ERROR "Windows SDK fxc.exe not found. Install a Windows 10/11 SDK with the Desktop C++ tools.")
endif()

set(_d3d_candidates
  "C:/Program Files (x86)/Windows Kits/10/Redist/D3D/x64/d3dcompiler_47.dll"
  "C:/Windows/System32/D3DCompiler_47.dll"
)

set(D3DCOMPILER_DLL "")
foreach(_candidate IN LISTS _d3d_candidates)
  if(EXISTS "${_candidate}")
    set(D3DCOMPILER_DLL "${_candidate}")
    break()
  endif()
endforeach()

if(NOT D3DCOMPILER_DLL)
  message(FATAL_ERROR "d3dcompiler_47.dll not found next to the Windows SDK.")
endif()

message(STATUS "fxc overlay using ${FXC_EXE}")
message(STATUS "fxc overlay using ${D3DCOMPILER_DLL}")

file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/tools/${PORT}")
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/bin")
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/share/${PORT}")

file(COPY "${FXC_EXE}" DESTINATION "${CURRENT_PACKAGES_DIR}/tools/${PORT}")
file(COPY "${D3DCOMPILER_DLL}" DESTINATION "${CURRENT_PACKAGES_DIR}/bin")
file(RENAME
  "${CURRENT_PACKAGES_DIR}/bin/d3dcompiler_47.dll"
  "${CURRENT_PACKAGES_DIR}/bin/D3dCompiler_47.dll"
)

file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright" "Microsoft Windows SDK / DirectX compiler redistributable.\n")
file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" "fxc tool can be used with the following:\n\n    find_program(FXC_TOOL fxc REQUIRED)\n")
