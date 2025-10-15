#pragma once 
#include "slic3r/GUI/Event/Event.hpp"

namespace Slic3r{
class ModelObject;
class ModelVolume;
class SlicingStatusEvent;
class SlicingProcessCompletedEvent;
namespace GUI {

#define EVT_PUBLISHING_START        1
#define EVT_PUBLISHING_STOP         2

//BBS: add EVT_SLICING_UPDATE declare here
wxDECLARE_EVENT(EVT_SCHEDULE_BACKGROUND_PROCESS,     SimpleEvent);
wxDECLARE_EVENT(EVT_SLICING_COMPLETED,               wxCommandEvent);
wxDECLARE_EVENT(EVT_PROCESS_COMPLETED,               SlicingProcessCompletedEvent);
wxDECLARE_EVENT(EVT_SLICING_UPDATE, Slic3r::SlicingStatusEvent);
wxDECLARE_EVENT(EVT_PUBLISH,        wxCommandEvent);
wxDECLARE_EVENT(EVT_OPEN_PLATESETTINGSDIALOG,        wxCommandEvent);
wxDECLARE_EVENT(EVT_REPAIR_MODEL,        wxCommandEvent);
wxDECLARE_EVENT(EVT_EXPORT_BEGAN,                    wxCommandEvent);
wxDECLARE_EVENT(EVT_EXPORT_FINISHED,                 wxCommandEvent);
wxDECLARE_EVENT(EVT_IMPORT_MODEL_ID,                 wxCommandEvent);
wxDECLARE_EVENT(EVT_DOWNLOAD_PROJECT,                wxCommandEvent);

wxDECLARE_EVENT(EVT_RESTORE_PROJECT,                 wxCommandEvent);
wxDECLARE_EVENT(EVT_PRINT_FINISHED,                  wxCommandEvent);
wxDECLARE_EVENT(EVT_SEND_CALIBRATION_FINISHED,       wxCommandEvent);
wxDECLARE_EVENT(EVT_SEND_FINISHED,                   wxCommandEvent);
wxDECLARE_EVENT(EVT_PUBLISH_FINISHED,                wxCommandEvent);

wxDECLARE_EVENT(EVT_FILAMENT_COLOR_CHANGED,        wxCommandEvent);
wxDECLARE_EVENT(EVT_INSTALL_PLUGIN_NETWORKING,        wxCommandEvent);
wxDECLARE_EVENT(EVT_INSTALL_PLUGIN_HINT,        wxCommandEvent);
wxDECLARE_EVENT(EVT_UPDATE_PLUGINS_WHEN_LAUNCH,        wxCommandEvent);
wxDECLARE_EVENT(EVT_PREVIEW_ONLY_MODE_HINT,        wxCommandEvent);
wxDECLARE_EVENT(EVT_GLCANVAS_COLOR_MODE_CHANGED,   SimpleEvent);
wxDECLARE_EVENT(EVT_PRINT_FROM_SDCARD_VIEW,   SimpleEvent);
wxDECLARE_EVENT(EVT_CREATE_FILAMENT, SimpleEvent);
wxDECLARE_EVENT(EVT_MODIFY_FILAMENT, SimpleEvent);
wxDECLARE_EVENT(EVT_ADD_FILAMENT, SimpleEvent);
wxDECLARE_EVENT(EVT_DEL_FILAMENT, SimpleEvent);
using ColorEvent = Event<wxColour>;
wxDECLARE_EVENT(EVT_ADD_CUSTOM_FILAMENT, ColorEvent);

#ifdef _WIN32
// USB HID attach / detach events from Windows OS.
using HIDDeviceAttachedEvent = Event<std::string>;
using HIDDeviceDetachedEvent = Event<std::string>;
wxDECLARE_EVENT(EVT_HID_DEVICE_ATTACHED, HIDDeviceAttachedEvent);
wxDECLARE_EVENT(EVT_HID_DEVICE_DETACHED, HIDDeviceDetachedEvent);

// Disk aka Volume attach / detach events from Windows OS.
using VolumeAttachedEvent = SimpleEvent;
using VolumeDetachedEvent = SimpleEvent;
wxDECLARE_EVENT(EVT_VOLUME_ATTACHED, VolumeAttachedEvent);
wxDECLARE_EVENT(EVT_VOLUME_DETACHED, VolumeDetachedEvent);
#endif /* _WIN32 */

struct ObjectVolumeID {
    ModelObject* object{ nullptr };
    ModelVolume* volume{ nullptr };
};

typedef Event<ObjectVolumeID> ObjectSettingEvent;

wxDECLARE_EVENT(EVT_OBJ_LIST_OBJECT_SELECT, SimpleEvent);
wxDECLARE_EVENT(EVT_PARTPLATE_LIST_PLATE_SELECT, IntEvent);

wxDECLARE_EVENT(EVT_DIFF_DIALOG_TRANSFER, SimpleEvent);

wxDECLARE_EVENT(EVT_HTTP_ERROR, wxCommandEvent);
wxDECLARE_EVENT(EVT_USER_LOGIN, wxCommandEvent);
wxDECLARE_EVENT(EVT_SELECT_TAB, wxCommandEvent);
wxDECLARE_EVENT(EVT_USER_LOGIN_HANDLE, wxCommandEvent);
wxDECLARE_EVENT(EVT_CHECK_PRIVACY_VER, wxCommandEvent);
wxDECLARE_EVENT(EVT_CHECK_PRIVACY_SHOW, wxCommandEvent);
wxDECLARE_EVENT(EVT_SHOW_IP_DIALOG, wxCommandEvent);
wxDECLARE_EVENT(EVT_SET_SELECTED_MACHINE, wxCommandEvent);
wxDECLARE_EVENT(EVT_UPDATE_MACHINE_LIST, wxCommandEvent);
wxDECLARE_EVENT(EVT_UPDATE_PRESET_CB, SimpleEvent);

wxDECLARE_EVENT(EVT_BACKUP_POST, wxCommandEvent);
wxDECLARE_EVENT(EVT_SYNC_CLOUD_PRESET,     SimpleEvent);
wxDECLARE_EVENT(EVT_LOAD_URL, wxCommandEvent);

wxDECLARE_EVENT(EVT_CONNECT_LAN_MODE_PRINT, wxCommandEvent);
wxDECLARE_EVENT(EVT_ENTER_FORCE_UPGRADE, wxCommandEvent);
wxDECLARE_EVENT(EVT_SHOW_NO_NEW_VERSION, wxCommandEvent);
wxDECLARE_EVENT(EVT_SHOW_DIALOG, wxCommandEvent);
}
}