#include "GUI_Init.hpp"

#include "slic3r/Config/AppConfig.hpp"
#include "slic3r/Global/InstanceCheck.hpp"

#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/format.hpp"
#include "slic3r/GUI/I18N.hpp"
// To show a message box if GUI initialization ends up with an exception thrown.
#include <wx/msgdlg.h>

#include <boost/nowide/iostream.hpp>
#include <boost/nowide/convert.hpp>

#if __APPLE__
    #include <signal.h>
#endif // __APPLE__

#include <boost/nowide/fstream.hpp>
#include <boost/nowide/integration/filesystem.hpp>
 #include <boost/dll/runtime_symbol_info.hpp>


#include "libslic3r/Base/Thread.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/FileSystem/DataDir.hpp"
#include "libslic3r/Base/BlacklistedLibraryCheck.hpp"

//BBS: add exception handler for win32
#include <wx/stdpaths.h>
#include <wx/filename.h>

#ifdef WIN32
#include "slic3r/GUI/BaseException.h"
#endif


namespace Slic3r {
namespace GUI {

static PrinterTechnology get_printer_technology(const DynamicConfig& config)
{
    const ConfigOptionEnum<PrinterTechnology>* opt = config.option<ConfigOptionEnum<PrinterTechnology>>("printer_technology");
    return (opt == nullptr) ? ptUnknown : opt->value;
}

int CLI::run(const boost::filesystem::path& resource_path)
{
    // Mark the main thread for the debugger and for runtime checks.
    set_current_thread_name("orcaslicer_main");
    // Save the thread ID of the main thread.
    save_main_thread_id();

    if (!this->setup(resource_path))
    {
        boost::nowide::cerr << "setup params error" << std::endl;
        return CLI_INVALID_PARAMS;
    }

    std::string temp_path = wxFileName::GetTempDir().utf8_str().data();
    set_temporary_dir(temp_path);

    return 1;
}

bool CLI::setup(const boost::filesystem::path& resource_path)
{
#ifdef WIN32
    // Notify user that a blacklisted DLL was injected into process (for example Nahimic, see GH #5573).
    // We hope that if a DLL is being injected into a process, it happens at the very start of the application,
    // thus we shall detect them now.
    if (BlacklistedLibraryCheck::get_instance().perform_check()) {
        std::wstring text = L"Following DLLs have been injected into the LightMaker process:\n\n";
        text += BlacklistedLibraryCheck::get_instance().get_blacklisted_string();
        text += L"\n\n"
            L"LightMaker is known to not run correctly with these DLLs injected. "
            L"We suggest stopping or uninstalling these services if you experience "
            L"crashes or unexpected behaviour while using LightMaker.\n"
            L"For example, ASUS Sonic Studio injects a Nahimic driver, which makes LightMaker "
            L"to crash on a secondary monitor";
        MessageBoxW(NULL, text.c_str(), L"Warning"/*L"Incopatible library found"*/, MB_OK);
    }
#endif

    //FIXME Validating at this stage most likely does not make sense, as the config is not fully initialized yet.
    std::map<std::string, std::string> validity = m_config.validate(true);

    // Initialize with defaults.
    for (const t_optiondef_map* options : { &cli_actions_config_def.options, &cli_transform_config_def.options, &cli_misc_config_def.options })
        for (const t_optiondef_map::value_type& optdef : *options)
            m_config.option(optdef.first, true);

    //FIXME Validating at this stage most likely does not make sense, as the config is not fully initialized yet.
    if (!validity.empty()) {
        boost::nowide::cerr << "Params in command line error: " << std::endl;
        for (std::map<std::string, std::string>::iterator it = validity.begin(); it != validity.end(); ++it)
            boost::nowide::cerr << it->first << ": " << it->second << std::endl;
        return false;
    }

    // file system initialization
    // See Invoking prusa-slicer from $PATH environment variable crashes #5542
    // boost::filesystem::path path_to_binary = boost::filesystem::system_complete(argv[0]);
    boost::filesystem::path path_to_binary = boost::dll::program_location();

    // Path from the Slic3r binary to its resources.
#ifdef __APPLE__
    // The application is packed in the .dmg archive as 'Slic3r.app/Contents/MacOS/Slic3r'
    // The resources are packed to 'Slic3r.app/Contents/Resources'
    boost::filesystem::path path_resources = boost::filesystem::canonical(path_to_binary).parent_path().parent_path() / "Resources";
#elif defined _WIN32
    // The application is packed in the .zip archive in the root,
    // The resources are packed to 'resources'
    // Path from Slic3r binary to resources:
    boost::filesystem::path path_resources = path_to_binary.parent_path() / "resources";
#elif defined SLIC3R_FHS
    // The application is packaged according to the Linux Filesystem Hierarchy Standard
    // Resources are set to the 'Architecture-independent (shared) data', typically /usr/share or /usr/local/share
    boost::filesystem::path path_resources = SLIC3R_FHS_RESOURCES;
#else
    // The application is packed in the .tar.bz archive (or in AppImage) as 'bin/slic3r',
    // The resources are packed to 'resources'
    // Path from Slic3r binary to resources:
    boost::filesystem::path path_resources = boost::filesystem::canonical(path_to_binary).parent_path().parent_path() / "resources";
#endif

    if(resource_path.string() != "")
        path_resources = resource_path;

    set_resources_dir(path_resources.string());
    set_var_dir((path_resources / "images").string());
    set_local_dir((path_resources / "i18n").string());
    set_sys_shapes_dir((path_resources / "shapes").string());
    set_custom_gcodes_dir((path_resources / "custom_gcodes").string());
    set_user_data_dir(m_config.opt_string("datadir"));

    // Set the Slic3r data directory at the Slic3r XS module.
    // Unix: ~/ .Slic3r
    // Windows : "C:\Users\username\AppData\Roaming\Slic3r" or "C:\Documents and Settings\username\Application Data\Slic3r"
    // Mac : "~/Library/Application Support/Slic3r"

    if (user_data_dir().empty()) {
        // Orca: check if data_dir folder exists in application folder use it if it exists
        // Note:wxStandardPaths::Get().GetExecutablePath() return following paths
        // Unix: /usr/local/bin/exename
        // Windows: "C:\Programs\AppFolder\exename.exe"
        // Mac: /Applications/exename.app/Contents/MacOS/exename
        // TODO: have no idea what to do with Linux bundles
        auto _app_folder = path_to_binary.parent_path();
#ifdef __APPLE__
        // On macOS, the executable is inside the .app bundle.
        _app_folder = _app_folder.parent_path().parent_path().parent_path();
#endif
        boost::filesystem::path app_data_dir_path = _app_folder / "data_dir";
        if (boost::filesystem::exists(app_data_dir_path)) {
            set_user_data_dir(app_data_dir_path.string());
        }
        else {
            boost::filesystem::path data_dir_path(os_user_data_dir(SLIC3R_APP_KEY));

            if (!boost::filesystem::exists(data_dir_path)) {
                boost::filesystem::create_directory(data_dir_path);
            }
            set_user_data_dir(data_dir_path.string());
        }

        // Change current dirtory of application
        chdir(encode_path((Slic3r::user_data_dir() + "/log").c_str()).c_str());
    }

    return true;
}

int GUI_Init()
{
#if __APPLE__
    // On OSX, we use boost::process::spawn() to launch new instances of PrusaSlicer from another PrusaSlicer.
    // boost::process::spawn() sets SIGCHLD to SIGIGN for the child process, thus if a child PrusaSlicer spawns another
    // subprocess and the subrocess dies, the child PrusaSlicer will not receive information on end of subprocess
    // (posix waitpid() call will always fail).
    // https://jmmv.dev/2008/10/boostprocess-and-sigchld.html
    // The child instance of PrusaSlicer has to reset SIGCHLD to its default, so that posix waitpid() and similar continue to work.
    // See GH issue #5507
    signal(SIGCHLD, SIG_DFL);
#endif // __APPLE__

    //BBS: remove the try-catch and let exception goto above
    try {
        GUI::AppAdapter* adapter = AppAdapter::inst();
        
        // G-code viewer is currently not performing instance check, a new G-code viewer is started every time.
        bool gui_single_instance_setting = app_get_bool("single_instance");
        std::pair<int, char**> arg_pair = adapter->m_context->run_params();
        if (Slic3r::instance_check(arg_pair.first, arg_pair.second, gui_single_instance_setting)) {
            //TODO: do we have delete gui and other stuff?
            return -1;
        }

        wxApp::SetInstance(adapter->app());

        return 0;
    } catch (const Slic3r::Exception &ex) {
        BOOST_LOG_TRIVIAL(error) << ex.what() << std::endl;
        wxMessageBox(boost::nowide::widen(ex.what()), _L("GUI initialization failed"), wxICON_STOP);
    } catch (const std::exception &ex) {
        BOOST_LOG_TRIVIAL(error) << ex.what() << std::endl;
        wxMessageBox(format_wxstr(_L("Fatal error, exception caught: %1%"), ex.what()), _L("GUI initialization failed"), wxICON_STOP);
    }
    // error
    return -1;
}
}
}
