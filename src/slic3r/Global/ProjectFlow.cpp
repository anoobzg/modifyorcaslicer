#include "ProjectFlow.hpp"

#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/I18N.hpp"

#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/MainPanel.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"

#include "slic3r/GUI/Event/UserPlaterEvent.hpp"

namespace Slic3r {
namespace GUI {

void queue_download_project_event(const std::string& project_id)
{
    wxCommandEvent* event = new wxCommandEvent(EVT_DOWNLOAD_PROJECT);
    event->SetString(project_id);
    wxQueueEvent(AppAdapter::plater(), event);    
}

void request_project_download(const std::string& project_id)
{
    queue_download_project_event(project_id);
}

void request_open_project(const std::string& project_id)
{
    Plater* p = AppAdapter::plater();
    if (p->is_background_process_slicing()) {
        Slic3r::GUI::show_info(nullptr, _L("new or open project file is not allowed during the slicing process!"), _L("Open Project"));
        return;
    }

    if (project_id == "<new>")
        p->new_project();
    else if (project_id.empty())
        p->load_project();
    else if (std::find_if_not(project_id.begin(), project_id.end(),
        [](char c) { return std::isdigit(c); }) == project_id.end())
        ;
    else if (boost::algorithm::starts_with(project_id, "http"))
        ;
    else
        AppAdapter::app()->CallAfter([project_id] {
            AppAdapter::main_panel()->open_recent_project(-1, wxString::FromUTF8(project_id)); 
        });
}

void request_remove_project(const std::string& project_id)
{
    AppAdapter::main_panel()->remove_recent_project(-1, wxString::FromUTF8(project_id));
}

void queue_download_model_event(const wxString& url)
{
    wxCommandEvent* event = new wxCommandEvent(EVT_IMPORT_MODEL_ID);
    event->SetString(url);
    wxQueueEvent(AppAdapter::plater(), event);
}

void request_model_download(const wxString& url)
{
    queue_download_model_event(url);
}

void plater_select_view(const std::string& direction)
{
    AppAdapter::plater()->get_camera().select_view(direction);
}

}
}