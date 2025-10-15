cmake_minimum_required(VERSION 3.13)
project(libslic3r_gui)

include(PrecompiledHeader)

set(SLIC3R_GUI_SOURCES
    pchheader.cpp
    pchheader.hpp

    Global/AppModule.hpp
    Global/AppModule.cpp
    Global/AppI18N.hpp
    Global/AppI18N.cpp
    Global/ProjectFlow.hpp
    Global/ProjectFlow.cpp
    Global/InstanceCheck.cpp
    Global/InstanceCheck.hpp

    GUI/BaseException.cpp
    GUI/BaseException.h
    GUI/StackWalker.cpp
    GUI/StackWalker.h

    GUI/Widgets/AxisCtrlButton.cpp
    GUI/Widgets/AxisCtrlButton.hpp
    GUI/Widgets/Button.cpp
    GUI/Widgets/Button.hpp
    GUI/Widgets/CheckBox.cpp
    GUI/Widgets/CheckBox.hpp
    GUI/Widgets/RadioBox.hpp
    GUI/Widgets/RadioBox.cpp
    GUI/Widgets/ProgressDialog.hpp
    GUI/Widgets/ProgressDialog.cpp
    GUI/Widgets/RoundedRectangle.hpp
    GUI/Widgets/RoundedRectangle.cpp
    GUI/Widgets/ComboBox.cpp
    GUI/Widgets/ComboBox.hpp
    GUI/Widgets/SideButton.hpp
    GUI/Widgets/SideButton.cpp
    GUI/Widgets/SideMenuPopup.hpp
    GUI/Widgets/SideMenuPopup.cpp
    GUI/Widgets/DropDown.cpp
    GUI/Widgets/DropDown.hpp
    GUI/Widgets/PopupWindow.cpp
    GUI/Widgets/PopupWindow.hpp
    GUI/Widgets/Label.cpp
    GUI/Widgets/Label.hpp
    GUI/Widgets/Scrollbar.cpp
    GUI/Widgets/Scrollbar.hpp
    GUI/Widgets/ScrolledWindow.cpp
    GUI/Widgets/ScrolledWindow.hpp
    GUI/Widgets/StaticBox.cpp
    GUI/Widgets/StaticBox.hpp
    GUI/Widgets/ImageSwitchButton.cpp
    GUI/Widgets/ImageSwitchButton.hpp
    GUI/Widgets/SwitchButton.cpp
    GUI/Widgets/SwitchButton.hpp
    GUI/Widgets/SpinInput.cpp
    GUI/Widgets/SpinInput.hpp
    GUI/Widgets/StaticLine.cpp
    GUI/Widgets/StaticLine.hpp
    GUI/Widgets/StateColor.cpp
    GUI/Widgets/StateColor.hpp
    GUI/Widgets/StateHandler.cpp
    GUI/Widgets/StateHandler.hpp
    GUI/Widgets/TabCtrl.cpp
    GUI/Widgets/TabCtrl.hpp
    GUI/Widgets/TextInput.cpp
    GUI/Widgets/TextInput.hpp
    GUI/Widgets/TempInput.cpp
    GUI/Widgets/TempInput.hpp
    GUI/Widgets/Scrollbar.cpp
    GUI/Widgets/Scrollbar.hpp
    GUI/Widgets/ScrolledWindow.cpp
    GUI/Widgets/ScrolledWindow.hpp
    GUI/Widgets/StepCtrl.cpp
    GUI/Widgets/StepCtrl.hpp
    GUI/Widgets/ProgressBar.cpp
    GUI/Widgets/ProgressBar.hpp
    GUI/Widgets/WebView.cpp
    GUI/Widgets/WebView.hpp
    GUI/Widgets/ErrorMsgStaticText.cpp
    GUI/Widgets/ErrorMsgStaticText.hpp
    GUI/Widgets/Tabbook.cpp
    GUI/Widgets/Tabbook.hpp
    GUI/Widgets/TabButton.cpp
    GUI/Widgets/TabButton.hpp

    GUI/Gizmos/GLGizmosManager.cpp
    GUI/Gizmos/GLGizmosManager.hpp
    GUI/Gizmos/GLGizmosCommon.cpp
    GUI/Gizmos/GLGizmosCommon.hpp
    GUI/Gizmos/GLGizmoBase.cpp
    GUI/Gizmos/GLGizmoBase.hpp
    GUI/Gizmos/GLGizmoMove.cpp
    GUI/Gizmos/GLGizmoMove.hpp
    GUI/Gizmos/GLGizmoRotate.cpp
    GUI/Gizmos/GLGizmoRotate.hpp
    GUI/Gizmos/GLGizmoScale.cpp
    GUI/Gizmos/GLGizmoScale.hpp
    GUI/Gizmos/GLGizmoFlatten.cpp
    GUI/Gizmos/GLGizmoFlatten.hpp
    GUI/Gizmos/GLGizmoSimplify.cpp
    GUI/Gizmos/GLGizmoSimplify.hpp
    GUI/Gizmos/GizmoObjectManipulation.cpp
    GUI/Gizmos/GizmoObjectManipulation.hpp

    GUI/Config/ConfigComboBox.hpp
    GUI/Config/ConfigComboBox.cpp
    GUI/Config/PlaterPrinterPresetComboBox.hpp
    GUI/Config/PlaterPrinterPresetComboBox.cpp
    GUI/Config/PrinterPresetDialog.hpp
    GUI/Config/PrinterPresetDialog.cpp
    GUI/Config/PlateSettingsDialog.cpp
    GUI/Config/PlateSettingsDialog.hpp
    GUI/Config/DragCanvas.cpp
    GUI/Config/DragCanvas.hpp
    GUI/Config/GUI_ObjectList.cpp
    GUI/Config/GUI_ObjectList.hpp
    GUI/Config/GUI_ObjectLayers.cpp
    GUI/Config/GUI_ObjectLayers.hpp
    GUI/Config/GUI_ObjectSettings.cpp
    GUI/Config/GUI_ObjectSettings.hpp
    GUI/Config/GUI_ObjectTable.cpp
    GUI/Config/GUI_ObjectTable.hpp
    GUI/Config/GUI_ObjectTableSettings.cpp
    GUI/Config/GUI_ObjectTableSettings.hpp

    GUI/format.hpp
    GUI/GUI.cpp
    GUI/GUI.hpp
    GUI/AppAdapter.cpp
    GUI/AppAdapter.hpp
    GUI/GUI_App.cpp
    GUI/AppAdapter.hpp
    GUI/GUI_Utils.cpp
    GUI/GUI_Utils.hpp
    GUI/I18N.cpp
    GUI/I18N.hpp
    GUI/MainFrame.cpp
    GUI/MainFrame.hpp
    GUI/MainPanel.cpp
    GUI/MainPanel.hpp
    GUI/BitmapComboBox.hpp
    GUI/BitmapComboBox.cpp
    GUI/GUI_Factories.cpp
    GUI/GUI_Factories.hpp
    GUI/Tab.cpp
    GUI/Tab.hpp
    GUI/MarkdownTip.cpp
    GUI/MarkdownTip.hpp
    GUI/wxExtensions.cpp
    GUI/wxExtensions.hpp

    Utils/AppWx.hpp
    Utils/AppWx.cpp
    Utils/Str.hpp
    Utils/Str.cpp
    Utils/AssociationFiles.cpp
    Utils/AssociationFiles.hpp
    Utils/FixModelByWin10.cpp
    Utils/FixModelByWin10.hpp
    Utils/Process.cpp
    Utils/Process.hpp
    Utils/Profile.hpp
    Utils/UndoRedo.cpp
    Utils/UndoRedo.hpp
    Utils/ProfileDescription.hpp
    Utils/RemovableDriveManager.cpp
    Utils/RemovableDriveManager.hpp

    GUI/Jobs/Job.hpp
    GUI/Jobs/Worker.hpp
    GUI/Jobs/BoostThreadWorker.hpp
    GUI/Jobs/BoostThreadWorker.cpp
    GUI/Jobs/BusyCursorJob.hpp
    GUI/Jobs/PlaterWorker.hpp
    GUI/Jobs/ArrangeJob.hpp
    GUI/Jobs/ArrangeJob.cpp
    GUI/Jobs/OrientJob.hpp
    GUI/Jobs/OrientJob.cpp
    GUI/Jobs/FillBedJob.hpp
    GUI/Jobs/FillBedJob.cpp
    GUI/Jobs/ProgressIndicator.hpp
    GUI/Jobs/NotificationProgressIndicator.hpp
    GUI/Jobs/NotificationProgressIndicator.cpp
    GUI/Jobs/ThreadSafeQueue.hpp

    Config/AppPreset.hpp
    Config/AppPreset.cpp
    Config/AppConfig.hpp
    Config/AppConfig.cpp
    Config/ConfigUtils.hpp
    Config/ConfigUtils.cpp
    Config/Snapshot.cpp
    Config/Snapshot.hpp
    Config/Version.cpp
    Config/Version.hpp
    Config/OptionsGroup.cpp
    Config/OptionsGroup.hpp
    Config/PresetComboBoxes.hpp
    Config/PresetComboBoxes.cpp
    Config/ConfigManipulation.cpp
    Config/ConfigManipulation.hpp
    Config/PresetHints.cpp
    Config/PresetHints.hpp
    Config/ParamsPanel.cpp
    Config/ParamsPanel.hpp
    Config/Field.cpp
    Config/Field.hpp
    Config/Search.cpp
    Config/Search.hpp
    Config/OG_CustomCtrl.cpp
    Config/OG_CustomCtrl.hpp
    Config/VersionInfo.hpp
    Config/VersionInfo.cpp
    Config/PresetUpdater.cpp
    Config/PresetUpdater.hpp

    GCode/GCodeDefine.hpp
    GCode/GCodeDefine.cpp
    GCode/GCodeViewerData.hpp
    GCode/GCodeViewerData.cpp
    GCode/GCodePanel.hpp
    GCode/GCodePanel.cpp
    GCode/GCodeRenderer.hpp
    GCode/GCodeRenderer.cpp
    GCode/GCodeExtensions.hpp
    GCode/GCodeExtensions.cpp
    GCode/GCodeViewerState.hpp
    GCode/GCodeViewerState.cpp
    GCode/GCodeViewInstance.hpp
    GCode/GCodeViewInstance.cpp
    GCode/GCodePlayer.hpp
    GCode/GCodePlayer.cpp

    
    Render/AppRender.hpp
    Render/AppRender.cpp
    Render/3DBed.cpp
    Render/3DBed.hpp
    Render/3DBedTexture.cpp
    Render/3DBedTexture.hpp
    Render/3DScene.cpp
    Render/3DScene.hpp
    Render/GLShadersManager.hpp
    Render/GLShadersManager.cpp
    Render/GLShader.cpp
    Render/GLShader.hpp
    Render/GLCanvas3D.hpp
    Render/GLCanvas3D.cpp
    Render/GLCanvas3DFacade.hpp
    Render/GLCanvas3DFacade.cpp
    Render/OpenGLManager.hpp
    Render/OpenGLManager.cpp
    Render/GLModel.hpp
    Render/GLModel.cpp
    Render/GLTexture.hpp
    Render/GLTexture.cpp
    Render/GLToolbar.hpp
    Render/GLToolbar.cpp
    Render/IMToolbar.hpp
    Render/IMToolbar.cpp
    Render/GCodeViewer.hpp
    Render/GCodeViewer.cpp
    Render/GLColors.hpp
    Render/GLColors.cpp
    Render/Mouse3DController.cpp
    Render/Mouse3DController.hpp
    Render/GLSelectionRectangle.cpp
    Render/GLSelectionRectangle.hpp
    Render/GUI_Geometry.cpp
    Render/GUI_Geometry.hpp
    Render/RaycastManager.cpp
    Render/RaycastManager.hpp
    Render/SceneRaycaster.hpp
    Render/SceneRaycaster.cpp
    Render/Selection.hpp
    Render/Selection.cpp
    Render/MeshUtils.cpp
    Render/MeshUtils.hpp
    Render/PlateBed.cpp
    Render/PlateBed.hpp
    Render/RenderUtils.hpp
    Render/RenderUtils.cpp
    Render/LayersEditing.hpp
    Render/LayersEditing.cpp
    Render/RenderHelpers.hpp
    Render/RenderHelpers.cpp
    Render/View3DCanvas.hpp
    Render/View3DCanvas.cpp
    Render/GCodePreviewCanvas.hpp
    Render/GCodePreviewCanvas.cpp
    Render/SelectionManipulator.hpp
    Render/SelectionManipulator.cpp

    Render/ImGUI/ImGuiWrapper.hpp
    Render/ImGUI/ImGuiWrapper.cpp
    Render/ImGUI/IMSlider.cpp
    Render/ImGUI/IMSlider.hpp

    
    Render/EventHandler/GUIEventHandler.hpp
    Render/EventHandler/GUIEventHandler.cpp
    Render/EventHandler/TrackballManipulator.hpp
    Render/EventHandler/TrackballManipulator.cpp
    Render/EventHandler/UICustomEventHandler.hpp
    Render/EventHandler/UICustomEventHandler.cpp
    Render/EventHandler/DragEventHandler.hpp
    Render/EventHandler/DragEventHandler.cpp
    Render/EventHandler/PreviewEventHandler.hpp
    Render/EventHandler/PreviewEventHandler.cpp

    Theme/AppColor.hpp
    Theme/AppColor.cpp
    Theme/AppFont.hpp
    Theme/AppFont.cpp
    Theme/ColorSpaceConvert.hpp
    Theme/ColorSpaceConvert.cpp
    Theme/FontConfigHelp.hpp
    Theme/FontConfigHelp.cpp
    Theme/Font.hpp
    Theme/Font.cpp
    Theme/WxFontUtils.hpp
    Theme/WxFontUtils.cpp
    Theme/MacDarkMode.hpp
    Theme/BitmapCache.cpp
    Theme/BitmapCache.hpp
    Theme/IconManager.cpp
    Theme/IconManager.hpp

    GUI/Event/Event.hpp
    GUI/Event/UserPlaterEvent.hpp
    GUI/Event/UserPlaterEvent.cpp
    GUI/Event/UserGLToolBarEvent.hpp
    GUI/Event/UserGLToolBarEvent.cpp
    GUI/Event/UserDriveDataEvent.hpp
    GUI/Event/UserDriveDataEvent.cpp
    GUI/Event/UserCanvasEvent.hpp
    GUI/Event/UserCanvasEvent.cpp
    GUI/Event/UserNotifyEvent.hpp
    GUI/Event/UserNotifyEvent.cpp
    GUI/Event/UserNetEvent.hpp
    GUI/Event/UserNetEvent.cpp    

    GUI/Dialog/AboutDialog.cpp
    GUI/Dialog/AboutDialog.hpp
    GUI/Dialog/DialogCommand.hpp
    GUI/Dialog/DialogCommand.cpp
    GUI/Dialog/Preferences.cpp
    GUI/Dialog/Preferences.hpp
    GUI/Dialog/DesktopIntegrationDialog.cpp
    GUI/Dialog/DesktopIntegrationDialog.hpp
    GUI/Dialog/MsgDialog.cpp
    GUI/Dialog/MsgDialog.hpp
    GUI/Dialog/KBShortcutsDialog.hpp
    GUI/Dialog/KBShortcutsDialog.cpp
    GUI/Dialog/EditGCodeDialog.hpp
    GUI/Dialog/EditGCodeDialog.cpp
    GUI/Dialog/ObjColorDialog.cpp
    GUI/Dialog/ObjColorDialog.hpp
    GUI/Dialog/UnsavedChangesDialog.cpp
    GUI/Dialog/UnsavedChangesDialog.hpp
    GUI/Dialog/SavePresetDialog.hpp
    GUI/Dialog/SavePresetDialog.cpp
    GUI/Dialog/RecenterDialog.cpp
    GUI/Dialog/RecenterDialog.hpp
    GUI/Dialog/ParamsDialog.cpp
    GUI/Dialog/ParamsDialog.hpp
    GUI/Dialog/SingleChoiceDialog.hpp
    GUI/Dialog/SingleChoiceDialog.cpp
    GUI/Dialog/CreatePresetsDialog.hpp
    GUI/Dialog/CreatePresetsDialog.cpp
    GUI/Dialog/FileArchiveDialog.cpp
    GUI/Dialog/FileArchiveDialog.hpp
    GUI/Dialog/ReleaseNote.hpp
    GUI/Dialog/ReleaseNote.cpp
    GUI/Dialog/ConfigWizard.cpp
    GUI/Dialog/ConfigWizard.hpp
    GUI/Dialog/ConfigWizard_private.hpp

    GUI/Calibration/calib_dlg.hpp
    GUI/Calibration/calib_dlg.cpp

    GUI/Project/ProjectDropDialog.hpp
    GUI/Project/ProjectDropDialog.cpp
    GUI/Project/ProjectDirtyStateManager.hpp
    GUI/Project/ProjectDirtyStateManager.cpp
    GUI/Project/SurfaceDrag.cpp
    GUI/Project/SurfaceDrag.hpp

    GUI/Slice/WipingDialog.hpp
    GUI/Slice/WipingDialog.cpp
    GUI/Slice/RammingDialog.hpp
    GUI/Slice/RammingDialog.cpp
    GUI/Slice/RammingChart.cpp
    GUI/Slice/RammingChart.hpp
    GUI/Slice/2DBed.cpp
    GUI/Slice/2DBed.hpp
    GUI/Slice/BedShapeDialog.hpp
    GUI/Slice/BedShapeDialog.cpp

    GUI/Net/NetworkTestDialog.cpp
    GUI/Net/NetworkTestDialog.hpp

    GUI/Frame/Notebook.cpp
    GUI/Frame/Notebook.hpp
    GUI/Frame/Plater.cpp
    GUI/Frame/Plater.hpp
    GUI/Frame/OpenGLPanel.hpp
    GUI/Frame/OpenGLPanel.cpp
    GUI/Frame/OpenGLWindow.hpp
    GUI/Frame/OpenGLWindow.cpp
    GUI/Frame/PlaterPrivate.hpp
    GUI/Frame/PlaterPrivate.cpp
    GUI/Frame/GUI_Init.cpp
    GUI/Frame/GUI_Init.hpp
    GUI/Frame/BBLTopbar.cpp
    GUI/Frame/BBLTopbar.hpp
    GUI/Frame/UserNotification.cpp
    GUI/Frame/UserNotification.hpp
    GUI/Frame/DailyTips.cpp
    GUI/Frame/DailyTips.hpp
    GUI/Frame/SplashScreen.hpp
    GUI/Frame/SplashScreen.cpp
    GUI/Frame/GCodePreview.hpp
    GUI/Frame/GCodePreview.cpp
    GUI/Frame/View3D.hpp
    GUI/Frame/View3D.cpp

    GUI/Web/WebGuideDialog.hpp
    GUI/Web/WebGuideDialog.cpp

    Slice/CalibrationDef.hpp
    Slice/BackgroundSlicingProcess.cpp
    Slice/BackgroundSlicingProcess.hpp
    Slice/SlicingProcessCompletedEvent.hpp
    Slice/SlicingProcessCompletedEvent.cpp
    Slice/TickCode.cpp
    Slice/TickCode.hpp
    Slice/GCodeImportExporter.cpp
    Slice/GCodeImportExporter.hpp
    Slice/GCodeResultWrapper.cpp
    Slice/GCodeResultWrapper.hpp

    Scene/Boolean.hpp
    Scene/Boolean.cpp
    Scene/NotificationManager.cpp
    Scene/NotificationManager.hpp
    Scene/HintNotification.hpp
    Scene/HintNotification.cpp
    Scene/SlicingProgressNotification.cpp
    Scene/SlicingProgressNotification.hpp
    Scene/ObjectDataViewModel.cpp
    Scene/ObjectDataViewModel.hpp
    Scene/ExtraRenderers.hpp
    Scene/ExtraRenderers.cpp
    Scene/Camera.cpp
    Scene/Camera.hpp
    Scene/CameraUtils.cpp
    Scene/CameraUtils.hpp
    Scene/PartPlate.cpp
    Scene/PartPlate.hpp
    Scene/PartPlateList.cpp
    Scene/PartPlateList.hpp

    Net/Http.hpp
    Net/Http.cpp
    Net/DownloaderFileGet.hpp
    Net/DownloaderFileGet.cpp
    Net/Downloader.hpp
    Net/Downloader.cpp
)

if (WIN32)
    list(APPEND SLIC3R_GUI_SOURCES
            Theme/dark_mode.hpp
            Theme/IatHook.hpp
            Theme/UAHMenuBar.hpp
            Theme/WinDarkMode.hpp
            Theme/WinDarkMode.cpp
        )
endif ()

if (APPLE)
    list(APPEND SLIC3R_GUI_SOURCES
            Utils/RetinaHelperImpl.mm
            Theme/MacDarkMode.mm
            Utils/RemovableDriveManagerMM.mm
            Utils/RemovableDriveManagerMM.h
            GUI/Mouse3DHandlerMac.mm
            Global/InstanceCheckMac.mm
            Global/InstanceCheckMac.h
        )
    FIND_LIBRARY(DISKARBITRATION_LIBRARY DiskArbitration)
endif ()

add_library(libslic3r_gui STATIC ${SLIC3R_GUI_SOURCES})

target_include_directories(libslic3r_gui PRIVATE Utils)
target_include_directories(libslic3r_gui PRIVATE ../fullcontrol/include)

if (WIN32)
    target_include_directories(libslic3r_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/WebView2/include)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SLIC3R_GUI_SOURCES})

encoding_check(libslic3r_gui)

target_link_libraries(libslic3r_gui libslic3r boost_libs cereal::cereal imgui imguizmo minilzo GLEW::GLEW OpenGL::GL hidapi ${wxWidgets_LIBRARIES} glfw libcurl OpenSSL::SSL OpenSSL::Crypto noise::noise fullcontrol_cpp)

if (MSVC)
    target_link_libraries(libslic3r_gui Setupapi.lib)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    FIND_LIBRARY(WAYLAND_SERVER_LIBRARIES NAMES wayland-server)
    FIND_LIBRARY(WAYLAND_EGL_LIBRARIES    NAMES wayland-egl)
    FIND_LIBRARY(WAYLAND_CLIENT_LIBRARIES NAMES wayland-client)
    find_package(CURL REQUIRED)
    target_link_libraries(libslic3r_gui ${DBUS_LIBRARIES} OSMesa)
    target_link_libraries(libslic3r_gui
        OpenGL::EGL
        ${WAYLAND_SERVER_LIBRARIES}
        ${WAYLAND_EGL_LIBRARIES}
        ${WAYLAND_CLIENT_LIBRARIES}
        ${CURL_LIBRARIES}
    )
elseif (APPLE)
    target_link_libraries(libslic3r_gui ${DISKARBITRATION_LIBRARY})
endif()

if (SLIC3R_STATIC)
    # FIXME: This was previously exported by wx-config but the wxWidgets
    # cmake build forgets this and the build fails in debug mode (or on raspberry release)
    target_compile_definitions(libslic3r_gui PUBLIC -DwxDEBUG_LEVEL=0)
endif()

if (HAVE_SPNAV)
    target_link_libraries(libslic3r_gui spnav)
endif()

if (SLIC3R_STATIC AND NOT SLIC3R_STATIC_EXCLUDE_CURL AND UNIX AND NOT APPLE)
    target_compile_definitions(libslic3r_gui PRIVATE OPENSSL_CERT_OVERRIDE)
endif ()

if (SLIC3R_PCH)
    add_precompiled_header(libslic3r_gui pchheader.hpp FORCEINCLUDE)
endif ()

# We need to implement some hacks for wxWidgets and touch the underlying GTK
# layer and sub-libraries. This forces us to use the include locations and
# link these libraries.
if (UNIX AND NOT APPLE)
    find_package(GTK${SLIC3R_GTK} REQUIRED)
    if (FLATPAK)
        # I don't know why this is needed, but for whatever reason slic3r isn't
        # linking to X11 and webkit2gtk. force it.
        find_package(X11 REQUIRED)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(webkit2gtk REQUIRED webkit2gtk-4.1)
        target_link_libraries (libslic3r_gui ${X11_LIBRARIES} ${webkit2gtk_LIBRARIES})
    endif()
    target_include_directories(libslic3r_gui PRIVATE ${GTK${SLIC3R_GTK}_INCLUDE_DIRS})
    target_link_libraries(libslic3r_gui ${GTK${SLIC3R_GTK}_LIBRARIES} fontconfig)


    # We add GStreamer for bambu:/// support.
    pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
    pkg_check_modules(GST_BASE REQUIRED gstreamer-base-1.0)
    target_link_libraries(libslic3r_gui ${GSTREAMER_LIBRARIES} ${GST_BASE_LIBRARIES})
    target_include_directories(libslic3r_gui PRIVATE ${GSTREAMER_INCLUDE_DIRS} ${GST_BASE_INCLUDE_DIRS})
endif ()

# Add a definition so that we can tell we are compiling slic3r.
target_compile_definitions(libslic3r_gui PRIVATE SLIC3R_CURRENTLY_COMPILING_GUI_MODULE)
