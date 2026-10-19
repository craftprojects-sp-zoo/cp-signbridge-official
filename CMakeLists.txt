cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0091 NEW)  # MSVC_RUNTIME_LIBRARY
project(cp-signbridge LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Statyczny CRT: DLL-e ładują się do cudzych procesów (Optima, mstsc) — bez zależności od VC Redist
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Diagnostyka z Etapu 0: klucz programowy w KSP (tworzenie kluczy, sbctl create-cert). Nigdy w wydaniu.
option(SB_ENABLE_SOFTKEY "Backend softkey w cpsignbridge.dll (tylko diagnostyka)" OFF)
option(SB_BUILD_TESTS "Testy jednostkowe (tests/, ctest)" OFF)

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0A00)
if(MSVC)
  add_compile_options(/W4 /wd4100 /wd4189 /wd4245 /utf-8 /EHsc /permissive-)
endif()

# Komunikacja serwer <-> komputer z kartą wyłącznie kanałem wirtualnym RDP (dvc.cpp) — bez gniazd sieciowych
set(COMMON_SRV_SRC  src/common/status.cpp src/common/certs.cpp src/common/dvc.cpp src/common/agentclient.cpp src/common/syncops.cpp)
set(COMMON_SIGN_SRC src/common/signer.cpp src/common/certs.cpp src/common/status.cpp src/ksp/log.cpp)
set(WIN_LIBS ncrypt bcrypt crypt32 shell32 advapi32 wtsapi32 version ole32 uuid)

# --- KSP (serwer; biblioteka ładowana przez CNG) — x64 i x86 ---
add_library(cpsignbridge SHARED
  src/ksp/ksp.cpp
  src/ksp/backend_remote.cpp
  src/ksp/backend_composite.cpp
  src/ksp/log.cpp
  src/ksp/ksp.def
  ${COMMON_SRV_SRC}
)
target_include_directories(cpsignbridge PRIVATE src/ksp src/common)
if(SB_ENABLE_SOFTKEY)
  message(WARNING "SB_ENABLE_SOFTKEY=ON: build diagnostyczny, nie do wydania")
  target_sources(cpsignbridge PRIVATE src/ksp/backend_softkey.cpp)
  target_compile_definitions(cpsignbridge PRIVATE SB_ENABLE_SOFTKEY)
endif()
target_link_libraries(cpsignbridge PRIVATE ${WIN_LIBS})
set_target_properties(cpsignbridge PROPERTIES OUTPUT_NAME "cpsignbridge" PREFIX "")

# --- wtyczka mstsc (komputer użytkownika; kanał RDP) — x64 i x86 ---
add_library(cpsignbridge-rdp SHARED
  src/rdpplugin/plugin.cpp
  src/rdpplugin/plugin.def
  ${COMMON_SIGN_SRC}
)
target_include_directories(cpsignbridge-rdp PRIVATE src/common src/ksp)
target_link_libraries(cpsignbridge-rdp PRIVATE ${WIN_LIBS})
set_target_properties(cpsignbridge-rdp PROPERTIES OUTPUT_NAME "cpsignbridge-rdp" PREFIX "")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  # --- sbctl (serwer: instalacja, sync, testy) ---
  add_executable(sbctl src/sbctl/sbctl.cpp src/common/kspsetup.cpp ${COMMON_SRV_SRC})
  target_include_directories(sbctl PRIVATE src/common)
  target_link_libraries(sbctl PRIVATE ${WIN_LIBS})

  # --- sbagent (komputer użytkownika: instalacja wtyczki, lista certyfikatów) ---
  add_executable(sbagent src/agent/agent.cpp src/common/pluginsetup.cpp ${COMMON_SIGN_SRC})
  target_include_directories(sbagent PRIVATE src/common src/ksp)
  target_link_libraries(sbagent PRIVATE ${WIN_LIBS})

  # --- sbpanel (okno + ikona przy zegarze; obie role) ---
  add_executable(sbpanel WIN32
    src/panel/panel.cpp
    src/panel/panel.rc
    src/common/pluginsetup.cpp
    src/common/kspsetup.cpp
    src/common/signer.cpp
    src/ksp/log.cpp
    ${COMMON_SRV_SRC}
  )
  target_include_directories(sbpanel PRIVATE src/common src/ksp src/panel)
  target_link_libraries(sbpanel PRIVATE ${WIN_LIBS} comctl32 user32 gdi32 msimg32)
  # DLL-e obu architektur wbudowane w sbpanel.exe: build.cmd kopiuje je do build/embed/ przed budowaniem panelu
  set(EMBED_DIR "${CMAKE_SOURCE_DIR}/build/embed")
  set(EMBED_FILES "${EMBED_DIR}/x64-cpsignbridge.dll" "${EMBED_DIR}/x86-cpsignbridge.dll"
                  "${EMBED_DIR}/x64-cpsignbridge-rdp.dll" "${EMBED_DIR}/x86-cpsignbridge-rdp.dll")
  set_source_files_properties(src/panel/panel.rc PROPERTIES
    COMPILE_OPTIONS "/I${EMBED_DIR}"
    OBJECT_DEPENDS "${EMBED_FILES}")
endif()

# --- testy jednostkowe (CI: x86 i x64; bez zewnętrznych bibliotek) ---
if(SB_BUILD_TESTS)
  enable_testing()
  add_executable(sbtests
    tests/main.cpp
    tests/test_protocol.cpp
    tests/test_kspsetup.cpp
    tests/test_ksp.cpp
    src/common/kspsetup.cpp
    src/ksp/backend_composite.cpp
    src/ksp/backend_remote.cpp
    src/ksp/log.cpp
    ${COMMON_SRV_SRC}
  )
  target_include_directories(sbtests PRIVATE src/common src/ksp tests)
  target_link_libraries(sbtests PRIVATE ${WIN_LIBS})
  add_test(NAME sbtests COMMAND sbtests)
endif()
