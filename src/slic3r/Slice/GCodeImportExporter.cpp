#include "GCodeImportExporter.hpp"

#include "libslic3r/FileSystem/ASCIIFolding.hpp"
#include "libslic3r/FileSystem/FileHelp.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/PresetBundle.hpp"

#include "slic3r/GUI/format.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/Utils/RemovableDriveManager.hpp"

#include "slic3r/Scene/PartPlate.hpp"
#include "slic3r/Scene/PartPlateList.hpp"
#include "slic3r/Slice/GCodeResultWrapper.hpp"
#include "slic3r/Config/AppPreset.hpp"
#include "slic3r/Utils/AppWx.hpp"
#include "slic3r/Global/AppModule.hpp"
#include "slic3r/GUI/I18N.hpp"

namespace Slic3r {

namespace GUI {

GCodeImportExporter::GCodeImportExporter(PartPlateList* part_plate_list) :
	m_part_plate_list(part_plate_list)
{

}

void GCodeImportExporter::import_plate_gcode_files(int plate_id, const PlateGCodeFile& gcode_file)
{
	if (!m_part_plate_list ||
		plate_id < 0 || 
		plate_id >= m_part_plate_list->get_plate_count())
		return;
	
    PartPlate* plate = m_part_plate_list->get_plate(plate_id);
	GCodeResultWrapper* gcode_result_wrapper = plate->get_slice_result_wrapper();
	// //BBS: add the logic to process from an existed gcode file
	// if (m_print->finished()) {
	// 	BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: skip slicing, to process previous gcode file")%__LINE__;
	// 	m_fff_print->set_status(80, _utf8(L("Processing G-Code from Previous file...")));
	// 	wxCommandEvent evt(EVT_SLICING_COMPLETED);
	// 	// Post the Slicing Finished message for the G-code viewer to update.
	// 	// Passing the timestamp 
	// 	evt.SetInt((int)(m_fff_print->step_state_with_timestamp(PrintStep::psSlicingFinished).timestamp));
	// 	wxQueueEvent(GUI::AppAdapter::main_panel()->m_plater, evt.Clone());

		// m_temp_output_path;
		// if (! m_export_path.empty()) {
		// 	BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: export gcode from %2% directly to %3%")%__LINE__%m_temp_output_path %m_export_path;
		// }
		// else {
        //     if (m_upload_job.empty()) {

	// if (!gcode_file.is_area_gcode)
	// {
	// 	print->export_gcode_from_previous_file(gcode_file.file, gcode_result_wrapper->get_result());
	// }
	// else 
	// {
	// 	gcode_result_wrapper->resize(gcode_file.areas.size());
	// 	for (int i = 0, count = gcode_file.areas.size(); i < count; ++i)
	// 	{
	// 		print->export_gcode_from_previous_file(gcode_file.areas[i], gcode_result_wrapper->get_result(i));
	// 	}
	// }
	

    //         }
    //         BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: export_gcode_from_previous_file from %2% finished")%__LINE__ % m_temp_output_path;
	// 	}
	// }

}

void GCodeImportExporter::import_all_plate_gcode_files(const std::vector<PlateGCodeFile>& gcode_files)
{
	
}

ExportResult export_gcode_from_part_plate(PartPlate* part_plate, const GCodeExportParam& param)
{
    ExportResult result;

    do{
        if(!part_plate)
            break;

        if(param.export_3mf)
            break;

        bool prefer_removable = param.prefer_removable;
        Print* print = nullptr;
        GCodeResultWrapper* gcode_result = nullptr;
        part_plate->get_print(&gcode_result, NULL);

        fs::path default_output_file = "";
        std::vector<Print*> _prints = gcode_result->get_prints();
        
        // Check if external gcode (has filename set)
        std::string external_filename = gcode_result->filename();
        if (!external_filename.empty()) {
            // External gcode: use filename directly (avoid crash on empty m_model)
            default_output_file = fs::path(external_filename);
        } else {
            // Normal sliced gcode: get filename from print
            if (!_prints.empty()) {
                for (auto _print: _prints) {
                    default_output_file = _print->output_filepath("");
                    if (!default_output_file.empty())
                        break;
                }
            }
        }
        
        default_output_file = fs::path(Slic3r::fold_utf8_to_ascii(default_output_file.string()));

        AppConfig 				&appconfig 				 = *AppAdapter::app_config();
        RemovableDriveManager 	&removable_drive_manager = *AppAdapter::gui_app()->removable_drive_manager();
        std::string      		 start_dir 				 = appconfig.get_last_output_dir(default_output_file.parent_path().string(), prefer_removable);
        if (prefer_removable) {
            // Returns a path to a removable media if it exists, prefering start_dir. Update the internal removable drives database.
            start_dir = removable_drive_manager.get_removable_drive_path(start_dir);
            if (start_dir.empty())
                // Direct user to the last internal media.
                start_dir = appconfig.get_last_output_dir(default_output_file.parent_path().string(), false);
        }

        fs::path output_path;
        {
            std::string ext = default_output_file.extension().string();
            wxFileDialog dlg(app_plater_panel(), _L("Save G-code file as:"),
                start_dir,
                from_path(default_output_file.filename()),
                file_wildcards(FT_GCODE, ext),
                wxFD_SAVE | wxFD_OVERWRITE_PROMPT
            );
            if (dlg.ShowModal() == wxID_OK) {
                output_path = into_path(dlg.GetPath());
                while (has_illegal_filename_characters(output_path.filename().string())) {
                    show_error(app_plater_panel(), _L("The provided file name is not valid.") + "\n" +
                        _L("The following characters are not allowed by a FAT file system:") + " <>:/\\|?*\"");
                    dlg.SetFilename(from_path(output_path.filename()));
                    if (dlg.ShowModal() == wxID_OK)
                        output_path = into_path(dlg.GetPath());
                    else {
                        output_path.clear();
                        break;
                    }
                }
            }
        }

        if (! output_path.empty()) {
            result.last_output_path = output_path.string();
            result.last_output_dir_path = output_path.parent_path().string();

            bool path_on_removable_media = removable_drive_manager.set_and_verify_last_save_path(output_path.string());
            //bool path_on_removable_media = false;
            // p->notification_manager->new_export_began(path_on_removable_media);
            // p->exporting_status = path_on_removable_media ? ExportingStatus::EXPORTING_TO_REMOVABLE : ExportingStatus::EXPORTING_TO_LOCAL;
            // p->last_output_path = output_path.string();
            // p->last_output_dir_path = output_path.parent_path().string();

            if (! output_path.empty()) {
                // wxQueueEvent(GUI::AppAdapter::main_panel()->m_plater, new wxCommandEvent(EVT_EXPORT_BEGAN));

                std::string export_path = "";
                if (!_prints.empty())
                {
                    for (auto _print : _prints)
                    {
                        export_path = _print->print_statistics().finalize_output_path(output_path.string());
                        if (!export_path.empty())
                            break;
                    }
                }
                //std::string input_path;
                std::vector<std::string> input_paths = gcode_result->get_area_gcode_paths();
                std::vector<std::string> export_paths;
                export_paths.reserve(input_paths.size());
                if (!export_path.empty())
                    export_paths.push_back(export_path);
                if (input_paths.size() > 1)
                {
                    size_t pos = export_path.rfind(".gcode");
                    if (pos != std::string::npos) {
                        std::string result = export_path;
                        result.insert(pos, "_right");
                        export_paths.push_back(result);
                    }
                }

                std::string error_message;
                int copy_ret_val = CopyFileResult::SUCCESS;
                try
                {
                    for (size_t i = 0; i < input_paths.size(); i++)
                    {
                        if(export_paths.size() > i)
                            copy_ret_val = copy_file(input_paths[i], export_paths[i], error_message, path_on_removable_media);
                    }
                }
                catch (...)
                {
                    throw Slic3r::ExportError(_u8L("Unknown error occurred during exporting G-code."));
                }

                wxString error_msg;
                switch (copy_ret_val) {
                case CopyFileResult::SUCCESS: break; // no error
                case CopyFileResult::FAIL_COPY_FILE:
                    error_msg = GUI::format(_L("Copying of the temporary G-code to the output G-code failed. Maybe the SD card is write locked?\nError message: %1%"), error_message);
                    break;
                case CopyFileResult::FAIL_FILES_DIFFERENT:
                    error_msg = GUI::format(_L("Copying of the temporary G-code to the output G-code failed. There might be problem with target device, please try exporting again or using different device. The corrupted output G-code is at %1%.tmp."), export_path);
                    break;
                case CopyFileResult::FAIL_RENAMING:
                    error_msg = GUI::format(_L("Renaming of the G-code after copying to the selected destination folder has failed. Current path is %1%.tmp. Please try exporting again."), export_path);
                    break;
                case CopyFileResult::FAIL_CHECK_ORIGIN_NOT_OPENED:
                    error_msg = GUI::format(_L("Copying of the temporary G-code has finished but the original code at %1% couldn't be opened during copy check. The output G-code is at %2%.tmp."), output_path, export_path);
                    break;
                case CopyFileResult::FAIL_CHECK_TARGET_NOT_OPENED:
                    error_msg = GUI::format(_L("Copying of the temporary G-code has finished but the exported code couldn't be opened during copy check. The output G-code is at %1%.tmp."), export_path);
                    break;
                default:
                    error_msg = _u8L("Unknown error occurred during exporting G-code.");
                    break;
                }
                BOOST_LOG_TRIVIAL(error) << "Unexpected fail code(" << error_msg << ") durring copy_file() to " << export_path << ".";
                // notification_manager->push_delayed_notification(NotificationType::ExportOngoing, []() {return true; }, 1000, 0);
            } else {
                BOOST_LOG_TRIVIAL(info) << "output_path  is empty";
            }

            // Storing a path to AppConfig either as path to removable media or a path to internal media.
            // is_path_on_removable_drive() is called with the "true" parameter to update its internal database as the user may have shuffled the external drives
            // while the dialog was open.
            appconfig.update_last_output_dir(output_path.parent_path().string(), path_on_removable_media);

            result.success = true;
        }    
    }while(0);
    
    return result;
}

};
};