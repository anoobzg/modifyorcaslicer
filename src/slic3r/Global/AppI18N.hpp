#pragma once 

namespace Slic3r {
namespace GUI {
    bool app_load_language(wxString language, bool initial = false);
    void app_uninit_language();

    wxString        app_current_language_code();
	// Translate the language code to a code, for which Prusa Research maintains translations. Defaults to "en_US".
    wxString 		app_current_language_code_safe();
    bool            app_is_localized();
}
}