#pragma once 
#include <string>

namespace Slic3r {
namespace GUI {

// extend is stl/3mf/gcode/step etc 
void            associate_files(std::wstring extend);
void            disassociate_files(std::wstring extend);
bool            check_url_association(std::wstring url_prefix, std::wstring& reg_bin);
void            associate_url(std::wstring url_prefix);
void            disassociate_url(std::wstring url_prefix);

#if ENABLE_THUMBNAIL_GENERATOR_DEBUG
    // temporary and debug only -> extract thumbnails from selected gcode and save them as png files
    void            gcode_thumbnails_debug();
#endif // ENABLE_THUMBNAIL_GENERATOR_DEBUG

#ifdef __APPLE__
    void            OSXStoreOpenFiles(const wxArrayString &files);
    // wxWidgets override to get an event on open files.
    void            MacOpenFiles(const wxArrayString &fileNames) override;
    void            MacOpenURL(const wxString& url) override;
#endif /* __APPLE */
}
}