add_library(photo_astra_core STATIC Document.cpp RasterImage.cpp Viewport.cpp History.cpp Effect.cpp LayerMoveGesture.cpp)
add_library(PhotoAstra::Core ALIAS photo_astra_core)
target_include_directories(photo_astra_core PUBLIC include)
photo_astra_target_defaults(photo_astra_core)
