qt_add_executable(photo_astra main.cpp)
target_link_libraries(photo_astra PRIVATE PhotoAstra::Ui)
photo_astra_target_defaults(photo_astra)
set_target_properties(photo_astra PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER org.photoastra.editor
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

include(GNUInstallDirs)
install(TARGETS photo_astra
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
qt_generate_deploy_app_script(TARGET photo_astra OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
install(FILES ${PROJECT_SOURCE_DIR}/third_party/licenses/Skia-LICENSE.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/photo-astra/licenses)
