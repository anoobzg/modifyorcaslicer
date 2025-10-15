#include "AppWx.hpp"

#include "libslic3r/FileSystem/Log.hpp"
#include "libslic3r/FileSystem/FileHelp.hpp"
#include "libslic3r/FileSystem/DataDir.hpp"

#include "slic3r/Config/AppConfig.hpp"
#include "slic3r/Global/AppI18N.hpp"
#include "slic3r/Global/AppModule.hpp"
#include "slic3r/Global/InstanceCheck.hpp"

#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/format.hpp"
#include "slic3r/GUI/wxExtensions.hpp"
#include "slic3r/GUI/GUI.hpp"

#include "slic3r/GUI/Event/UserPlaterEvent.hpp"
#ifdef WIN32
#include <Dbt.h>
#include <shlobj_core.h>
#endif


#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/MainPanel.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"
#include "slic3r/Render/Mouse3DController.hpp"


namespace Slic3r {
namespace GUI {

void generic_exception_handle()
{
    // Note: Some wxWidgets APIs use wxLogError() to report errors, eg. wxImage
    // - see https://docs.wxwidgets.org/3.1/classwx_image.html#aa249e657259fe6518d68a5208b9043d0
    //
    // wxLogError typically goes around exception handling and display an error dialog some time
    // after an error is logged even if exception handling and OnExceptionInMainLoop() take place.
    // This is why we use wxLogError() here as well instead of a custom dialog, because it accumulates
    // errors if multiple have been collected and displays just one error message for all of them.
    // Otherwise we would get multiple error messages for one missing png, for example.
    //
    // If a custom error message window (or some other solution) were to be used, it would be necessary
    // to turn off wxLogError() usage in wx APIs, most notably in wxImage
    // - see https://docs.wxwidgets.org/trunk/classwx_image.html#aa32e5d3507cc0f8c3330135bc0befc6a
/*#ifdef WIN32
    //LPEXCEPTION_POINTERS exception_pointers = nullptr;
    __try {
        throw;
    }
    __except (CBaseException::UnhandledExceptionFilter2(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) {
    //__except (exception_pointers = GetExceptionInformation(), EXCEPTION_EXECUTE_HANDLER) {
    //    if (exception_pointers) {
    //        CBaseException::UnhandledExceptionFilter(exception_pointers);
    //    }
    //    else
            throw;
    }
#else*/
    try {
        throw;
    } catch (const std::bad_alloc& ex) {
        // bad_alloc in main thread is most likely fatal. Report immediately to the user (wxLogError would be delayed)
        // and terminate the app so it is at least certain to happen now.
        BOOST_LOG_TRIVIAL(error) << boost::format("std::bad_alloc exception: %1%") % ex.what();
        flush_logs();
        wxString errmsg = wxString::Format(_L("Program will terminate because of running out of memory."
                                              "It may be a bug. It will be appreciated if you report the issue to our team."));
        wxMessageBox(errmsg + "\n\n" + wxString(ex.what()), _L("Fatal error"), wxOK | wxICON_ERROR);

        std::terminate();
        //throw;
     } catch (const boost::io::bad_format_string& ex) {
     	BOOST_LOG_TRIVIAL(error) << boost::format("Uncaught exception: %1%") % ex.what();
        	flush_logs();
        wxString errmsg = _L("Program will terminate because of a localization error. "
                             "It will be appreciated if you report the specific scenario this issue happened.");
        wxMessageBox(errmsg + "\n\n" + wxString(ex.what()), _L("Critical error"), wxOK | wxICON_ERROR);
        std::terminate();
        //throw;
    } catch (const std::exception& ex) {
        wxLogError(format_wxstr(_L("Program got an unhandled exception: %1%"), ex.what()));
        BOOST_LOG_TRIVIAL(error) << boost::format("Uncaught exception: %1%") % ex.what();
        flush_logs();
        throw;
    }
//#endif
}

std::string libslic3r_translate_callback(const char *s)
{ 
    return wxGetTranslation(wxString(s, wxConvUTF8)).utf8_str().data(); 
}

#ifdef __linux__
bool static check_old_linux_datadir(const wxString& app_name) {
    // If we are on Linux and the datadir does not exist yet, look into the old
    // location where the datadir was before version 2.3. If we find it there,
    // tell the user that he might wanna migrate to the new location.
    // (https://github.com/prusa3d/PrusaSlicer/issues/2911)
    // To be precise, the datadir should exist, it is created when single instance
    // lock happens. Instead of checking for existence, check the contents.

    namespace fs = boost::filesystem;

    std::string new_path = Slic3r::user_data_dir();

    wxString dir;
    if (! wxGetEnv(wxS("XDG_CONFIG_HOME"), &dir) || dir.empty() )
        dir = wxFileName::GetHomeDir() + wxS("/.config");
    std::string default_path = (dir + "/" + app_name).ToUTF8().data();

    if (new_path != default_path) {
        // This happens when the user specifies a custom --datadir.
        // Do not show anything in that case.
        return true;
    }

    fs::path data_dir = fs::path(new_path);
    if (! fs::is_directory(data_dir))
        return true; // This should not happen.

    int file_count = std::distance(fs::directory_iterator(data_dir), fs::directory_iterator());

    if (file_count <= 1) { // just cache dir with an instance lock
        // BBS
    } else {
        // If the new directory exists, be silent. The user likely already saw the message.
    }
    return true;
}
#endif

#ifdef _WIN32
bool is_associate_files(std::wstring extend)
{
    wchar_t app_path[MAX_PATH];
    ::GetModuleFileNameW(nullptr, app_path, sizeof(app_path));

    std::wstring prog_id             = L" Orca.Slicer.1";
    std::wstring reg_base            = L"Software\\Classes";
    std::wstring reg_extension       = reg_base + L"\\." + extend;

    wchar_t szValueCurrent[1000];
    DWORD   dwType;
    DWORD   dwSize = sizeof(szValueCurrent);

    int iRC = ::RegGetValueW(HKEY_CURRENT_USER, reg_extension.c_str(), nullptr, RRF_RT_ANY, &dwType, szValueCurrent, &dwSize);

    bool bDidntExist = iRC == ERROR_FILE_NOT_FOUND;

    if (!bDidntExist && ::wcscmp(szValueCurrent, prog_id.c_str()) == 0)
        return true;

    return false;
}
#endif


#ifdef WIN32
#if !wxVERSION_EQUAL_OR_GREATER_THAN(3,1,3)
static void register_win32_dpi_event()
{
    enum { WM_DPICHANGED_ = 0x02e0 };

    wxWindow::MSWRegisterMessageHandler(WM_DPICHANGED_, [](wxWindow* win, WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam) {
        const int dpi = wParam & 0xffff;
        const auto rect = reinterpret_cast<PRECT>(lParam);
        const wxRect wxrect(wxPoint(rect->top, rect->left), wxPoint(rect->bottom, rect->right));

        DpiChangedEvent evt(EVT_DPI_CHANGED_SLICER, dpi, wxrect);
        win->GetEventHandler()->AddPendingEvent(evt);

        return true;
        });
}
#endif // !wxVERSION_EQUAL_OR_GREATER_THAN

static GUID GUID_DEVINTERFACE_HID = { 0x4D1E55B2, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 };

static void register_win32_device_notification_event()
{
    wxWindow::MSWRegisterMessageHandler(WM_DEVICECHANGE, [](wxWindow* win, WXUINT /* nMsg */, WXWPARAM wParam, WXLPARAM lParam) {
        // Some messages are sent to top level windows by default, some messages are sent to only registered windows, and we explictely register on MainFrame only.
        auto plater = AppAdapter::plater();
        if (plater == nullptr)
            // Maybe some other top level window like a dialog or maybe a pop-up menu?
            return true;
        PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR)lParam;
        switch (wParam) {
        case DBT_DEVICEARRIVAL:
            if (lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME)
                plater->GetEventHandler()->AddPendingEvent(VolumeAttachedEvent(EVT_VOLUME_ATTACHED));
            else if (lpdb->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                PDEV_BROADCAST_DEVICEINTERFACE lpdbi = (PDEV_BROADCAST_DEVICEINTERFACE)lpdb;
                //				if (lpdbi->dbcc_classguid == GUID_DEVINTERFACE_VOLUME) {
                //					printf("DBT_DEVICEARRIVAL %d - Media has arrived: %ws\n", msg_count, lpdbi->dbcc_name);
                if (lpdbi->dbcc_classguid == GUID_DEVINTERFACE_HID)
                    plater->GetEventHandler()->AddPendingEvent(HIDDeviceAttachedEvent(EVT_HID_DEVICE_ATTACHED, boost::nowide::narrow(lpdbi->dbcc_name)));
            }
            break;
        case DBT_DEVICEREMOVECOMPLETE:
            if (lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME)
                plater->GetEventHandler()->AddPendingEvent(VolumeDetachedEvent(EVT_VOLUME_DETACHED));
            else if (lpdb->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                PDEV_BROADCAST_DEVICEINTERFACE lpdbi = (PDEV_BROADCAST_DEVICEINTERFACE)lpdb;
                //				if (lpdbi->dbcc_classguid == GUID_DEVINTERFACE_VOLUME)
                //					printf("DBT_DEVICEARRIVAL %d - Media was removed: %ws\n", msg_count, lpdbi->dbcc_name);
                if (lpdbi->dbcc_classguid == GUID_DEVINTERFACE_HID)
                    plater->GetEventHandler()->AddPendingEvent(HIDDeviceDetachedEvent(EVT_HID_DEVICE_DETACHED, boost::nowide::narrow(lpdbi->dbcc_name)));
            }
            break;
        default:
            break;
        }
        return true;
        });

    wxWindow::MSWRegisterMessageHandler(WM_INPUT, [](wxWindow* win, WXUINT /* nMsg */, WXWPARAM wParam, WXLPARAM lParam) {
        auto main_frame = dynamic_cast<MainFrame*>(Slic3r::GUI::find_toplevel_parent(win));
        auto plater = (main_frame == nullptr) ? nullptr : AppAdapter::plater();
        //        if (wParam == RIM_INPUTSINK && plater != nullptr && main_frame->IsActive()) {
        if (wParam == RIM_INPUT && plater != nullptr && main_frame->IsActive()) {
            RAWINPUT raw;
            UINT rawSize = sizeof(RAWINPUT);
            ::GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &rawSize, sizeof(RAWINPUTHEADER));
            if (raw.header.dwType == RIM_TYPEHID && plater->get_mouse3d_controller().handle_raw_input_win32(raw.data.hid.bRawData, raw.data.hid.dwSizeHid))
                return true;
        }
        return false;
        });

    wxWindow::MSWRegisterMessageHandler(WM_COPYDATA, [](wxWindow* win, WXUINT /* nMsg */, WXWPARAM wParam, WXLPARAM lParam) {
        COPYDATASTRUCT* copy_data_structure = { 0 };
        copy_data_structure = (COPYDATASTRUCT*)lParam;
        if (copy_data_structure->dwData == 1) {
            LPCWSTR arguments = (LPCWSTR)copy_data_structure->lpData;
            other_instance_message_handler()->handle_message(boost::nowide::narrow(arguments));
        }
        return true;
        });
}
#endif // WIN32

void register_win32_event()
{
#ifdef WIN32
#if !wxVERSION_EQUAL_OR_GREATER_THAN(3,1,3)
    register_win32_dpi_event();
#endif // !wxVERSION_EQUAL_OR_GREATER_THAN
    register_win32_device_notification_event();
#endif // WIN32
}

bool has_illegal_filename_characters(const wxString& wxs_name)
{
    std::string name = into_u8(wxs_name);
    return has_illegal_filename_characters(name);
}

bool has_illegal_filename_characters(const std::string& name)
{
    const char* illegal_characters = "<>:/\\|?*\"";
    for (size_t i = 0; i < std::strlen(illegal_characters); i++)
        if (name.find_first_of(illegal_characters[i]) != std::string::npos)
            return true;

    return false;
}

std::string w2s(wxString sSrc)
{
    return std::string(sSrc.mb_str());
}

void GetStardardFilePath(std::string &FilePath) {
    Slic3r::Utils::StrReplace(FilePath, "\\", w2s(wxString::Format("%c", boost::filesystem::path::preferred_separator)));
    Slic3r::Utils::StrReplace(FilePath, "/" , w2s(wxString::Format("%c", boost::filesystem::path::preferred_separator)));
}

void run_printer_model_wizard()
{
    AppAdapter::app()->CallAfter([]() { AppAdapter::gui_app()->run_wizard(ConfigWizard::RR_USER, ConfigWizard::SP_PRINTERS); });
}

bool open_browser_with_warning_dialog(const wxString& url, int flags/* = 0*/)
{
    return wxLaunchDefaultBrowser(url, flags);
}

void desktop_open_datadir_folder()
{
	// Execute command to open a file explorer, platform dependent.
	// FIXME: The const_casts aren't needed in wxWidgets 3.1, remove them when we upgrade.

	const auto path = user_data_dir();
#ifdef _WIN32
		const wxString widepath = from_u8(path);
		const wchar_t *argv[] = { L"explorer", widepath.GetData(), nullptr };
		::wxExecute(const_cast<wchar_t**>(argv), wxEXEC_ASYNC, nullptr);
#elif __APPLE__
		const char *argv[] = { "open", path.data(), nullptr };
		::wxExecute(const_cast<char**>(argv), wxEXEC_ASYNC, nullptr);
#else
		const char *argv[] = { "xdg-open", path.data(), nullptr };

		// Check if we're running in an AppImage container, if so, we need to remove AppImage's env vars,
		// because they may mess up the environment expected by the file manager.
		// Mostly this is about LD_LIBRARY_PATH, but we remove a few more too for good measure.
		if (wxGetEnv("APPIMAGE", nullptr)) {
			// We're running from AppImage
			wxEnvVariableHashMap env_vars;
			wxGetEnvMap(&env_vars);

			env_vars.erase("APPIMAGE");
			env_vars.erase("APPDIR");
			env_vars.erase("LD_LIBRARY_PATH");
			env_vars.erase("LD_PRELOAD");
			env_vars.erase("UNION_PRELOAD");

			wxExecuteEnv exec_env;
			exec_env.env = std::move(env_vars);

			wxString owd;
			if (wxGetEnv("OWD", &owd)) {
				// This is the original work directory from which the AppImage image was run,
				// set it as CWD for the child process:
				exec_env.cwd = std::move(owd);
			}

			::wxExecute(const_cast<char**>(argv), wxEXEC_ASYNC, nullptr, &exec_env);
		} else {
			// Looks like we're NOT running from AppImage, we'll make no changes to the environment.
			::wxExecute(const_cast<char**>(argv), wxEXEC_ASYNC, nullptr, nullptr);
		}
#endif
}

void desktop_open_any_folder( const std::string& path )
{
    // Execute command to open a file explorer, platform dependent.
    // FIXME: The const_casts aren't needed in wxWidgets 3.1, remove them when we upgrade.

#ifdef _WIN32
    const wxString widepath = from_u8(path);
    ::wxExecute(L"explorer /select," + widepath, wxEXEC_ASYNC, nullptr);
#elif __APPLE__
    openFolderForFile(from_u8(path));
#else

    // Orca#6449: Open containing dir instead of opening the file directly.
    std::string new_path = path;
    boost::filesystem::path p(new_path);
    if (!fs::is_directory(p)) {
        new_path = p.parent_path().string();
    }
    const char* argv[] = {"xdg-open", new_path.data(), nullptr};

    // Check if we're running in an AppImage container, if so, we need to remove AppImage's env vars,
    // because they may mess up the environment expected by the file manager.
    // Mostly this is about LD_LIBRARY_PATH, but we remove a few more too for good measure.
    if (wxGetEnv("APPIMAGE", nullptr)) {
        // We're running from AppImage
        wxEnvVariableHashMap env_vars;
        wxGetEnvMap(&env_vars);

        env_vars.erase("APPIMAGE");
        env_vars.erase("APPDIR");
        env_vars.erase("LD_LIBRARY_PATH");
        env_vars.erase("LD_PRELOAD");
        env_vars.erase("UNION_PRELOAD");

        wxExecuteEnv exec_env;
        exec_env.env = std::move(env_vars);

        wxString owd;
        if (wxGetEnv("OWD", &owd)) {
            // This is the original work directory from which the AppImage image was run,
            // set it as CWD for the child process:
            exec_env.cwd = std::move(owd);
        }

        ::wxExecute(const_cast<char **>(argv), wxEXEC_ASYNC, nullptr, &exec_env);
    } else {
        // Looks like we're NOT running from AppImage, we'll make no changes to the environment.
        ::wxExecute(const_cast<char **>(argv), wxEXEC_ASYNC, nullptr, nullptr);
    }
#endif
}

int get_brightness_value(wxImage image) {

    wxImage grayImage = image.ConvertToGreyscale();

    int width = grayImage.GetWidth();
    int height = grayImage.GetHeight();

    int totalLuminance = 0;
    unsigned char alpha;
    int num_none_transparent = 0;
    for (int y = 0; y < height; y += 2) {

        for (int x = 0; x < width; x += 2) {

            alpha = image.GetAlpha(x, y);
            if (alpha != 0) {
                wxColour pixelColor = grayImage.GetRed(x, y);
                totalLuminance += pixelColor.Red();
                num_none_transparent = num_none_transparent + 1;
            }
        }
    }
    if (totalLuminance <= 0 || num_none_transparent <= 0) {
        return 0;
    }
    return totalLuminance / num_none_transparent;
}

wxString choose_project_name(wxWindow *parent)
{
    wxString file_name;
    wxFileDialog dialog(parent ? parent : app_top_window(),
        _L("Choose one file (3mf):"),
        app_get_last_dir(), "",
        file_wildcards(FT_PROJECT), wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dialog.ShowModal() == wxID_OK)
        file_name = dialog.GetPath();
    
    return file_name;
}

wxArrayString choose_model_name(wxWindow *parent)
{
    wxArrayString input_files;
    wxFileDialog dialog(parent ? parent : app_top_window(),
#ifdef __APPLE__
        _L("Choose one or more files (3mf/step/stl/svg/obj/amf/usd*/abc/ply):"),
#else
        _L("Choose one or more files (3mf/step/stl/svg/obj/amf):"),
#endif
        from_u8(app_get_last_dir()), "",
        file_wildcards(FT_MODEL), wxFD_OPEN | wxFD_MULTIPLE | wxFD_FILE_MUST_EXIST);

    if (dialog.ShowModal() == wxID_OK)
        dialog.GetPaths(input_files);

    return input_files;
}

wxString choose_zip_name(wxWindow* parent)
{
    wxString input_file;
    wxFileDialog dialog(parent ? parent : app_top_window(),
                        _L("Choose ZIP file") + ":",
                        from_u8(app_get_last_dir()), "",
                        file_wildcards(FT_ZIP), wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dialog.ShowModal() == wxID_OK)
        input_file = dialog.GetPath();

    return input_file;
}

wxString choose_gcode_name(wxWindow* parent)
{
    wxString input_file;
    wxFileDialog dialog(parent ? parent : app_top_window(),
        _L("Choose one file (gcode/3mf):"),
        app_get_last_dir(), "",
        file_wildcards(FT_GCODE), wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dialog.ShowModal() == wxID_OK)
        input_file = dialog.GetPath();

    return input_file;
}

wxString transition_tridid(int trid_id)
{
    wxString maping_dict[8] = { "A", "B", "C", "D", "E", "F", "G" };
    int id_index = ceil(trid_id / 4);
    int id_suffix = (trid_id + 1) % 4 == 0 ? 4 : (trid_id + 1) % 4;
    return wxString::Format("%s%d", maping_dict[id_index], id_suffix);
}

const char* separator_head()
{
#ifdef __linux__
    return "------- ";
#else // __linux__ 
    return "------ ";
#endif // __linux__
}

const char* separator_tail()
{
#ifdef __linux__
    return " -------";
#else // __linux__ 
    return " ------";
#endif // __linux__
}

wxString separator(const std::string& label)
{
    return wxString::FromUTF8(separator_head()) + _(label) + wxString::FromUTF8(separator_tail());
}

}
}