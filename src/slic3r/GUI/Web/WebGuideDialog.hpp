#ifndef slic3r_WebGuideDialog_hpp_
#define slic3r_WebGuideDialog_hpp_

#include "wx/webview.h"
#include "slic3r/GUI/GUI_Utils.hpp"

namespace Slic3r {
class AppConfig;
class PresetBundle;
class PresetUpdater;
namespace GUI {
class GuideFrame : public DPIDialog
{
public:
    GuideFrame(long style = wxCAPTION | wxCLOSE_BOX | wxSYSTEM_MENU);
    virtual ~GuideFrame();

    enum GuidePage {
        BBL_WELCOME,
        BBL_REGION,
        BBL_MODELS,
        BBL_FILAMENTS,
        BBL_FILAMENT_ONLY,
        BBL_MODELS_ONLY
    }m_page;

    //Web Function
    void load_url(wxString &url);
    wxString SetStartPage(GuidePage startpage=BBL_WELCOME, bool load = true);

    void UpdateState();
    void OnIdle(wxIdleEvent &evt);
    // void OnClose(wxCloseEvent &evt);

    void OnNavigationRequest(wxWebViewEvent &evt);
    void OnNavigationComplete(wxWebViewEvent &evt);
    void OnDocumentLoaded(wxWebViewEvent &evt);
    void OnNewWindow(wxWebViewEvent &evt);
    void OnError(wxWebViewEvent &evt);
    void OnTitleChanged(wxWebViewEvent &evt);
    void OnFullScreenChanged(wxWebViewEvent &evt);
    void OnScriptMessage(wxWebViewEvent &evt);

    void OnScriptResponseMessage(wxCommandEvent &evt);
    void RunScript(const wxString &javascript);

    //Logic
    bool IsFirstUse();

    //Model - Machine - Filaments
    int LoadProfile();
    int SaveProfile();


    bool apply_config(AppConfig *app_config, PresetBundle *preset_bundle, const PresetUpdater *updater, bool& apply_keeped_changes);
    bool run();

    void on_dpi_changed(const wxRect &suggested_rect) {}

private:
    AppConfig* m_appconfig_new;

    wxWebView *m_browser;

    wxString m_SectionName;

    bool orca_bundle_rsrc;

    // User Config
    bool PrivacyUse;
    bool StealthMode;
    std::string m_Region;

    bool InstallNetplugin;
    bool network_plugin_ready {false};

    std::string m_OrcaFilaLibPath;
    std::string m_editing_filament_id;
};

}} // namespace Slic3r::GUI

#endif /* slic3r_Tab_hpp_ */
