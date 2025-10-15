#pragma once
#include "slic3r/GUI/Event/Event.hpp"

namespace Slic3r {
namespace GUI {

using EjectDriveNotificationClickedEvent = SimpleEvent;
wxDECLARE_EVENT(EVT_EJECT_DRIVE_NOTIFICAION_CLICKED, EjectDriveNotificationClickedEvent);
using ExportGcodeNotificationClickedEvent = SimpleEvent;
wxDECLARE_EVENT(EVT_EXPORT_GCODE_NOTIFICAION_CLICKED, ExportGcodeNotificationClickedEvent);
using PresetUpdateAvailableClickedEvent = SimpleEvent;
wxDECLARE_EVENT(EVT_PRESET_UPDATE_AVAILABLE_CLICKED, PresetUpdateAvailableClickedEvent);
using PrinterConfigUpdateAvailableClickedEvent = SimpleEvent;
wxDECLARE_EVENT(EVT_PRINTER_CONFIG_UPDATE_AVAILABLE_CLICKED, PrinterConfigUpdateAvailableClickedEvent);

wxDECLARE_EVENT(EVT_AMS_EXTRUSION_CALI, wxCommandEvent);
wxDECLARE_EVENT(EVT_AMS_LOAD, SimpleEvent);
wxDECLARE_EVENT(EVT_AMS_UNLOAD, SimpleEvent);
wxDECLARE_EVENT(EVT_AMS_SETTINGS, SimpleEvent);
wxDECLARE_EVENT(EVT_AMS_FILAMENT_BACKUP, SimpleEvent);
wxDECLARE_EVENT(EVT_AMS_REFRESH_RFID, wxCommandEvent);
wxDECLARE_EVENT(EVT_AMS_ON_SELECTED, wxCommandEvent);
wxDECLARE_EVENT(EVT_AMS_ON_FILAMENT_EDIT, wxCommandEvent);
wxDECLARE_EVENT(EVT_VAMS_ON_FILAMENT_EDIT, wxCommandEvent);
wxDECLARE_EVENT(EVT_AMS_CLIBRATION_AGAIN, wxCommandEvent);
wxDECLARE_EVENT(EVT_AMS_CLIBRATION_CANCEL, wxCommandEvent);
wxDECLARE_EVENT(EVT_AMS_GUIDE_WIKI, wxCommandEvent);
wxDECLARE_EVENT(EVT_AMS_RETRY, wxCommandEvent);
wxDECLARE_EVENT(EVT_AMS_SHOW_HUMIDITY_TIPS, wxCommandEvent);
wxDECLARE_EVENT(EVT_AMS_UNSELETED_VAMS, wxCommandEvent);
wxDECLARE_EVENT(EVT_CLEAR_SPEED_CONTROL, wxCommandEvent);

}
}