cmake_minimum_required(VERSION 3.13)
project(LightMaker-native)

if (WIN32)
    add_executable(LightMakerSlicer WIN32 
                    LightMaker.cpp 
                    LightMaker.hpp 
                    LightMaker_app_msvc.cpp 
                    LightSlicerApp.cpp 
                    LightSlicerApp.hpp 
                    # ${CMAKE_CURRENT_BINARY_DIR}/LightMaker.rc
                    )

    if (MINGW)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -municode")
    endif()
    target_compile_definitions(LightMakerSlicer PRIVATE -DSLIC3R_WRAPPER_NOCONSOLE)
else ()
    add_executable(LightMakerSlicer LightMaker.cpp LightMaker.hpp)
endif ()

if (MINGW)
    target_link_options(LightMakerSlicer PUBLIC "-Wl,-allow-multiple-definition")
    set_target_properties(LightMakerSlicer PROPERTIES PREFIX "")
endif (MINGW)

set_target_properties(LightMakerSlicer PROPERTIES OUTPUT_NAME "LightMakerSlicer")

target_link_libraries(LightMakerSlicer libslic3r cereal::cereal boost_libs)

if (APPLE)
#    add_compile_options(-stdlib=libc++)
#    add_definitions(-DBOOST_THREAD_DONT_USE_CHRONO -DBOOST_NO_CXX11_RVALUE_REFERENCES -DBOOST_THREAD_USES_MOVE)
    # -liconv: boost links to libiconv by default
    target_link_libraries(LightMakerSlicer "-liconv -framework IOKit" "-framework CoreFoundation" "-framework AVFoundation" "-framework AVKit" "-framework CoreMedia" "-framework VideoToolbox" -lc++)
elseif (MSVC)
    # Manifest is provided through LightMaker.rc, don't generate your own.
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
else ()
    # Boost on Raspberry-Pi does not link to pthreads explicitely.
    target_link_libraries(LightMakerSlicer ${CMAKE_DL_LIBS} -lstdc++ Threads::Threads pangoft2-1.0)
endif ()

# Add the Slic3r GUI library, libcurl, OpenGL and GLU libraries.
if (SLIC3R_GUI)
    # #    target_link_libraries(LightMaker ws2_32 uxtheme setupapi libslic3r_gui ${wxWidgets_LIBRARIES})
    target_link_libraries(LightMakerSlicer libslic3r_gui)
    if (MSVC)
        # Generate debug symbols even in release mode.
        target_link_options(LightMakerSlicer PUBLIC "$<$<CONFIG:RELEASE>:/DEBUG>")
        target_link_libraries(LightMakerSlicer user32.lib Setupapi.lib)
    elseif (MINGW)
        target_link_libraries(LightMakerSlicer ws2_32 uxtheme setupapi)
    elseif (APPLE)
        target_link_libraries(LightMakerSlicer "-framework OpenGL")
    else ()
        target_link_libraries(LightMakerSlicer -ldl)
    endif ()
endif ()

# Link the resources dir to where Slic3r GUI expects it
set(output_dlls_Release "")
set(output_dlls_Debug "")
set(output_dlls_RelWithDebInfo "")
if (WIN32)
    # This has to be a separate target due to the windows command line lenght limits
    add_custom_target(COPY_DLLS ALL DEPENDS LightMakerSlicer)

    if (CMAKE_CONFIGURATION_TYPES)
        foreach (CONF ${CMAKE_CONFIGURATION_TYPES})
            file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/${CONF}" WIN_CONF_OUTPUT_DIR)
            file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/${CONF}/resources" WIN_RESOURCES_SYMLINK)
            add_custom_command(TARGET LightMakerSlicer POST_BUILD
                COMMAND if exist "${WIN_CONF_OUTPUT_DIR}" "("
                        if not exist "${WIN_RESOURCES_SYMLINK}" "("
                            mklink /J "${WIN_RESOURCES_SYMLINK}" "${SLIC3R_RESOURCES_DIR_WIN}"
                        ")"
                    ")"
                COMMENT "Symlinking the resources directory into the build tree"
                VERBATIM
            )
        endforeach ()

        # not copy dlls
        if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
            win_copy_dlls(COPY_DLLS "Debug" "d" output_dlls_Debug)
        elseif("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
            win_copy_dlls(COPY_DLLS "RelWithDebInfo" "" output_dlls_Release)
        else()
            win_copy_dlls(COPY_DLLS "Release" "" output_dlls_Release)
        endif()
    else ()
        file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/resources" WIN_RESOURCES_SYMLINK)
        add_custom_command(TARGET LightMakerSlicer POST_BUILD
            COMMAND if not exist "${WIN_RESOURCES_SYMLINK}" "(" mklink /J "${WIN_RESOURCES_SYMLINK}" "${SLIC3R_RESOURCES_DIR_WIN}" ")"
            COMMENT "Symlinking the resources directory into the build tree"
            VERBATIM
        )
    endif ()


else ()
    if (APPLE AND NOT CMAKE_MACOSX_BUNDLE)
        # On OSX, the name of the binary matches the name of the Application.
        add_custom_command(TARGET LightMakerSlicer POST_BUILD
            COMMAND ln -sf LightMakerSlicer LightMakerSlicer
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:LightMakerSlicer>"
            VERBATIM)
    else ()
        add_custom_command(TARGET LightMakerSlicer POST_BUILD
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:LightMakerSlicer>"
            VERBATIM)
    endif ()
    if (XCODE)
        # Because of Debug/Release/etc. configurations (similar to MSVC) the slic3r binary is located in an extra level
        set(BIN_RESOURCES_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources")
        set(BIN_CONF_DIR "Debug")
    else ()
        set(BIN_RESOURCES_DIR "${CMAKE_CURRENT_BINARY_DIR}/../resources")
    endif ()
    if (CMAKE_MACOSX_BUNDLE)
        if (CMAKE_CONFIGURATION_TYPES)
            set(BIN_RESOURCES_DIR "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/LightMakerSlicer.app/Contents/Resources")
        else()
            set(BIN_RESOURCES_DIR "${CMAKE_CURRENT_BINARY_DIR}/LightMakerSlicer.app/Contents/Resources")
        endif()
        set(MACOSX_BUNDLE_ICON_FILE Icon.icns)
        set(MACOSX_BUNDLE_BUNDLE_NAME "LightMakerSlicer")
        set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${LightMaker_VERSION})
        set(MACOSX_BUNDLE_COPYRIGHT "Copyright(C) 2025-2028 hms All Rights Reserved")
    endif()
    add_custom_command(TARGET LightMakerSlicer POST_BUILD
        COMMAND ln -sfn "${SLIC3R_RESOURCES_DIR}" "${BIN_RESOURCES_DIR}"
        COMMENT "Symlinking the resources directory into the build tree"
        VERBATIM)
endif ()

set_target_properties(LightMakerSlicer PROPERTIES
                    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
                    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
                    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
                    )

# Slic3r binary install target. Default build type is release in case no CMAKE_BUILD_TYPE is provided.
if( ("${CMAKE_BUILD_TYPE}" STREQUAL "Release") OR ("${CMAKE_BUILD_TYPE}" STREQUAL "") )
    set (build_type "Release")
    set(CMAKE_BUILD_POSTFIX "")
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    set (build_type "RelWithDebInfo")
    set(CMAKE_BUILD_POSTFIX "")
else()
    set (build_type "Debug")
    set(CMAKE_BUILD_POSTFIX "d")
endif()
message(STATUS "libslic3r-CMAKE_BUILD_TYPE: ${build_type}")
message(STATUS "CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}")
if (WIN32)
    install(TARGETS LightMakerSlicer RUNTIME DESTINATION ".")
    install(FILES ${output_dlls_${build_type}} DESTINATION ".")
else ()
    install(TARGETS LightMakerSlicer RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()
