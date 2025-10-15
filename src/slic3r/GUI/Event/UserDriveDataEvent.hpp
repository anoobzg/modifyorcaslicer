#pragma once
#include "slic3r/GUI/Event/Event.hpp"

namespace Slic3r {
namespace GUI {

struct DriveData
{
	std::string name;
	std::string path;

	void clear() {
		name.clear();
		path.clear();
	}
	bool empty() const {
		return path.empty();
	}
};

inline bool operator< (const DriveData &lhs, const DriveData &rhs) { return lhs.path < rhs.path; }
inline bool operator> (const DriveData &lhs, const DriveData &rhs) { return lhs.path > rhs.path; }
inline bool operator==(const DriveData &lhs, const DriveData &rhs) { return lhs.path == rhs.path; }

using RemovableDriveEjectEvent = Event<std::pair<DriveData, bool>>;
wxDECLARE_EVENT(EVT_REMOVABLE_DRIVE_EJECTED, RemovableDriveEjectEvent);

using RemovableDrivesChangedEvent = SimpleEvent;
wxDECLARE_EVENT(EVT_REMOVABLE_DRIVES_CHANGED, RemovableDrivesChangedEvent);

using LoadFromOtherInstanceEvent = Event<std::vector<boost::filesystem::path>>;
using StartDownloadOtherInstanceEvent = Event<std::vector<std::string>>;
wxDECLARE_EVENT(EVT_LOAD_MODEL_OTHER_INSTANCE, LoadFromOtherInstanceEvent);
wxDECLARE_EVENT(EVT_START_DOWNLOAD_OTHER_INSTANCE, StartDownloadOtherInstanceEvent);

using InstanceGoToFrontEvent = SimpleEvent;
wxDECLARE_EVENT(EVT_INSTANCE_GO_TO_FRONT, InstanceGoToFrontEvent);

}
}