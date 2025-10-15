#include "WebGuideDialog.hpp"

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/FileSystem/DataDir.hpp"
#include "libslic3r/FileSystem/FileHelp.hpp"
#include "libslic3r/Config/PresetLoad.hpp"

#include "slic3r/Utils/AppWx.hpp"
#include "slic3r/Config/AppPreset.hpp"
#include "slic3r/Theme/AppColor.hpp"
#include "slic3r/Global/AppModule.hpp"
#include "slic3r/Config/PresetUpdater.hpp"
#include "slic3r/GUI/Dialog/DialogCommand.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/Global/AppI18N.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/MainPanel.hpp"
#include "slic3r/GUI/Widgets/WebView.hpp"
#include "slic3r/GUI/Dialog/ParamsDialog.hpp"
#include "slic3r/GUI/Event/UserNetEvent.hpp"
#include "slic3r/GUI/Event/UserPlaterEvent.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"

using namespace nlohmann;

namespace Slic3r { namespace GUI {

json m_ProfileJson;

static wxString update_custom_filaments()
{
    json m_Res                                                                     = json::object();
    m_Res["command"]                                                               = "update_custom_filaments";
    m_Res["sequence_id"]                                                           = "2000";
    json                                               m_CustomFilaments           = json::array();
    PresetBundle *                                     preset_bundle               = app_preset_bundle();
    std::map<std::string, std::vector<Preset const *>> temp_filament_id_to_presets = preset_bundle->filaments.get_filament_presets();
    
    std::vector<std::pair<std::string, std::string>>   need_sort;
    bool                                             need_delete_some_filament = false;
    for (std::pair<std::string, std::vector<Preset const *>> filament_id_to_presets : temp_filament_id_to_presets) {
        std::string filament_id = filament_id_to_presets.first;
        if (filament_id.empty()) continue;
        if (filament_id == "null") {
            need_delete_some_filament = true;
        }
        bool filament_with_base_id = false;
        bool not_need_show = false;
        std::string filament_name;
        for (const Preset *preset : filament_id_to_presets.second) {
            if (preset->is_system || preset->is_project_embedded) {
                not_need_show = true;
                break;
            }
            if (preset->inherits() != "") continue;
            if (!preset->base_id.empty()) filament_with_base_id = true;

            if (!not_need_show) {
                auto filament_vendor = dynamic_cast<ConfigOptionStrings *>(const_cast<Preset *>(preset)->config.option("filament_vendor", false));
                if (filament_vendor && filament_vendor->values.size() && filament_vendor->values[0] == "Generic") not_need_show = true;
            }
            
            if (filament_name.empty()) {
                std::string preset_name = preset->name;
                size_t      index_at    = preset_name.find(" @");
                if (std::string::npos != index_at) { preset_name = preset_name.substr(0, index_at); }
                filament_name = preset_name;
            }
        }
        if (not_need_show) continue;
        if (!filament_name.empty()) {
            if (filament_with_base_id) {
                need_sort.push_back(std::make_pair("[Action Required] " + filament_name, filament_id));
            } else {

                need_sort.push_back(std::make_pair(filament_name, filament_id));
            }
        }
    }
    std::sort(need_sort.begin(), need_sort.end(), [](const std::pair<std::string, std::string> &a, const std::pair<std::string, std::string> &b) { return a.first < b.first; });
    if (need_delete_some_filament) {
        need_sort.push_back(std::make_pair("[Action Required]", "null"));
    }
    json temp_j;
    for (std::pair<std::string, std::string> &filament_name_to_id : need_sort) {
        temp_j["name"] = filament_name_to_id.first;
        temp_j["id"]   = filament_name_to_id.second;
        m_CustomFilaments.push_back(temp_j);
    }
    m_Res["data"]  = m_CustomFilaments;
    wxString strJS = wxString::Format("HandleStudio(%s)", wxString::FromUTF8(m_Res.dump(-1, ' ', false, json::error_handler_t::ignore)));
    return strJS;
}

GuideFrame::GuideFrame(long style)
    : DPIDialog(app_main_window(), wxID_ANY, "OrcaSlicer", wxDefaultPosition, wxDefaultSize, style),
	m_appconfig_new(new AppConfig())
{
    SetBackgroundColour(*wxWHITE);
    // INI
    m_SectionName = "firstguide";
    PrivacyUse    = false;
    StealthMode   = false;
    InstallNetplugin = false;

    // set the frame icon
    wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);

    wxString TargetUrl = SetStartPage(BBL_WELCOME, false);
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(",  set start page to welcome ");

    // Create the webview
    m_browser = WebView::CreateWebView(this, TargetUrl);
    if (m_browser == nullptr) {
        wxLogError("Could not init m_browser");
        return;
    }
    m_browser->Hide();
    m_browser->SetSize(0, 0);
    
    SetSizer(topsizer);

    topsizer->Add(m_browser, wxSizerFlags().Expand().Proportion(1));

    // Log backend information
    // wxLogMessage(wxWebView::GetBackendVersionInfo().ToString());
    // wxLogMessage("Backend: %s Version: %s",
    // m_browser->GetClassInfo()->GetClassName(),wxWebView::GetBackendVersionInfo().ToString());
    // wxLogMessage("User Agent: %s", m_browser->GetUserAgent());

    // Set a more sensible size for web browsing
    wxSize pSize = FromDIP(wxSize(820, 660));
    SetSize(pSize);

    int screenheight = wxSystemSettings::GetMetric(wxSYS_SCREEN_Y, NULL);
    int screenwidth  = wxSystemSettings::GetMetric(wxSYS_SCREEN_X, NULL);
    int MaxY         = (screenheight - pSize.y) > 0 ? (screenheight - pSize.y) / 2 : 0;
    wxPoint tmpPT((screenwidth - pSize.x) / 2, MaxY);
    Move(tmpPT);
#ifdef __WXMSW__
    this->Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& e) {
        if ((m_page == BBL_FILAMENT_ONLY || m_page == BBL_MODELS_ONLY) && e.GetKeyCode() == WXK_ESCAPE) {
            if (this->IsModal())
                this->EndModal(wxID_CANCEL);
            else
                this->Close();
        }
        else
            e.Skip();
        });
#endif
    // Connect the webview events
    Bind(wxEVT_WEBVIEW_NAVIGATING, &GuideFrame::OnNavigationRequest, this, m_browser->GetId());
    Bind(wxEVT_WEBVIEW_NAVIGATED, &GuideFrame::OnNavigationComplete, this, m_browser->GetId());
    Bind(wxEVT_WEBVIEW_LOADED, &GuideFrame::OnDocumentLoaded, this, m_browser->GetId());
    Bind(wxEVT_WEBVIEW_ERROR, &GuideFrame::OnError, this, m_browser->GetId());
    Bind(wxEVT_WEBVIEW_NEWWINDOW, &GuideFrame::OnNewWindow, this, m_browser->GetId());
    Bind(wxEVT_WEBVIEW_TITLE_CHANGED, &GuideFrame::OnTitleChanged, this, m_browser->GetId());
    Bind(wxEVT_WEBVIEW_FULLSCREEN_CHANGED, &GuideFrame::OnFullScreenChanged, this, m_browser->GetId());
    Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED, &GuideFrame::OnScriptMessage, this, m_browser->GetId());

    auto start = std::chrono::high_resolution_clock::now();
    LoadProfile();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << ": LoadProfile() took " << duration.count() << " milliseconds";

    // UI
    SetStartPage(BBL_REGION);

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(",  finished");
    UpdateDlgDarkUI(this);
}

GuideFrame::~GuideFrame()
{
    delete m_appconfig_new;
    if (m_browser) {
        delete m_browser;
        m_browser = nullptr;
    }
}

void GuideFrame::load_url(wxString &url)
{
    BOOST_LOG_TRIVIAL(trace) << __FUNCTION__<< " enter, url=" << url.ToStdString();
    WebView::LoadUrl(m_browser, url);
    m_browser->SetFocus();
    UpdateState();

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< " exit";
}

wxString GuideFrame::SetStartPage(GuidePage startpage, bool load)
{
    m_page = startpage;
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< boost::format(" enter, load=%1%, start_page=%2%")%load%int(startpage);
    //wxLogMessage("GUIDE: webpage_1  %s", (boost::filesystem::path(resources_dir()) / "web\\guide\\1\\index.html").make_preferred().string().c_str() );
    wxString TargetUrl = from_u8( (boost::filesystem::path(resources_dir()) / "web/guide/1/index.html").make_preferred().string() );
    //wxLogMessage("GUIDE: webpage_2  %s", TargetUrl.mb_str());

    if (startpage == BBL_WELCOME){
        SetTitle(_L("Setup Wizard"));
        TargetUrl = from_u8((boost::filesystem::path(resources_dir()) / "web/guide/1/index.html").make_preferred().string());
    } else if (startpage == BBL_REGION) {
        SetTitle(_L("Setup Wizard"));
        TargetUrl = from_u8((boost::filesystem::path(resources_dir()) / "web/guide/11/index.html").make_preferred().string());
    } else if (startpage == BBL_MODELS) {
        SetTitle(_L("Setup Wizard"));
        TargetUrl = from_u8((boost::filesystem::path(resources_dir()) / "web/guide/21/index.html").make_preferred().string());
    } else if (startpage == BBL_FILAMENTS) {
        SetTitle(_L("Setup Wizard"));

        int nSize = m_ProfileJson["model"].size();

        if (nSize>0)
            TargetUrl = from_u8((boost::filesystem::path(resources_dir()) / "web/guide/22/index.html").make_preferred().string());
        else
            TargetUrl = from_u8((boost::filesystem::path(resources_dir()) / "web/guide/21/index.html").make_preferred().string());
    } else if (startpage == BBL_FILAMENT_ONLY) {
        SetTitle("");
        TargetUrl = from_u8((boost::filesystem::path(resources_dir()) / "web/guide/23/index.html").make_preferred().string());
    } else if (startpage == BBL_MODELS_ONLY) {
        SetTitle("");
        TargetUrl = from_u8((boost::filesystem::path(resources_dir()) / "web/guide/24/index.html").make_preferred().string());
    }
    else {
        SetTitle(_L("Setup Wizard"));
        TargetUrl = from_u8((boost::filesystem::path(resources_dir()) / "web/guide/21/index.html").make_preferred().string());
    }

    wxString strlang = app_current_language_code_safe();
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< boost::format(", strlang=%1%") % into_u8(strlang);
    if (strlang != "")
        TargetUrl = wxString::Format("%s?lang=%s", w2s(TargetUrl), strlang);

    TargetUrl = "file://" + TargetUrl;
    if (load)
        load_url(TargetUrl);

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< " exit";
    return TargetUrl;
}

/**
 * Method that retrieves the current state from the web control and updates
 * the GUI the reflect this current state.
 */
void GuideFrame::UpdateState()
{
    // SetTitle(m_browser->GetCurrentTitle());
}

void GuideFrame::OnIdle(wxIdleEvent &WXUNUSED(evt))
{
    if (m_browser->IsBusy()) {
        wxSetCursor(wxCURSOR_ARROWWAIT);
    } else {
        wxSetCursor(wxNullCursor);
    }
}

// void GuideFrame::OnClose(wxCloseEvent& evt)
//{
//    this->Hide();
//}

/**
 * Callback invoked when there is a request to load a new page (for instance
 * when the user clicks a link)
 */
void GuideFrame::OnNavigationRequest(wxWebViewEvent &evt)
{
    // wxLogMessage("%s", "Navigation request to '" + evt.GetURL() + "'
    // (target='" + evt.GetTarget() + "')");

    UpdateState();
}

/**
 * Callback invoked when a navigation request was accepted
 */
void GuideFrame::OnNavigationComplete(wxWebViewEvent &evt)
{
    //wxLogMessage("%s", "Navigation complete; url='" + evt.GetURL() + "'");
    m_browser->Show();
    Layout();
    
    wxString NewUrl = evt.GetURL();

    UpdateState();
}

/**
 * Callback invoked when a page is finished loading
 */
void GuideFrame::OnDocumentLoaded(wxWebViewEvent &evt)
{
    // Only notify if the document is the main frame, not a subframe
    wxString tmpUrl = evt.GetURL();
    wxString NowUrl = m_browser->GetCurrentURL();

    if (evt.GetURL() == m_browser->GetCurrentURL()) {
        // wxLogMessage("%s", "Document loaded; url='" + evt.GetURL() + "'");
    }
    UpdateState();

    // wxCommandEvent *event = new
    // wxCommandEvent(EVT_WEB_RESPONSE_MESSAGE,this->GetId()); wxQueueEvent(this,
    // event);
}

/**
 * On new window, we veto to stop extra windows appearing
 */
void GuideFrame::OnNewWindow(wxWebViewEvent &evt)
{
    wxString flag = " (other)";

    wxString NewUrl= evt.GetURL();
    wxLaunchDefaultBrowser(NewUrl);
    //if (evt.GetNavigationAction() == wxWEBVIEW_NAV_ACTION_USER) { flag = " (user)"; }
    // wxLogMessage("%s", "New window; url='" + evt.GetURL() + "'" + flag);

    // If we handle new window events then just load them in this window as we
    // are a single window browser
    // if (m_tools_handle_new_window->IsChecked())
    //    m_browser->LoadURL(evt.GetURL());

    UpdateState();
}

void GuideFrame::OnTitleChanged(wxWebViewEvent &evt)
{
    // SetTitle(evt.GetString());
    // wxLogMessage("%s", "Title changed; title='" + evt.GetString() + "'");
}

void GuideFrame::OnFullScreenChanged(wxWebViewEvent &evt)
{
    // wxLogMessage("Full screen changed; status = %d", evt.GetInt());
    ShowFullScreen(evt.GetInt() != 0);
}

void GuideFrame::OnScriptMessage(wxWebViewEvent &evt)
{
    try {
        wxString strInput = evt.GetString();
        BOOST_LOG_TRIVIAL(trace) << "GuideFrame::OnScriptMessage;OnRecv:" << strInput.c_str();
        json     j        = json::parse(strInput);

        wxString strCmd = j["command"];
        BOOST_LOG_TRIVIAL(trace) << "GuideFrame::OnScriptMessage;Command:" << strCmd;

        if (strCmd == "close_page") {
            this->EndModal(wxID_CANCEL);
        }
        if (strCmd == "user_clause") {
            wxString strAction = j["data"]["action"];

            if (strAction == "refuse") {
                // CloseTheApp
                this->EndModal(wxID_OK);

                AppAdapter::main_panel()->Close(); // Refuse Clause, App quit immediately
            }
        } else if (strCmd == "user_private_choice") {
            wxString strAction = j["data"]["action"];

            if (strAction == "agree") {
                PrivacyUse = true;
            } else {
                PrivacyUse = false;
            }
        }
        else if (strCmd == "request_userguide_profile") {
            json m_Res = json::object();
            m_Res["command"] = "response_userguide_profile";
            m_Res["sequence_id"] = "10001";
            m_Res["response"]        = m_ProfileJson;

            //wxString strJS = wxString::Format("HandleStudio(%s)", m_Res.dump(-1, ' ', false, json::error_handler_t::ignore));
            wxString strJS = wxString::Format("HandleStudio(%s)", m_Res.dump(-1, ' ', true));

            BOOST_LOG_TRIVIAL(trace) << "GuideFrame::OnScriptMessage;request_userguide_profile:" << strJS.c_str();
            AppAdapter::app()->CallAfter([this,strJS] { RunScript(strJS); });
        }
        else if (strCmd == "request_custom_filaments") {
            wxString strJS = update_custom_filaments();
            AppAdapter::app()->CallAfter([this, strJS] { RunScript(strJS); });
        }
        else if (strCmd == "create_custom_filament") {
            this->EndModal(wxID_OK);
            wxQueueEvent(AppAdapter::plater(), new SimpleEvent(EVT_CREATE_FILAMENT));
        } else if (strCmd == "modify_custom_filament") {
            m_editing_filament_id = j["id"];
            this->EndModal(wxID_EDIT);
        }
        else if (strCmd == "save_userguide_models")
        {
            json MSelected = j["data"];

            int nModel = m_ProfileJson["model"].size();
            for (int m = 0; m < nModel; m++) {
                json TmpModel = m_ProfileJson["model"][m];
                m_ProfileJson["model"][m]["nozzle_selected"] = "";

                for (auto it = MSelected.begin(); it != MSelected.end(); ++it) {
                    json OneSelect = it.value();

                    wxString s1 = TmpModel["model"];
                    wxString s2 = OneSelect["model"];
                    if (s1.compare(s2) == 0) {
                        m_ProfileJson["model"][m]["nozzle_selected"] = OneSelect["nozzle_diameter"];
                        break;
                    }
                }
            }
        }
        else if (strCmd == "save_userguide_filaments") {
            //reset
            for (auto it = m_ProfileJson["filament"].begin(); it != m_ProfileJson["filament"].end(); ++it)
            {
                m_ProfileJson["filament"][it.key()]["selected"] = 0;
            }

            json fSelected = j["data"]["filament"];
            int nF = fSelected.size();
            for (int m = 0; m < nF; m++)
            {
                std::string fName = fSelected[m];

                m_ProfileJson["filament"][fName]["selected"] = 1;
            }
        }
        else if (strCmd == "user_guide_finish") {
            SaveProfile();
            this->EndModal(wxID_OK);
        }
        else if (strCmd == "user_guide_cancel") {
            this->EndModal(wxID_CANCEL);
            this->Close();
        } else if (strCmd == "save_region") {
            m_Region = j["region"];
        }
        else if (strCmd == "network_plugin_install") {
            std::string sAction = j["data"]["action"];

            if (sAction == "yes") {
                if (!network_plugin_ready)
                    InstallNetplugin = true;
                else //already ready
                    InstallNetplugin = false;
            }
            else
                InstallNetplugin = false;
        }
        else if (strCmd == "save_stealth_mode") {
            wxString strAction = j["data"]["action"];

            if (strAction == "yes") {
                StealthMode = true;
            } else {
                StealthMode = false;
            }
        }
    } catch (std::exception &e) {
        // wxMessageBox(e.what(), "json Exception", MB_OK);
        BOOST_LOG_TRIVIAL(trace) << "GuideFrame::OnScriptMessage;Error:" << e.what();
    }

    wxString strAll = m_ProfileJson.dump(-1,' ',false, json::error_handler_t::ignore);
}

void GuideFrame::RunScript(const wxString &javascript)
{
    if (!m_browser) return;

    WebView::RunScript(m_browser, javascript);
}

#if wxUSE_WEBVIEW_IE
void GuideFrame::OnRunScriptObjectWithEmulationLevel(wxCommandEvent &WXUNUSED(evt))
{
    wxWebViewIE::MSWSetModernEmulationLevel();
    RunScript("function f(){var person = new Object();person.name = 'Foo'; \
    person.lastName = 'Bar';return person;}f();");
    wxWebViewIE::MSWSetModernEmulationLevel(false);
}

void GuideFrame::OnRunScriptDateWithEmulationLevel(wxCommandEvent &WXUNUSED(evt))
{
    wxWebViewIE::MSWSetModernEmulationLevel();
    RunScript("function f(){var d = new Date('10/08/2017 21:30:40'); \
    var tzoffset = d.getTimezoneOffset() * 60000; return \
    new Date(d.getTime() - tzoffset);}f();");
    wxWebViewIE::MSWSetModernEmulationLevel(false);
}

void GuideFrame::OnRunScriptArrayWithEmulationLevel(wxCommandEvent &WXUNUSED(evt))
{
    wxWebViewIE::MSWSetModernEmulationLevel();
    RunScript("function f(){ return [\"foo\", \"bar\"]; }f();");
    wxWebViewIE::MSWSetModernEmulationLevel(false);
}
#endif

/**
 * Callback invoked when a loading error occurs
 */
void GuideFrame::OnError(wxWebViewEvent &evt)
{
#define WX_ERROR_CASE(type) \
    case type: category = #type; break;

    wxString category;
    switch (evt.GetInt()) {
        WX_ERROR_CASE(wxWEBVIEW_NAV_ERR_CONNECTION);
        WX_ERROR_CASE(wxWEBVIEW_NAV_ERR_CERTIFICATE);
        WX_ERROR_CASE(wxWEBVIEW_NAV_ERR_AUTH);
        WX_ERROR_CASE(wxWEBVIEW_NAV_ERR_SECURITY);
        WX_ERROR_CASE(wxWEBVIEW_NAV_ERR_NOT_FOUND);
        WX_ERROR_CASE(wxWEBVIEW_NAV_ERR_REQUEST);
        WX_ERROR_CASE(wxWEBVIEW_NAV_ERR_USER_CANCELLED);
        WX_ERROR_CASE(wxWEBVIEW_NAV_ERR_OTHER);
    }

    BOOST_LOG_TRIVIAL(trace) << "GuideFrame::OnError: An error occurred loading " << evt.GetURL() << category;

    UpdateState();
}

void GuideFrame::OnScriptResponseMessage(wxCommandEvent &WXUNUSED(evt))
{
}

bool GuideFrame::IsFirstUse()
{
    wxString    strUse;
    std::string strVal = AppAdapter::app_config()->get(std::string(m_SectionName.mb_str()), "finish");
    if (strVal == "1")
        return false;

    if (orca_bundle_rsrc == true)
        return true;

    return true;
}

int GuideFrame::SaveProfile()
{
    // SoftFever: don't collect info
    //privacy
    // if (PrivacyUse == true) {
    //     AppAdapter::app_config()->set(std::string(m_SectionName.mb_str()), "privacyuse", "1");
    // } else
    //     AppAdapter::app_config()->set(std::string(m_SectionName.mb_str()), "privacyuse", "0");

    AppAdapter::app_config()->set("region", m_Region);
    AppAdapter::app_config()->set_bool("stealth_mode", StealthMode);

    //finish
    AppAdapter::app_config()->set(std::string(m_SectionName.mb_str()), "finish", "1");

    AppAdapter::app_config()->save();

    std::string strAll = m_ProfileJson.dump(-1, ' ', false, json::error_handler_t::ignore);

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "before save to app_config: "<< std::endl<<strAll;

    //set filaments to app_config
    const std::string &section_name = AppConfig::SECTION_FILAMENTS;
    std::map<std::string, std::string> section_new;
    m_appconfig_new->clear_section(section_name);
    for (auto it = m_ProfileJson["filament"].begin(); it != m_ProfileJson["filament"].end(); ++it) {
        if (it.value()["selected"] == 1){
            section_new[it.key()] = "true";
        }
    }
    m_appconfig_new->set_section(section_name, section_new);

    //set vendors to app_config
    Slic3r::AppConfig::VendorMap empty_vendor_map;
    m_appconfig_new->set_vendors(empty_vendor_map);
    for (auto it = m_ProfileJson["model"].begin(); it != m_ProfileJson["model"].end(); ++it)
    {
        if (it.value().is_object()) {
            json temp_model = it.value();
            std::string model_name = temp_model["model"];
            std::string vendor_name = temp_model["vendor"];
            std::string selected = temp_model["nozzle_selected"];
            boost::trim(selected);
            std::string nozzle;
            while (selected.size() > 0) {
                auto pos = selected.find(';');
                if (pos != std::string::npos) {
                    nozzle   = selected.substr(0, pos);
                    m_appconfig_new->set_variant(vendor_name, model_name, nozzle, "true");
                    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format("vendor_name %1%, model_name %2%, nozzle %3% selected")%vendor_name %model_name %nozzle;
                    selected = selected.substr(pos + 1);
                    boost::trim(selected);
                }
                else {
                    m_appconfig_new->set_variant(vendor_name, model_name, selected, "true");
                    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format("vendor_name %1%, model_name %2%, nozzle %3% selected")%vendor_name %model_name %selected;
                    break;
                }
            }
        }
    }

    //m_appconfig_new

    return 0;
}

static std::set<std::string> get_new_added_presets(const std::map<std::string, std::string>& old_data, const std::map<std::string, std::string>& new_data)
{
    auto get_aliases = [](const std::map<std::string, std::string>& data) {
        std::set<std::string> old_aliases;
        for (auto item : data) {
            const std::string& name = item.first;
            size_t pos = name.find("@");
            old_aliases.emplace(pos == std::string::npos ? name : name.substr(0, pos-1));
        }
        return old_aliases;
    };

    std::set<std::string> old_aliases = get_aliases(old_data);
    std::set<std::string> new_aliases = get_aliases(new_data);
    std::set<std::string> diff;
    std::set_difference(new_aliases.begin(), new_aliases.end(), old_aliases.begin(), old_aliases.end(), std::inserter(diff, diff.begin()));

    return diff;
}

static std::string get_first_added_preset(const std::map<std::string, std::string>& old_data, const std::map<std::string, std::string>& new_data)
{
    std::set<std::string> diff = get_new_added_presets(old_data, new_data);
    if (diff.empty())
        return std::string();
    return *diff.begin();
}

bool GuideFrame::apply_config(AppConfig *app_config, PresetBundle *preset_bundle, const PresetUpdater *updater, bool& apply_keeped_changes)
{
    const auto enabled_vendors = m_appconfig_new->vendors();
    const auto old_enabled_vendors = app_config->vendors();

    const auto enabled_filaments = m_appconfig_new->has_section(AppConfig::SECTION_FILAMENTS) ? m_appconfig_new->get_section(AppConfig::SECTION_FILAMENTS) : std::map<std::string, std::string>();
    const auto old_enabled_filaments = app_config->has_section(AppConfig::SECTION_FILAMENTS) ? app_config->get_section(AppConfig::SECTION_FILAMENTS) : std::map<std::string, std::string>();

    bool check_unsaved_preset_changes = false;
    std::vector<std::string> install_bundles;
    std::vector<std::string> remove_bundles;
    const auto vendor_dir = (boost::filesystem::path(Slic3r::user_data_dir()) / PRESET_SYSTEM_DIR).make_preferred();
    for (const auto &it : enabled_vendors) {
        if (it.second.size() > 0) {
            auto vendor_file = vendor_dir/(it.first + ".json");
            if (!fs::exists(vendor_file)) {
                install_bundles.emplace_back(it.first);
            }
        }
    }

    //add the removed vendor bundles
    for (const auto &it : old_enabled_vendors) {
        if (it.second.size() > 0) {
            if (enabled_vendors.find(it.first) != enabled_vendors.end())
                continue;
            auto vendor_file = vendor_dir/(it.first + ".json");
            if (fs::exists(vendor_file)) {
                remove_bundles.emplace_back(it.first);
            }
        }
    }

    check_unsaved_preset_changes = (enabled_vendors != old_enabled_vendors) || (enabled_filaments != old_enabled_filaments);
    wxString header = _L("The configuration package is changed in previous Config Guide");
    wxString caption = _L("Configuration package changed");
    int act_btns = ActionButtons::KEEP|ActionButtons::SAVE;

    if (check_unsaved_preset_changes &&
        !AppAdapter::gui_app()->check_and_keep_current_preset_changes(caption, header, act_btns, &apply_keeped_changes))
        return false;

    if (install_bundles.size() > 0) {
        // Install bundles from resources.
        // Don't create snapshot - we've already done that above if applicable.
        if (! updater->install_bundles_rsrc(std::move(install_bundles), false))
            return false;
    } else {
        BOOST_LOG_TRIVIAL(info) << "No bundles need to be installed from resource directory";
    }

    std::string preferred_model;
    std::string preferred_variant;
    PrinterTechnology preferred_pt = ptFFF;
    auto get_preferred_printer_model = [preset_bundle, enabled_vendors, old_enabled_vendors, preferred_pt](const std::string& bundle_name, std::string& variant) {
        const auto config = enabled_vendors.find(bundle_name);
        if (config == enabled_vendors.end())
            return std::string();

        const std::map<std::string, std::set<std::string>>& model_maps = config->second;
        //for (const auto& vendor_profile : preset_bundle->vendors) {
        for (const auto& model_it: model_maps) {
            if (model_it.second.size() > 0) {
                variant = *model_it.second.begin();
                const auto config_old = old_enabled_vendors.find(bundle_name);
                if (config_old == old_enabled_vendors.end())
                    return model_it.first;
                const auto model_it_old = config_old->second.find(model_it.first);
                if (model_it_old == config_old->second.end())
                    return model_it.first;
                else if (model_it_old->second != model_it.second) {
                    for (const auto& var : model_it.second)
                        if (model_it_old->second.find(var) == model_it_old->second.end()) {
                            variant = var;
                            return model_it.first;
                        }
                }
            }
        }
        //}
        if (!variant.empty())
            variant.clear();
        return std::string();
    };
    // Orca "custom" printers are considered first, then 3rd party.
    if (preferred_model = get_preferred_printer_model(PresetBundle::ORCA_DEFAULT_BUNDLE, preferred_variant);
        preferred_model.empty()) {
        for (const auto& bundle : enabled_vendors) {
            if (bundle.first == PresetBundle::ORCA_DEFAULT_BUNDLE) { continue; }
            if (preferred_model = get_preferred_printer_model(bundle.first, preferred_variant);
                !preferred_model.empty())
                    break;
        }
    }

    std::string first_added_filament;
    auto get_first_added_material_preset = [this, app_config](const std::string& section_name, std::string& first_added_preset) {
        if (m_appconfig_new->has_section(section_name)) {
            // get first of new added preset names
            const std::map<std::string, std::string>& old_presets = app_config->has_section(section_name) ? app_config->get_section(section_name) : std::map<std::string, std::string>();
            first_added_preset = get_first_added_preset(old_presets, m_appconfig_new->get_section(section_name));
        }
    };
    // Not switch filament
    //get_first_added_material_preset(AppConfig::SECTION_FILAMENTS, first_added_filament);

    //update the app_config
    app_config->set_section(AppConfig::SECTION_FILAMENTS, enabled_filaments);
    app_config->set_vendors(*m_appconfig_new);

    if (check_unsaved_preset_changes)
        preset_bundle->load_presets(*app_config, ForwardCompatibilitySubstitutionRule::Enable,
                                    {preferred_model, preferred_variant, first_added_filament, std::string()});

    // Update the selections from the compatibilty.
    preset_bundle->export_selections(*app_config);

    return true;
}

bool GuideFrame::run()
{
    //BOOST_LOG_TRIVIAL(info) << boost::format("Running ConfigWizard, reason: %1%, start_page: %2%") % reason % start_page;

    GUI_App* app = AppAdapter::gui_app();

    //p->set_run_reason(reason);
    //p->set_start_page(start_page);
    app->preset_bundle->export_selections(*app->app_config);

    BOOST_LOG_TRIVIAL(info) << "GuideFrame before ShowModal";
    // display position
    int main_frame_display_index = wxDisplay::GetFromWindow(AppAdapter::main_panel());
    int guide_display_index = wxDisplay::GetFromWindow(this);
    if (main_frame_display_index != guide_display_index) {
        wxDisplay display    = wxDisplay(main_frame_display_index);
        wxRect    screenRect = display.GetGeometry();
        int       guide_x    = screenRect.x + (screenRect.width - this->GetSize().GetWidth()) / 2;
        int       guide_y    = screenRect.y + (screenRect.height - this->GetSize().GetHeight()) / 2;
        this->SetPosition(wxPoint(guide_x, guide_y));
    }

    int result = this->ShowModal();
    if (result == wxID_OK) {
        bool apply_keeped_changes = false;
        BOOST_LOG_TRIVIAL(info) << "GuideFrame returned ok";
        if (! this->apply_config(app->app_config, app->preset_bundle, app->preset_updater, apply_keeped_changes))
            return false;

        if (apply_keeped_changes)
            app->apply_keeped_preset_modifications();

        app->app_config->set_legacy_datadir(false);
        app->update_mode();
        BOOST_LOG_TRIVIAL(info) << "GuideFrame applied";
        this->Close();
        return true;
    } else if (result == wxID_CANCEL) {
        BOOST_LOG_TRIVIAL(info) << "GuideFrame cancelled";
        if (app->preset_bundle->printers.only_default_printers()) {
            //we install the default here
            bool apply_keeped_changes = false;
            //clear filament section and use default materials
            app->app_config->set_variant(PresetBundle::ORCA_DEFAULT_BUNDLE,
                PresetBundle::ORCA_DEFAULT_PRINTER_MODEL, PresetBundle::ORCA_DEFAULT_PRINTER_VARIANT, "true");
            app->app_config->clear_section(AppConfig::SECTION_FILAMENTS);
            app->preset_bundle->load_selections(*app->app_config, {PresetBundle::ORCA_DEFAULT_PRINTER_MODEL, PresetBundle::ORCA_DEFAULT_PRINTER_VARIANT, PresetBundle::ORCA_DEFAULT_FILAMENT, std::string()});

            app->app_config->set_legacy_datadir(false);
            app->update_mode();
            return true;
        }
        else
            return false;
    } else if (result == wxID_EDIT) {
        this->Close();
        Filamentinformation *filament_info = new Filamentinformation();
        filament_info->filament_id        = m_editing_filament_id;
        wxQueueEvent(AppAdapter::plater(), new SimpleEvent(EVT_MODIFY_FILAMENT, filament_info));
        return false;
    }
    else
        return false;
}

int GuideFrame::LoadProfile()
{
    try {
        m_ProfileJson             = json::parse("{}");
        m_ProfileJson["model"]    = json::array();
        m_ProfileJson["machine"]  = json::object();
        m_ProfileJson["filament"] = json::object();
        m_ProfileJson["process"]  = json::array();

        const auto vendor_dir      = (boost::filesystem::path(Slic3r::user_data_dir()) / PRESET_SYSTEM_DIR ).make_preferred();
        const auto rsrc_vendor_dir = (boost::filesystem::path(resources_dir()) / "profiles").make_preferred();

        // Orca: add custom as default
        // Orca: add json logic for vendor bundle
        orca_bundle_rsrc = true;

        // search if there exists a .json file in vendor_dir folder, if exists, set orca_bundle_rsrc to false
        for (const auto& entry : boost::filesystem::directory_iterator(vendor_dir)) {
            if (!boost::filesystem::is_directory(entry) && boost::iequals(entry.path().extension().string(), ".json") && !boost::iequals(entry.path().stem().string(), PresetBundle::ORCA_FILAMENT_LIBRARY)) {
                orca_bundle_rsrc = false;
                break;
            }
        }

        // load the default filament library first
        std::set<std::string> loaded_vendors;
        auto filament_library_name = boost::filesystem::path(PresetBundle::ORCA_FILAMENT_LIBRARY).replace_extension(".json");
        if (boost::filesystem::exists(vendor_dir / filament_library_name)) {
            load_profile_family(PresetBundle::ORCA_FILAMENT_LIBRARY, (vendor_dir / filament_library_name).string(), m_ProfileJson);
            m_OrcaFilaLibPath = (vendor_dir / PresetBundle::ORCA_FILAMENT_LIBRARY).string();
        } else {
            load_profile_family(PresetBundle::ORCA_FILAMENT_LIBRARY, (rsrc_vendor_dir / filament_library_name).string(), m_ProfileJson);
            m_OrcaFilaLibPath = (rsrc_vendor_dir / PresetBundle::ORCA_FILAMENT_LIBRARY).string();
        }
        loaded_vendors.insert(PresetBundle::ORCA_FILAMENT_LIBRARY);

        //load custom bundle from user data path
        boost::filesystem::directory_iterator endIter;
        for (boost::filesystem::directory_iterator iter(vendor_dir); iter != endIter; iter++) {
            if (!boost::filesystem::is_directory(*iter)) {
                wxString strVendor = from_u8(iter->path().string()).BeforeLast('.');
                strVendor          = strVendor.AfterLast('\\');
                strVendor          = strVendor.AfterLast('/');

                wxString strExtension = from_u8(iter->path().string()).AfterLast('.').Lower();
                if(strExtension.CmpNoCase("json") != 0 || loaded_vendors.find(w2s(strVendor)) != loaded_vendors.end())
                    continue;

                load_profile_family(w2s(strVendor), iter->path().string(), m_ProfileJson);
                loaded_vendors.insert(w2s(strVendor));
            }
        }

        boost::filesystem::directory_iterator others_endIter;
        for (boost::filesystem::directory_iterator iter(rsrc_vendor_dir); iter != others_endIter; iter++) {
            if (!boost::filesystem::is_directory(*iter)) {
                wxString strVendor = from_u8(iter->path().string()).BeforeLast('.');
                strVendor          = strVendor.AfterLast('\\');
                strVendor          = strVendor.AfterLast('/');
                wxString strExtension = from_u8(iter->path().string()).AfterLast('.').Lower();
                if (strExtension.CmpNoCase("json") != 0 || loaded_vendors.find(w2s(strVendor)) != loaded_vendors.end())
                    continue;

                load_profile_family(w2s(strVendor), iter->path().string(), m_ProfileJson);
                loaded_vendors.insert(w2s(strVendor));
            }
        }



        const auto enabled_filaments = AppAdapter::app_config()->has_section(AppConfig::SECTION_FILAMENTS) ? AppAdapter::app_config()->get_section(AppConfig::SECTION_FILAMENTS) : std::map<std::string, std::string>();
        m_appconfig_new->set_vendors(*AppAdapter::app_config());
        m_appconfig_new->set_section(AppConfig::SECTION_FILAMENTS, enabled_filaments);

        for (auto it = m_ProfileJson["model"].begin(); it != m_ProfileJson["model"].end(); ++it)
        {
            if (it.value().is_object()) {
                json& temp_model = it.value();
                std::string model_name = temp_model["model"];
                std::string vendor_name = temp_model["vendor"];
                std::string nozzle_diameter = temp_model["nozzle_diameter"];
                std::string selected;
                boost::trim(nozzle_diameter);
                std::string nozzle;
                bool enabled = false, first=true;
                while (nozzle_diameter.size() > 0) {
                    auto pos = nozzle_diameter.find(';');
                    if (pos != std::string::npos) {
                        nozzle   = nozzle_diameter.substr(0, pos);
                        enabled = m_appconfig_new->get_variant(vendor_name, model_name, nozzle);
                        if (enabled) {
                            if (!first)
                                selected += ";";
                            selected += nozzle;
                            first = false;
                        }
                        nozzle_diameter = nozzle_diameter.substr(pos + 1);
                        boost::trim(nozzle_diameter);
                    }
                    else {
                        enabled = m_appconfig_new->get_variant(vendor_name, model_name, nozzle_diameter);
                        if (enabled) {
                            if (!first)
                                selected += ";";
                            selected += nozzle_diameter;
                        }
                        break;
                    }
                }
                temp_model["nozzle_selected"] = selected;
                //m_ProfileJson["model"][a]["nozzle_selected"]
            }
        }

        for (auto it = m_ProfileJson["filament"].begin(); it != m_ProfileJson["filament"].end(); ++it) {
            //json temp_filament = it.value();
            std::string filament_name = it.key();
            if (enabled_filaments.find(filament_name) != enabled_filaments.end())
                m_ProfileJson["filament"][filament_name]["selected"] = 1;
        }

        //----region
        m_Region = AppAdapter::app_config()->get("region");
        m_ProfileJson["region"] = m_Region;

        m_ProfileJson["network_plugin_install"] = AppAdapter::app_config()->get("app","installed_networking");
        m_ProfileJson["network_plugin_compability"] = "0";
        network_plugin_ready = false;

        StealthMode = AppAdapter::app_config()->get_bool("app","stealth_mode");
        m_ProfileJson["stealth_mode"] = StealthMode;
    }
    catch (std::exception &e) {
        BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ", error: "<< e.what() <<std::endl;
    }

    std::string strAll = m_ProfileJson.dump(-1, ' ', false, json::error_handler_t::ignore);

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ", finished, json contents: "<< std::endl<<strAll;
    return 0;
}

}} // namespace Slic3r::GUI
