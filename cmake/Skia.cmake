set(PHOTO_ASTRA_SKIA_ROOT "${PROJECT_SOURCE_DIR}/.deps/skia" CACHE PATH "Pinned Skia source and build root")
set(PHOTO_ASTRA_SKIA_REVISION "ed427fd003ba3bc6eb4a8ae0337f9cdafc39e5fb")
if(NOT EXISTS "${PHOTO_ASTRA_SKIA_ROOT}/include/core/SkCanvas.h")
    message(FATAL_ERROR "Skia missing. Run scripts/bootstrap-skia.ps1 or set PHOTO_ASTRA_SKIA_ROOT; see docs/skia.md.")
endif()
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${PHOTO_ASTRA_SKIA_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE skia_revision OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE skia_git_result)
if(NOT skia_git_result EQUAL 0 OR NOT skia_revision STREQUAL PHOTO_ASTRA_SKIA_REVISION)
    message(FATAL_ERROR "Skia must be checked out at ${PHOTO_ASTRA_SKIA_REVISION}.")
endif()
if(WIN32)
    set(skia_release "${PHOTO_ASTRA_SKIA_ROOT}/out/photoastra/skia.lib")
    set(skia_debug "${PHOTO_ASTRA_SKIA_ROOT}/out/photoastra-debug/skia.lib")
else()
    set(skia_release "${PHOTO_ASTRA_SKIA_ROOT}/out/photoastra/libskia.a")
    set(skia_debug "${PHOTO_ASTRA_SKIA_ROOT}/out/photoastra-debug/libskia.a")
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(skia_required "${skia_debug}")
else()
    set(skia_required "${skia_release}")
endif()
if(NOT EXISTS "${skia_required}")
    message(FATAL_ERROR "Missing ${skia_required}. Build the matching Skia configuration; see docs/skia.md.")
endif()
add_library(Skia::Skia STATIC IMPORTED GLOBAL)
set_target_properties(Skia::Skia PROPERTIES
    IMPORTED_LOCATION "${skia_release}"
    IMPORTED_LOCATION_DEBUG "${skia_debug}"
    IMPORTED_LOCATION_RELEASE "${skia_release}"
    MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
    MAP_IMPORTED_CONFIG_MINSIZEREL Release
    INTERFACE_INCLUDE_DIRECTORIES "${PHOTO_ASTRA_SKIA_ROOT}"
    INTERFACE_COMPILE_DEFINITIONS "SK_GANESH;SK_GL")
if(WIN32)
    set_property(TARGET Skia::Skia PROPERTY INTERFACE_LINK_LIBRARIES "opengl32;gdi32;user32;ole32;windowscodecs;uuid")
elseif(APPLE)
    set_property(TARGET Skia::Skia PROPERTY INTERFACE_LINK_LIBRARIES "-framework OpenGL;-framework CoreFoundation;-framework CoreGraphics")
else()
    find_package(OpenGL REQUIRED)
    find_package(Threads REQUIRED)
    set_property(TARGET Skia::Skia PROPERTY INTERFACE_LINK_LIBRARIES "OpenGL::GL;Threads::Threads;${CMAKE_DL_LIBS}")
endif()
