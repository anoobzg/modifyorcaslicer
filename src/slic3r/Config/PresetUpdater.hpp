#ifndef slic3r_PresetUpdate_hpp_
#define slic3r_PresetUpdate_hpp_

#include <memory>
#include <vector>

#include <wx/event.h>

namespace Slic3r {


class AppConfig;
class PresetBundle;
class Semver;

static constexpr const int SLIC3R_VERSION_BODY_MAX = 256;

class PresetUpdater
{
public:
	PresetUpdater();
	PresetUpdater(PresetUpdater &&) = delete;
	PresetUpdater(const PresetUpdater &) = delete;
	PresetUpdater &operator=(PresetUpdater &&) = delete;
	PresetUpdater &operator=(const PresetUpdater &) = delete;
	~PresetUpdater();

	// If version check is enabled, check if chaced online slic3r version is newer, notify if so.
	void slic3r_update_notify();

	enum UpdateResult {
		R_NOOP,
		R_INCOMPAT_EXIT,
		R_INCOMPAT_CONFIGURED,
		R_UPDATE_INSTALLED,
		R_UPDATE_REJECT,
		R_UPDATE_NOTIFICATION,
		R_ALL_CANCELED
	};

	enum class UpdateParams {
		SHOW_TEXT_BOX,				// force modal textbox
		SHOW_NOTIFICATION,			// only shows notification
		FORCED_BEFORE_WIZARD		// indicates that check of updated is forced before ConfigWizard opening
	};

	// "Update" a list of bundles from resources (behaves like an online update).
	bool install_bundles_rsrc(std::vector<std::string> bundles, bool snapshot = true) const;

	bool version_check_enabled() const;

private:
	struct priv;
	std::unique_ptr<priv> p;
};


}
#endif
