#ifndef __part_plate_hpp_
#define __part_plate_hpp_

#include <vector>
#include <set>
#include <array>
#include <thread>
#include <mutex>

#include "libslic3r/ObjectID.hpp"
#include "libslic3r/Slicing.hpp"
#include "libslic3r/Arrange.hpp"
#include "slic3r/Slice/GCodeImportExporter.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "slic3r/Render/PlateBed.hpp"
#include "slic3r/Render/MeshUtils.hpp"
#include "libslic3r/ParameterUtils.hpp"

class GLUquadric;
typedef class GLUquadric GLUquadricObject;


// use PLATE_CURRENT_IDX stands for using current plate
// and use PLATE_ALL_IDX
#define PLATE_CURRENT_IDX   -1
#define PLATE_ALL_IDX       -2

#define MAX_PLATE_COUNT     36

inline int compute_colum_count(int count)
{
    float value = sqrt((float)count);
    float round_value = round(value);
    int cols;

    if (value > round_value)
        cols = round_value +1;
    else
        cols = round_value;

    return cols;
}

namespace Slic3r {

class Model;
class ModelObject;
class ModelInstance;
class Print;
class SLAPrint;
class GCodeProcessorResult;

namespace GUI {
class Plater;
struct Camera;
class PartPlateList;
class GCodeImportExporter;
class PlateBed;
class SceneRaycaster;

using GCodeResult = GCodeProcessorResult;
class GCodeResultWrapper;

class PartPlate : public ObjectBase
{
public:
    enum HeightLimitMode{
        HEIGHT_LIMIT_NONE,
        HEIGHT_LIMIT_BOTTOM,
        HEIGHT_LIMIT_TOP,
        HEIGHT_LIMIT_BOTH
    };

private:
    PartPlateList* m_partplate_list {nullptr };
    Plater* m_plater; //Plater reference, not own it
    Model* m_model; //Model reference, not own it

    PlateBed m_plate_bed;

    std::set<std::pair<int, int>> obj_to_instance_set;
    std::set<std::pair<int, int>> instance_outside_set;
    int m_plate_index;
    int m_progress_palte_index;// for part plate progress

    bool m_printable;
    bool m_locked;
    bool m_ready_for_slice;
    bool m_slice_result_valid;
    bool m_apply_invalid {false};
    bool m_has_external_gcode {false};  // Flag indicating if gcode was loaded from external file
    float m_slice_percent;

    GCodeResultWrapper* m_gcode_result;
    std::vector<std::string> m_area_gcode_files_from_3mf;


    std::vector<FilamentInfo> slice_filaments_info;
    int m_print_index;

    std::string m_temp_config_3mf_path; //use a temp path to store the config 3mf
    std::string m_gcode_path_from_3mf;  //use a path to store the gcode loaded from 3mf

    friend class PartPlateList;

    float m_scale_factor{ 1.0f };
    int m_hover_id;
    bool m_selected;
    int m_timelapse_warning_code = 0;

    // BBS
    DynamicPrintConfig m_config;

    // SoftFever
    // part plate name
    std::string m_name;

    void init();
    void set_plate_area_count(int count);
    
    bool valid_instance(int obj_id, int instance_id);
    void generate_print_polygon(std::vector<ExPolygon> &print_polygon);
    void generate_exclude_polygon(std::vector<ExPolygon>&exclude_polygon);
    void generate_logo_polygon(ExPolygon &logo_polygon);
    
    void register_raycasters_for_picking(SceneRaycaster* raycaster);

    
public:
    static const unsigned int PLATE_NAME_HOVER_ID = 6;
    static const unsigned int GRABBER_COUNT = 8;

    PartPlate();
    PartPlate(PartPlateList *partplate_list, Vec3d origin, int width, int depth, int height, Plater* platerObj, Model* modelObj, bool printable = true);
    ~PartPlate();

    bool operator<(PartPlate&) const;

    //clear alll the instances in plate
    void clear(bool clear_sliced_result = true);

    BedType get_bed_type(bool load_from_project = false) const;
    void set_bed_type(BedType bed_type);
    void reset_bed_type();
    DynamicPrintConfig* config() { return &m_config; }

    // set print sequence per plate
    //bool print_seq_same_global = true;
    void set_print_seq(PrintSequence print_seq = PrintSequence::ByDefault);
    PrintSequence get_print_seq() const;
    // Get the real effective print sequence of current plate.
    // If curr_plate's print_seq is ByDefault, use the global sequence
    // @return PrintSequence::{ByLayer,ByObject}
    PrintSequence get_real_print_seq(bool* plate_same_as_global=nullptr) const;

    bool has_spiral_mode_config() const;
    bool get_spiral_vase_mode() const;
    void set_spiral_vase_mode(bool spiral_mode, bool as_global);

    //static const int plate_x_offset = 20; //mm
    //static const double plate_x_gap = 0.2;
    ThumbnailData thumbnail_data;
    ThumbnailData no_light_thumbnail_data;
    static const int plate_thumbnail_width = 512;
    static const int plate_thumbnail_height = 512;

    ThumbnailData top_thumbnail_data;
    ThumbnailData pick_thumbnail_data;

    //ThumbnailData cali_thumbnail_data;
    PlateBBoxData cali_bboxes_data;
    //static const int cali_thumbnail_width = 2560;
    //static const int cali_thumbnail_height = 2560;

    //set the plate's index
    void set_index(int index);

    //get the plate's index
    int get_index() { return m_plate_index; }

    // SoftFever
    //get the plate's name
    std::string get_plate_name() const { return m_name; }
    void generate_plate_name_texture();
    //set the plate's name
    void set_plate_name(const std::string& name);

    void set_timelapse_warning_code(int code) { m_timelapse_warning_code = code; }
    int  timelapse_warning_code() { return m_timelapse_warning_code; }
    
    void get_print(GCodeResultWrapper**result, int *index);
    //set the print object, result and it's index
    void set_print(GCodeResultWrapper* result = nullptr, int index = -1);

    GCodeResultWrapper* get_gcode_result();

    //get gcode filename
    std::string get_gcode_filename();

    bool is_valid_gcode_file();

    //get the plate's center point origin
    Vec3d get_center_origin();
    /* size and position related functions*/
    //set position and size
    void set_pos_and_size(Vec3d& origin, int width, int depth, int height, bool with_instance_move, bool do_clear = true);

    // BBS
    int& get_width(); 
    int& get_depth();
    int& get_height();
    Vec2d get_size() const;
    Vec3d& get_origin();
    ModelObjectPtrs get_objects();
    ModelObjectPtrs get_objects_on_this_plate();
    ModelInstance* get_instance(int obj_id, int instance_id);
    BoundingBoxf3 get_objects_bounding_box();

    Vec3d estimate_wipe_tower_size(const DynamicPrintConfig & config, const double w, const double d, int plate_extruder_size = 0, bool use_global_objects = false) const;
    arrangement::ArrangePolygon estimate_wipe_tower_polygon(const DynamicPrintConfig & config, int plate_index, int plate_extruder_size = 0, bool use_global_objects = false) const;
    std::vector<int> get_extruders(bool conside_custom_gcode = false) const;
    std::vector<int> get_extruders_under_cli(bool conside_custom_gcode, DynamicPrintConfig& full_config) const;
    std::vector<int> get_extruders_without_support(bool conside_custom_gcode = false) const;
    std::vector<int> get_used_extruders();

    /* instance related operations*/
    //judge whether instance is bound in plate or not
    bool contain_instance(int obj_id, int instance_id);
    bool contain_instance_totally(ModelObject* object, int instance_id) const;
    //judge whether instance is totally included in plate or not
    bool contain_instance_totally(int obj_id, int instance_id) const;

    //judge whether the plate's origin is at the left of instance or not
    bool is_left_top_of(int obj_id, int instance_id);

    //check whether instance is outside the plate or not
    bool check_outside(int obj_id, int instance_id, BoundingBoxf3* bounding_box = nullptr);

    //judge whether instance is intesected with plate or not
    bool intersect_instance(int obj_id, int instance_id, BoundingBoxf3* bounding_box = nullptr);

    //add an instance into plate
    int add_instance(int obj_id, int instance_id, bool move_position, BoundingBoxf3* bounding_box = nullptr);

    //remove instance from plate
    int remove_instance(int obj_id, int instance_id);

    //translate instance on the plate
    void translate_all_instance(Vec3d position);

    //duplicate all instance for count
    void duplicate_all_instance(unsigned int dup_count, bool need_skip, std::map<int, bool>& skip_objects);

    //update instance exclude state
    void update_instance_exclude_status(int obj_id, int instance_id, BoundingBoxf3* bounding_box = nullptr);

    //update object's index caused by original object deleted
    void update_object_index(int obj_idx_removed, int obj_idx_max);

    // set objects configs when enabling spiral vase mode.
    void set_vase_mode_related_object_config(int obj_id = -1);

    //whether it is empty
    bool empty() { return obj_to_instance_set.empty(); }

    int printable_instance_size();

    //whether it is has printable instances
    bool has_printable_instances();
    bool is_all_instances_unprintable();

    //move instances to left or right PartPlate
    void move_instances_to(PartPlate& left_plate, PartPlate& right_plate, BoundingBoxf3* bounding_box = nullptr);

    /*rendering related functions*/
    const std::vector<Pointfs>& get_shape() const;
    bool set_shape(const std::vector<Pointfs>& shape_group, const std::vector<Pointfs>& exclude_areas_group, Vec2d position, float height_to_lid, float height_to_rod);
    bool contains(const Vec3d& point) const;
    bool contains(const GLVolume& v) const;
    bool contains(const BoundingBoxf3& bb) const;
    bool intersects(const BoundingBoxf3& bb) const;

    void render(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom, bool only_body = false, bool force_background_color = false, HeightLimitMode mode = HEIGHT_LIMIT_NONE, int hover_id = -1, bool render_cali = false, bool show_grid = true);

    void set_selected();
    void set_unselected();
    void set_hover_id(int id) { m_hover_id = id; }
    const BoundingBoxf3& get_bounding_box(bool extended = false);
    const std::vector<BoundingBoxf3>& get_exclude_areas();
    std::vector<BoundingBoxf3> get_exclude_bounding_box();
    const BoundingBox get_bounding_box_crd();
    BoundingBoxf3 get_plate_box();
    // Orca: support non-rectangular bed
    BoundingBoxf3 get_build_volume();
    bool check_intersects(const BoundingBoxf3& box);

    /*status related functions*/
    //update status
    void update_states();

    //is locked or not
    bool is_locked() const { return m_locked; }
    void lock(bool state) { m_locked = state; }

    //is a printable plate or not
    bool is_printable() const { return m_printable; }

    //can be sliced or not
    bool can_slice() const
    {
        return m_ready_for_slice && !m_apply_invalid && !obj_to_instance_set.empty();
    }
    void update_slice_ready_status(bool ready_slice)
    {
        m_ready_for_slice = ready_slice;
    }

    //bedtype mismatch or not
    bool is_apply_result_invalid() const
    {
        return m_apply_invalid;
    }
    void update_apply_result_invalid(bool invalid)
    {
        m_apply_invalid = invalid;
    }

    //is slice result valid or not
    bool is_slice_result_valid() const
    {
        return m_slice_result_valid;
    }

    //is slice result ready for print
    bool is_slice_result_ready_for_print() const;

    // check whether plate's slice result valid for export to file
    bool is_slice_result_ready_for_export()
    {
        return is_slice_result_ready_for_print() && has_printable_instances();
    }

    //invalid sliced result
    void update_slice_result_valid_state(bool valid = false);

    // External gcode flag management
    void set_has_external_gcode(bool value) { 
        m_has_external_gcode = value; 
    }
    bool has_external_gcode() const { 
        return m_has_external_gcode; 
    }
    void clear_external_gcode() {
        m_has_external_gcode = false;
    }

    void update_slicing_percent(float percent)
    {
        m_slice_percent = percent;
    }

    float get_slicing_percent() { return m_slice_percent; }

    void set_slicing_progress();

    void set_slicing_progress_index(int index);

    /*slice related functions*/
    //update current slice context into backgroud slicing process
    void update_slice_context(BackgroundSlicingProcess& process);

    //return the slice result
    std::vector<const GCodeProcessorResult*> get_slice_result();
    GCodeResultWrapper* get_slice_result_wrapper();

    std::string           get_temp_config_3mf_path();
    //this API should only be used for command line usage
    void set_tmp_gcode_path(std::string new_path)
    {
    }
    //load gcode from file
    int load_gcode_from_file(const std::string& filename);
    //load thumbnail data from file
    int load_thumbnail_data(std::string filename, ThumbnailData& thumb_data);
    //load pattern box data from file
    int load_pattern_box_data(std::string filename);

    std::vector<int> get_first_layer_print_sequence() const;
    std::vector<LayerPrintSequence> get_other_layers_print_sequence() const;
    void set_first_layer_print_sequence(const std::vector<int> &sorted_filaments);
    void set_other_layers_print_sequence(const std::vector<LayerPrintSequence>& layer_seq_list);
    void update_first_layer_print_sequence(size_t filament_nums);

    friend class cereal::access;
    friend class UndoRedo::StackImpl;

    template<class Archive> void load(Archive& ar) {
        std::vector<std::pair<int, int>>	objects_and_instances;
        std::vector<std::pair<int, int>>	instances_outside;

        ar(m_plate_index, m_name, m_print_index, m_plate_bed.m_origin, m_plate_bed.m_width, m_plate_bed.m_depth, m_plate_bed.m_height, m_locked, m_selected, m_ready_for_slice, m_slice_result_valid, m_apply_invalid, m_printable, objects_and_instances, instances_outside, m_config);

        for (std::vector<std::pair<int, int>>::iterator it = objects_and_instances.begin(); it != objects_and_instances.end(); ++it)
            obj_to_instance_set.insert(std::pair(it->first, it->second));

        for (std::vector<std::pair<int, int>>::iterator it = instances_outside.begin(); it != instances_outside.end(); ++it)
            instance_outside_set.insert(std::pair(it->first, it->second));
    }
    template<class Archive> void save(Archive& ar) const {
        std::vector<std::pair<int, int>>	objects_and_instances;
        std::vector<std::pair<int, int>>	instances_outside;

        for (std::set<std::pair<int, int>>::iterator it = instance_outside_set.begin(); it != instance_outside_set.end(); ++it)
            instances_outside.emplace_back(it->first, it->second);

        for (std::set<std::pair<int, int>>::iterator it = obj_to_instance_set.begin(); it != obj_to_instance_set.end(); ++it)
            objects_and_instances.emplace_back(it->first, it->second);

        ar(m_plate_index, m_name, m_print_index, m_plate_bed.m_origin, m_plate_bed.m_width, m_plate_bed.m_depth, m_plate_bed.m_height, m_locked, m_selected, m_ready_for_slice, m_slice_result_valid, m_apply_invalid, m_printable, objects_and_instances, instances_outside, m_config);
    }

};

} // namespace GUI
} // namespace Slic3r

namespace cereal
{
    template <class Archive> struct specialize<Archive, Slic3r::GUI::PartPlate, cereal::specialization::member_load_save> {};
}
#endif //__part_plate_hpp_
