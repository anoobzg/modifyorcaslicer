#ifndef _slic3r_GCodeResultWrapper_hpp_
#define _slic3r_GCodeResultWrapper_hpp_

#include <vector>
#include "libslic3r/GCode/GCodeProcessor.hpp"

namespace Slic3r {
class GCodeProcessorResult;
class Model;
class Print;
namespace GUI {

using GCodeResult = GCodeProcessorResult;
class GCodeResultWrapper
{
public:
    GCodeResultWrapper(Model* model);

    GCodeResult* get_result(int id = 0);
    Print* get_print(int id = 0);

    std::vector<const GCodeResult*> get_const_all_result() const;
    std::vector<GCodeResult*> get_all_result();
    int size() const;
    void resize(int size);

    bool is_valid();
    std::vector<std::string> get_area_gcode_paths();
    std::vector<Print*> get_prints();

    /* method */
    void reset();


    float max_time(int type = 0);
    bool get_toolpath_outside();
    bool& toolpath_outside() { return m_agent->toolpath_outside; }
    std::string& filename() { return m_agent->filename; }
    int& timelapse_warning_code() { return m_agent->timelapse_warning_code; }
    bool& label_object_enabled() { return m_agent->label_object_enabled; }
    PrintEstimatedStatistics& print_statistics() { return m_agent->print_statistics; }
    std::vector<GCodeProcessorResult::SliceWarning>& warnings() { return m_agent->warnings; }
    std::vector<GCodeProcessorResult::MoveVertex>& moves() { return m_agent->moves; }
    BedType& bed_type() { return m_agent->bed_type; }
    std::vector<float>& filament_diameters() { return m_agent->filament_diameters; }
    std::vector<float>& filament_densities() { return m_agent->filament_densities; }
    std::vector<float>& filament_costs() { return m_agent->filament_costs; }

private:
    Model* m_model;
    GCodeResult* m_agent;
    std::string m_area_path_prefix;
    std::vector<std::string> m_area_paths; 
    std::vector<GCodeResult*> m_results; 
    std::vector<Print*> m_prints;
    

};


};
};



#endif