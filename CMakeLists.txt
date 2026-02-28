cmake_minimum_required(VERSION 3.21)

# ---- Versioning ----
set(VERSION_MAJOR 2)
set(VERSION_MINOR 5)
set(VERSION_PATCH 6)

# ---- Project ----
project(
    CraftingInventoryExtender
    VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
    LANGUAGES CXX
)

# ---- Include guards ----
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed.")
endif()

# ---- C++ Standard ----
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

# ---- Strip source paths from binaries (Release only) ----
# This removes full filesystem paths from __FILE__ macros in all targets including dependencies
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /d1trimfile:\"${CMAKE_SOURCE_DIR}/\"")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /d1trimfile:\"${CMAKE_SOURCE_DIR}/\"")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG:NONE")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG:NONE")
endif()

# ---- Dependencies ----
# spdlog and xbyak come from vcpkg
find_package(spdlog CONFIG REQUIRED)
find_package(minhook CONFIG REQUIRED)

# CommonLibSSE-NG via FetchContent
include(FetchContent)
message(STATUS "Fetching CommonLibSSE-NG...")
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL "" FORCE)
set(ENABLE_XBYAK ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    CommonLibSSE
    GIT_REPOSITORY https://github.com/CharmedBaryon/CommonLibSSE-NG.git
    GIT_TAG v3.7.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(CommonLibSSE)

# ---- Source files ----
set(SOURCES
    src/main.cpp
    src/Hooks/InventoryHooks.cpp
    src/Hooks/CraftingSession.cpp
    src/Services/ContainerManager.cpp
    src/Services/ContainerRegistry.cpp
    src/Services/INISettings.cpp
    src/Services/SourceScanner.cpp
    src/Services/SLIDIntegration.cpp
    src/Services/APIService.cpp
    src/Services/TranslationService.cpp
    src/API/APIMessaging.cpp
    src/Papyrus/PapyrusInterface.cpp
)

set(HEADERS
    include/PCH.h
    include/Version.h
    include/Hooks/InventoryHooks.h
    include/Hooks/CraftingSession.h
    include/Services/ContainerManager.h
    include/Services/ContainerRegistry.h
    include/Services/INISettings.h
    include/Services/SourceScanner.h
    include/Services/SLIDIntegration.h
    include/Services/APIService.h
    include/Services/TranslationService.h
    include/API/APIMessaging.h
    include/Papyrus/PapyrusInterface.h
)

# ---- Create DLL ----
add_library(
    ${PROJECT_NAME}
    SHARED
    ${SOURCES}
    ${HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/version.rc
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
    CommonLibSSE::CommonLibSSE
    minhook::minhook
)

target_precompile_headers(
    ${PROJECT_NAME}
    PRIVATE
    include/PCH.h
)

# ---- Compiler options ----
if(MSVC)
    target_compile_options(
        ${PROJECT_NAME}
        PRIVATE
        /W4
        /WX-
        /permissive-
        /Zc:__cplusplus
        /Zc:preprocessor
        /external:anglebrackets
        /external:W0
        /MP
    )
endif()

# ---- Post build ----
if(DEFINED ENV{SKYRIM_MODS_PATH})
    set(SKYRIM_MODS_PATH $ENV{SKYRIM_MODS_PATH})
endif()

if(DEFINED SKYRIM_MODS_PATH)
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SKYRIM_MODS_PATH}/${PROJECT_NAME}/SKSE/Plugins"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${PROJECT_NAME}> "${SKYRIM_MODS_PATH}/${PROJECT_NAME}/SKSE/Plugins/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_PDB_FILE:${PROJECT_NAME}> "${SKYRIM_MODS_PATH}/${PROJECT_NAME}/SKSE/Plugins/"
        VERBATIM
    )
endif()
