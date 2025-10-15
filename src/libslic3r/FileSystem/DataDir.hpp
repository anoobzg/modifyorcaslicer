#ifndef SLIC3R_FileSystem_DataDir_hpp_
#define SLIC3R_FileSystem_DataDir_hpp_

namespace Slic3r {
std::string os_user_data_dir(const char* folder_name = nullptr); // Return the OS user data dir, for example: /home/user/.local/share/PrusaSlicer

// set user data dir, should be called before app
void set_user_data_dir(const std::string &path);
// Return a full path to the user data dir.
const std::string& user_data_dir();

//BBS: add temp dir
void set_temporary_dir(const std::string &path);
const std::string& temporary_dir();

// Set a path with GUI resource files.
void set_var_dir(const std::string &path);
// Return a full path to the GUI resource files.
const std::string& var_dir();
// Return a full resource path for a file_name.
std::string var(const std::string &file_name);

// Set a path with various static definition data (for example the initial config bundles).
void set_resources_dir(const std::string &path);
// Return a full path to the resources directory.
const std::string& resources_dir();

// Set a path with GUI localization files.
void set_local_dir(const std::string &path);
// Return a full path to the localization directory.
const std::string& localization_dir();

// Set a path with shapes gallery files.
void set_sys_shapes_dir(const std::string &path);
// Return a full path to the system shapes gallery directory.
const std::string& sys_shapes_dir();

// Return a full path to the custom shapes gallery directory.
std::string custom_shapes_dir();

// Set a path with shapes gallery files.
void set_custom_gcodes_dir(const std::string &path);
// Return a full path to the system shapes gallery directory.
const std::string& custom_gcodes_dir();
}

#endif // SLIC3R_FileSystem_DataDir_hpp_