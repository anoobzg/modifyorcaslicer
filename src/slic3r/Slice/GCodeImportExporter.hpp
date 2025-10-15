#ifndef _slic3r_GCodeImportExporter_hpp_
#define _slic3r_GCodeImportExporter_hpp_

#include <vector>

namespace Slic3r {
namespace GUI {

class PartPlateList;
struct PlateGCodeFile
{
    bool is_area_gcode { false };
    std::string file;
    std::vector<std::string> areas;
};

class GCodeImportExporter
{
public:
    GCodeImportExporter(PartPlateList* part_plate_list);

    void import_plate_gcode_files(int plate_id, const PlateGCodeFile& gcode_files);
    void import_all_plate_gcode_files(const std::vector<PlateGCodeFile>& gcode_files);

private:
    PartPlateList* m_part_plate_list { NULL };

};


struct GCodeExportParam
{
    bool export_3mf = false;
    bool prefer_removable = false; 
};

struct ExportResult
{
    bool success = false;
    bool is_removable_path = false;

    std::string last_output_path;
    std::string last_output_dir_path;
};

class PartPlate;
ExportResult export_gcode_from_part_plate(PartPlate* part_plate, const GCodeExportParam& param);

};
};



#endif