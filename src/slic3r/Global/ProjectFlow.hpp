#pragma once 

namespace Slic3r {
namespace GUI {
    void queue_download_project_event(const std::string& project_id);

    void request_project_download(const std::string& project_id);
    void request_open_project(const std::string& project_id);
    void request_remove_project(const std::string& project_id);

    void queue_download_model_event(const wxString& url);
    void request_model_download(const wxString& url);

    void plater_select_view(const std::string& direction);
}
}