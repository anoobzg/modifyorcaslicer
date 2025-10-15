#include "UserDriveDataEvent.hpp"

namespace Slic3r {
namespace GUI {

wxDEFINE_EVENT(EVT_REMOVABLE_DRIVE_EJECTED, RemovableDriveEjectEvent);
wxDEFINE_EVENT(EVT_REMOVABLE_DRIVES_CHANGED, RemovableDrivesChangedEvent);

wxDEFINE_EVENT(EVT_LOAD_MODEL_OTHER_INSTANCE, LoadFromOtherInstanceEvent);
wxDEFINE_EVENT(EVT_START_DOWNLOAD_OTHER_INSTANCE, StartDownloadOtherInstanceEvent);
wxDEFINE_EVENT(EVT_INSTANCE_GO_TO_FRONT, InstanceGoToFrontEvent);

}
}