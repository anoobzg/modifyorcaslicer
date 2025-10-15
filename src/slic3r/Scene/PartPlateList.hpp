#pragma once
#include "slic3r/GUI/Frame/Plater.hpp"
#include "slic3r/Slice/GCodeImportExporter.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/PrintConfig.hpp"

namespace Slic3r {
namespace GUI {

class SceneRaycaster;
class PartPlateList : public ObjectBase
{
    Plater* m_plater; //Plater reference, not own it
    Model* m_model; //Model reference, not own it
    GCodeImportExporter* m_gcode_import_exporter;

    std::vector<PartPlate*> m_plate_list;
    std::mutex m_plates_mutex;
    int m_plate_area_count {1};
    int m_plate_count;
    int m_plate_cols;
    int m_current_plate;
    int m_print_index;

    int m_plate_width;
    int m_plate_depth;
    int m_plate_height;

    float m_height_to_lid;
    float m_height_to_rod;
    PartPlate::HeightLimitMode m_height_limit_mode{PartPlate::HEIGHT_LIMIT_BOTH};

    PartPlate unprintable_plate;
    std::vector<Pointfs> m_shape_group;
    std::vector<Pointfs> m_exclude_areas_group;
    BoundingBoxf3 m_bounding_box;
    bool m_intialized;
    std::string m_logo_texture_filename;

    bool m_is_dark = false;

    void init();
    //compute the origin for printable plate with index i
    Vec3d compute_origin(int index, int column_count);
    //compute the origin for unprintable plate
    Vec3d compute_origin_for_unprintable();
    //compute shape position
    Vec2d compute_shape_position(int index, int cols);
    //generate icon textures
    void generate_icon_textures();
    void release_icon_textures();

    void set_default_wipe_tower_pos_for_plate(int plate_idx);

    friend class cereal::access;
    friend class UndoRedo::StackImpl;
    friend class PartPlate;

public:
    static const unsigned int MAX_PLATES_COUNT = MAX_PLATE_COUNT;

    PartPlateList(int width, int depth, int height, Plater* platerObj, Model* modelObj);
    PartPlateList(Plater* platerObj, Model* modelObj);
    ~PartPlateList();

    void set_plate_area_count(int count);

    //this may be happened after machine changed
    void reset_size(int width, int depth, int height, bool reload_objects = true, bool update_shapes = false);
    //clear all the instances in the plate, but keep the plates
    void clear(bool delete_plates = false, bool release_print_list = false, bool except_locked = false, int plate_index = -1);
    //clear all the instances in the plate, and delete the plates, only keep the first default plate
    void reset(bool do_init);
    //compute the origin for printable plate with index i using new width
    Vec3d compute_origin_using_new_size(int i, int new_width, int new_depth);

    //reset partplate to init states
    void reinit();

    //get the plate stride
    double plate_stride_x();
    double plate_stride_y();
    void get_plate_size(int& width, int& depth, int& height) {
        width = m_plate_width;
        depth = m_plate_depth;
        height = m_plate_height;
    }

    // Pantheon: update plates after moving plate to the front
    void update_plates();

    /*basic plate operations*/
    //create an empty plate and return its index
    int create_plate(bool adjust_position = true);

    // duplicate plate
    int duplicate_plate(int index);

    //destroy print which has the index of print_index
    int destroy_print(int print_index);

    //delete a plate by index
    int delete_plate(int index);

    //delete a plate by pointer
    //int delete_plate(PartPlate* plate);
    void delete_selected_plate();

    //get a plate pointer by index
    PartPlate* get_plate(int index);

    void get_height_limits(float& height_to_lid, float& height_to_rod)
    {
        height_to_lid = m_height_to_lid;
        height_to_rod = m_height_to_rod;
    }

    void set_height_limits_mode(PartPlate::HeightLimitMode mode)
    {
        m_height_limit_mode = mode;
    }

    int get_curr_plate_index() const { return m_current_plate; }
    PartPlate* get_curr_plate() { return m_plate_list[m_current_plate]; }
    const PartPlate* get_curr_plate() const { return m_plate_list[m_current_plate]; }

    std::vector<PartPlate*>& get_plate_list() { return m_plate_list; };

    PartPlate* get_selected_plate();

    std::vector<PartPlate*> get_nonempty_plate_list();

    std::vector<const GCodeProcessorResult*> get_nonempty_plates_slice_results();

    //compute the origin for printable plate with index i
    Vec3d get_current_plate_origin() { return compute_origin(m_current_plate, m_plate_cols); }
    Vec2d get_current_shape_position() { return compute_shape_position(m_current_plate, m_plate_cols); }
    std::vector<Pointfs> get_exclude_area() { return m_exclude_areas_group; }

    std::set<int> get_extruders(bool conside_custom_gcode = false) const;

    //select plate
    int select_plate(int index);

    //get the plate counts, not including the invalid plate
    int get_plate_count() const;

    //update the plate cols due to plate count change
    void update_plate_cols();

    void update_all_plates_pos_and_size(bool adjust_position = true, bool with_unprintable_move = true, bool switch_plate_type = false, bool do_clear = true);

    //get the plate cols
    int get_plate_cols() { return m_plate_cols; }

    //move the plate to position index
    int move_plate_to_index(int old_index, int new_index);

    //lock plate
    int lock_plate(int index, bool state);

    //is locked
    bool is_locked(int index) { return m_plate_list[index]->is_locked();}

    //find plate by print index, return -1 if not found
    int find_plate_by_print_index(int index);

    /*instance related operations*/
    //find instance in which plate, return -1 when not found
    //this function only judges whether it is intersect with plate
    int find_instance(int obj_id, int instance_id);
    int find_instance(BoundingBoxf3& bounding_box);

    //find instance belongs to which plate
    //this function not only judges whether it is intersect with plate, but also judges whether it is fully included in plate
    //returns -1 when can not find any plate
    int find_instance_belongs(int obj_id, int instance_id);

    //notify instance's update, need to refresh the instance in plates
    int notify_instance_update(int obj_id, int instance_id, bool is_new = false);

    //notify instance is removed
    int notify_instance_removed(int obj_id, int instance_id);

    //add instance to special plate, need to remove from the original plate
    int add_to_plate(int obj_id, int instance_id, int plate_id);

    //reload all objects
    int reload_all_objects(bool except_locked = false, int plate_index = -1);

    //reload objects for newly created plate
    int construct_objects_list_for_new_plate(int plate_index);

    /* arrangement related functions */
    //compute the plate index
    int compute_plate_index(arrangement::ArrangePolygon& arrange_polygon);
    //preprocess an arrangement::ArrangePolygon, return true if it is in a locked plate
    bool preprocess_arrange_polygon(int obj_index, int instance_index, arrangement::ArrangePolygon& arrange_polygon, bool selected);
    bool preprocess_arrange_polygon_other_locked(int obj_index, int instance_index, arrangement::ArrangePolygon& arrange_polygon, bool selected);
    bool preprocess_exclude_areas(arrangement::ArrangePolygons& unselected, int num_plates = 16, float inflation = 0);
    bool preprocess_nonprefered_areas(arrangement::ArrangePolygons& regions, int num_plates = 1, float inflation=0);

    void postprocess_bed_index_for_selected(arrangement::ArrangePolygon& arrange_polygon);
    void postprocess_bed_index_for_unselected(arrangement::ArrangePolygon& arrange_polygon);
    void postprocess_bed_index_for_current_plate(arrangement::ArrangePolygon& arrange_polygon);

    //postprocess an arrangement:;ArrangePolygon
    void postprocess_arrange_polygon(arrangement::ArrangePolygon& arrange_polygon, bool selected);

    /*rendering related functions*/
    void on_change_color_mode(bool is_dark) { m_is_dark = is_dark; }
    void render(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom, bool only_current = false, bool only_body = false, int hover_id = -1, bool render_cali = false, bool show_grid = true);
    void register_raycasters_for_picking(SceneRaycaster* raycaster)
    {
        for (auto plate : m_plate_list)
            plate->register_raycasters_for_picking(raycaster);
    }
    BoundingBoxf3& get_bounding_box() { return m_bounding_box; }
    //int select_plate_by_hover_id(int hover_id);
    int select_plate_by_obj(int obj_index, int instance_index);
    void calc_bounding_boxes();
    void select_plate_view();
    bool set_shapes(const std::vector<Pointfs> & shape_group, const  std::vector<Pointfs>& exclude_areas_group, const std::string& custom_texture, float height_to_lid, float height_to_rod);
    void set_hover_id(int id);
    void reset_hover_id();
    bool intersects(const BoundingBoxf3 &bb);
    bool contains(const BoundingBoxf3 &bb);

    /*slice related functions*/
    //update current slice context into backgroud slicing process
    void update_slice_context_to_current_plate(BackgroundSlicingProcess& process);
    //return the slice result
    // GCodeResult* get_current_slice_result() const;
    GCodeResultWrapper* get_current_slice_result_wrapper() const;

    //invalid all the plater's slice result
    void invalid_all_slice_result();
    //set current plater's slice result to valid
    void update_current_slice_result_state(bool valid) { m_plate_list[m_current_plate]->update_slice_result_valid_state(valid); }
    //is slice result valid or not
    bool is_all_slice_results_valid() const;
    bool is_all_slice_results_ready_for_print() const;
    bool is_all_plates_ready_for_slice() const;
    bool is_all_slice_result_ready_for_export() const;

    //get the all the sliced result
    void get_sliced_result(std::vector<bool>& sliced_result, std::vector<std::string>& gcode_paths);
    //retruct plates structures after de-serialize
    int rebuild_plates_after_deserialize(std::vector<bool>& previous_sliced_result, std::vector<std::string>& previous_gcode_paths);

    //retruct plates structures after auto-arrangement
    int rebuild_plates_after_arrangement(bool recycle_plates = true, bool except_locked = false, int plate_index = -1);

    /* load/store releted functions, with_gcode = true and plate_idx = -1, export all gcode
    * if with_gcode = true and specify plate_idx, export plate_idx gcode only
    */
    int store_to_3mf_structure(PlateDataPtrs& plate_data_list, bool with_slice_info = true, int plate_idx = -1);
    int load_from_3mf_structure(PlateDataPtrs& plate_data_list);
    //load gcode files
    int load_gcode_files();

    template<class Archive> void serialize(Archive& ar)
    {
        //ar(cereal::base_class<ObjectBase>(this));
        //Cancel undo/redo for m_shape ,Because the printing area of different models is different, currently if the grid changes, it cannot correspond to the model on the left ui
        ar(m_plate_width, m_plate_depth, m_plate_height, m_height_to_lid, m_height_to_rod, m_height_limit_mode, m_plate_count, m_current_plate, m_plate_list, unprintable_plate);
        //ar(m_plate_width, m_plate_depth, m_plate_height, m_plate_count, m_current_plate);
    }

    // void init_bed_type_info();
    // void load_bedtype_textures();

    // void show_cali_texture(bool show = true);
    // void init_cali_texture_info();
    // void load_cali_textures();

    // BedTextureInfo bed_texture_info[btCount];
    // BedTextureInfo cali_texture_info;
};

}
}