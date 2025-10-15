#include "PartPlateList.hpp"

#include "libslic3r/FileSystem/DataDir.hpp"
#include "libslic3r/FileSystem/Log.hpp"
#include "libslic3r/PresetBundle.hpp"

#include "slic3r/GUI/Config/GUI_ObjectList.hpp"
#include "slic3r/Config/AppPreset.hpp"
#include "slic3r/Theme/AppFont.hpp"
#include "slic3r/Render/AppRender.hpp"
#include "slic3r/GUI/Tab.hpp"
#include "slic3r/Slice/GCodeResultWrapper.hpp"

#include "slic3r/Render/PlateBed.hpp"

static const double LOGICAL_PART_PLATE_GAP = 1. / 5.;
static const float GROUND_Z = -0.03f;
static const float WIPE_TOWER_DEFAULT_X_POS = 165.;
static const float WIPE_TOWER_DEFAULT_Y_POS = 250.;  // Max y
static const float I3_WIPE_TOWER_DEFAULT_X_POS = 0.;
static const float I3_WIPE_TOWER_DEFAULT_Y_POS = 250.; // Max y

namespace Slic3r {
namespace GUI {

/* PartPlate List related functions*/
PartPlateList::PartPlateList(int width, int depth, int height, Plater* platerObj, Model* modelObj)
	:m_plate_width(width), m_plate_depth(depth), m_plate_height(height), m_plater(platerObj), m_model(modelObj),
	unprintable_plate(this, Vec3d(0.0 + width * (1. + LOGICAL_PART_PLATE_GAP), 0.0, 0.0), width, depth, height, platerObj, modelObj, false)
{
	BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":plate_width %1%, plate_depth %2%, plate_height %3%") % width % depth % height;

	init();
}
PartPlateList::PartPlateList(Plater* platerObj, Model* modelObj)
	:m_plate_width(0), m_plate_depth(0), m_plate_height(0), m_plater(platerObj), m_model(modelObj),
	unprintable_plate(this, Vec3d(0.0, 0.0, 0.0), m_plate_width, m_plate_depth, m_plate_height, platerObj, modelObj, false)
{
	init();
}

PartPlateList::~PartPlateList()
{
	clear(true, true);
	PlateBed::release_icon_textures();
}

void PartPlateList::init()
{
	m_intialized = false;
	PartPlate* first_plate = NULL;
	first_plate = new PartPlate(this, Vec3d(0.0, 0.0, 0.0), m_plate_width, m_plate_depth, m_plate_height, m_plater, m_model, true);
	assert(first_plate != NULL);
	m_plate_list.push_back(first_plate);

	m_print_index = 0;

	{
		GCodeResultWrapper* gcode_result_wrapper = new GCodeResultWrapper(m_model);
		gcode_result_wrapper->resize(m_plate_area_count);
		first_plate->set_print(gcode_result_wrapper, m_print_index);
		m_print_index++;

		m_gcode_import_exporter = new GCodeImportExporter(this);
	}
	first_plate->set_index(0);

	m_plate_count = 1;
	m_plate_cols = 1;
	m_current_plate = 0;

	if (m_plater) {
        // In GUI mode
        set_default_wipe_tower_pos_for_plate(0);
    }

	select_plate(0);
	unprintable_plate.set_index(1);

	m_intialized = true;
}

//compute the origin for printable plate with index i
Vec3d PartPlateList::compute_origin(int i, int cols)
{
	Vec3d origin;
	Vec2d pos = compute_shape_position(i, cols);
	origin    = Vec3d(pos.x(), pos.y(), 0);

	return origin;
}

//compute the origin for printable plate with index i using new width
Vec3d PartPlateList::compute_origin_using_new_size(int i, int new_width, int new_depth)
{
	Vec3d origin;
	int row, col;

	row = i / m_plate_cols;
	col = i % m_plate_cols;

	origin(0) = col * (new_width * (1. + LOGICAL_PART_PLATE_GAP));
	origin(1) = -row * (new_depth * (1. + LOGICAL_PART_PLATE_GAP));
	origin(2) = 0;

	return origin;
}


//compute the origin for printable plate with index i
Vec3d PartPlateList::compute_origin_for_unprintable()
{
	int max_count = m_plate_cols * m_plate_cols;
	if (m_plate_count == max_count)
		return compute_origin(max_count + m_plate_cols - 1, m_plate_cols + 1);
	else
		return compute_origin(m_plate_count, m_plate_cols);
}

//compute shape position
Vec2d PartPlateList::compute_shape_position(int index, int cols)
{
	Vec2d pos;
	int row, col;

	row = index / cols;
	col = index % cols;

	pos(0) = col * plate_stride_x();
	pos(1) = -row * plate_stride_y();

	return pos;
}

void PartPlateList::set_default_wipe_tower_pos_for_plate(int plate_idx)
{
    DynamicConfig &     proj_cfg     = app_preset_bundle()->project_config;
    ConfigOptionFloats *wipe_tower_x = proj_cfg.opt<ConfigOptionFloats>("wipe_tower_x");
    ConfigOptionFloats *wipe_tower_y = proj_cfg.opt<ConfigOptionFloats>("wipe_tower_y");
    wipe_tower_x->values.resize(m_plate_list.size(), wipe_tower_x->values.front());
    wipe_tower_y->values.resize(m_plate_list.size(), wipe_tower_y->values.front());

    auto printer_structure_opt = app_preset_bundle()->printers.get_edited_preset().config.option<ConfigOptionEnum<PrinterStructure>>("printer_structure");
    // set the default position, the same with print config(left top)
    ConfigOptionFloat wt_x_opt(WIPE_TOWER_DEFAULT_X_POS);
    ConfigOptionFloat wt_y_opt(WIPE_TOWER_DEFAULT_Y_POS);
    if (printer_structure_opt && printer_structure_opt->value == PrinterStructure::psI3) {
        wt_x_opt = ConfigOptionFloat(I3_WIPE_TOWER_DEFAULT_X_POS);
        wt_y_opt = ConfigOptionFloat(I3_WIPE_TOWER_DEFAULT_Y_POS);
    }
    dynamic_cast<ConfigOptionFloats *>(proj_cfg.option("wipe_tower_x"))->set_at(&wt_x_opt, plate_idx, 0);
    dynamic_cast<ConfigOptionFloats *>(proj_cfg.option("wipe_tower_y"))->set_at(&wt_y_opt, plate_idx, 0);
}

void PartPlateList::set_plate_area_count(int count)
{
	m_plate_area_count = count;
	for (PartPlate* plate : m_plate_list)
		plate->set_plate_area_count(count);
}

//this may be happened after machine changed
void PartPlateList::reset_size(int width, int depth, int height, bool reload_objects, bool update_shapes)
{
	Vec3d origin1, origin2;

	BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":before size: plate_width %1%, plate_depth %2%, plate_height %3%") % m_plate_width % m_plate_depth % m_plate_height;
	BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":after size: plate_width %1%, plate_depth %2%, plate_height %3%") % width % depth % height;
	if ((m_plate_width != width) || (m_plate_depth != depth) || (m_plate_height != height))
	{
		m_plate_width = width;
		m_plate_depth = depth;
		m_plate_height = height;
		update_all_plates_pos_and_size(false, false, true);
		if (update_shapes) {
			set_shapes(m_shape_group, m_exclude_areas_group, m_logo_texture_filename, m_height_to_lid, m_height_to_rod);
		}
		if (reload_objects)
			reload_all_objects();
		else
			clear(false, false, false, -1);
	}

	return;
}

//clear all the instances in the plate, but keep the plates
void PartPlateList::clear(bool delete_plates, bool release_print_list, bool except_locked, int plate_index)
{
	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		PartPlate* plate = m_plate_list[i];
		assert(plate != NULL);

		if (except_locked && plate->is_locked())
			plate->clear(false);
		else if ((plate_index != -1) && (plate_index != i))
			plate->clear(false);
		else
			plate->clear();
		if (delete_plates)
			delete plate;
	}

	if (delete_plates)
	{
		//also delete print related to the plate
		m_plate_list.clear();
		m_current_plate = 0;
	}

	unprintable_plate.clear();
}

//clear all the instances in the plate, and delete the plates, only keep the first default plate
void PartPlateList::reset(bool do_init)
{
	clear(true, false);

	//m_plate_list.clear();

	if (do_init)
		init();

	return;
}

//reset partplate to init states
void PartPlateList::reinit()
{
	clear(true, true);

	init();

	//reset plate 0's position
	Vec2d pos = compute_shape_position(0, m_plate_cols);
	m_plate_list[0]->set_shape(m_shape_group, m_exclude_areas_group, pos, m_height_to_lid, m_height_to_rod);
	//reset unprintable plate's position
	Vec3d origin2 = compute_origin_for_unprintable();
	unprintable_plate.set_pos_and_size(origin2, m_plate_width, m_plate_depth, m_plate_height, false);
	//re-calc the bounding boxes
	calc_bounding_boxes();

	return;
}

/*basic plate operations*/
//create an empty plate, and return its index
//these model instances which are not in any plates should not be affected also

void PartPlateList::update_plates()
{
    update_all_plates_pos_and_size(true, false);
    set_shapes(m_shape_group, m_exclude_areas_group, m_logo_texture_filename, m_height_to_lid, m_height_to_rod);
}

int PartPlateList::create_plate(bool adjust_position)
{
	PartPlate* plate = NULL;
	Vec3d origin;
	int new_index;

	new_index = m_plate_list.size();
	if (new_index >= MAX_PLATES_COUNT)
		return -1;
	int cols = compute_colum_count(new_index + 1);
	int old_cols = compute_colum_count(new_index);

	origin = compute_origin(new_index, cols);
	plate = new PartPlate(this, origin, m_plate_width, m_plate_depth, m_plate_height, m_plater, m_model, true);
	assert(plate != NULL);

	{
		GCodeResultWrapper* gcode_result_wrapper = new GCodeResultWrapper(m_model);
		gcode_result_wrapper->resize(m_plate_area_count);
		plate->set_print(gcode_result_wrapper, m_print_index);
		m_print_index++;
	}

	plate->set_index(new_index);
	Vec2d pos = compute_shape_position(new_index, cols);
	plate->set_shape(m_shape_group, m_exclude_areas_group, pos, m_height_to_lid, m_height_to_rod);
	m_plate_list.emplace_back(plate);
	update_plate_cols();
	if (old_cols != cols)
	{
		BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(":old_cols %1% -> new_cols %2%") % old_cols % cols;
		//update the origin of each plate
		update_all_plates_pos_and_size(adjust_position, false);
		set_shapes(m_shape_group, m_exclude_areas_group, m_logo_texture_filename, m_height_to_lid, m_height_to_rod);

	}
	else
	{
		BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": the same cols %1%") % old_cols;
		Vec3d origin2 = compute_origin_for_unprintable();
		unprintable_plate.set_pos_and_size(origin2, m_plate_width, m_plate_depth, m_plate_height, false);

		//update bounding_boxes
		calc_bounding_boxes();
	}

	// update wipe tower config
	if (m_plater) {
		// In GUI mode
        set_default_wipe_tower_pos_for_plate(new_index);
	}

	unprintable_plate.set_index(new_index+1);

	//reload all objects here
	if (adjust_position)
		construct_objects_list_for_new_plate(new_index);

	if (m_plater) {
		// In GUI mode
		AppAdapter::obj_list()->on_plate_added(plate);
	}

	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(":created a new plate %1%") % new_index;
	return new_index;
}


int PartPlateList::duplicate_plate(int index)
{
    // create a new plate
    int new_plate_index = create_plate(true);
    PartPlate* old_plate = NULL;
    PartPlate* new_plate = NULL;
    old_plate = get_plate(index);
    new_plate = get_plate(new_plate_index);

    // get the offset between plate centers
    Vec3d plate_to_plate_offset = new_plate->get_origin() - old_plate->get_origin();

    // iterate over all the objects in this plate
    ModelObjectPtrs obj_ptrs = old_plate->get_objects_on_this_plate();
    for (ModelObject* object : obj_ptrs){
        // copy and move the object to the same relative position in the new plate
        ModelObject* object_copy = m_model->add_object(*object);
        int new_obj_id = new_plate->m_model->objects.size() - 1;
        // go over the instances and pair with the object
        for (size_t new_instance_id = 0; new_instance_id < object_copy->instances.size(); new_instance_id++){
            new_plate->obj_to_instance_set.emplace(std::pair(new_obj_id, new_instance_id));
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": duplicate object into plate: index_pair [%1%,%2%], obj_id %3%") % new_obj_id % new_instance_id % object_copy->id().id;
        }
    }
    new_plate->translate_all_instance(plate_to_plate_offset);
    // update the plates
    AppAdapter::obj_list()->reload_all_plates();
    return new_plate_index;
}


//destroy print's objects and results
int PartPlateList::destroy_print(int print_index)
{
	int result = 0;
	return result;
}

//delete a plate by index
//keep its instance at origin position and add them into next plate if have
//update the plate index and position after it
int PartPlateList::delete_plate(int index)
{
	int ret = 0;
	PartPlate* plate = NULL;

	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(":delete plate %1%, count %2%") % index % m_plate_list.size();
	if (index >= m_plate_list.size())
	{
		BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(":can not find plate");
		return -1;
	}
	if (m_plate_list.size() <= 1)
	{
		BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(":only one plate left, can not delete");
		return -1;
	}

	plate = m_plate_list[index];
	if (index != plate->get_index())
	{
		BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(":plate %1%, has an invalid index %2%") % index % plate->get_index();
		return -1;
	}

	if (m_plater) {
		// In GUI mode
		// BBS: add wipe tower logic
		DynamicConfig& proj_cfg = app_preset_bundle()->project_config;
		ConfigOptionFloats* wipe_tower_x = proj_cfg.opt<ConfigOptionFloats>("wipe_tower_x");
		ConfigOptionFloats* wipe_tower_y = proj_cfg.opt<ConfigOptionFloats>("wipe_tower_y");
		// wipe_tower_x and wip_tower_y may be less than plate count in the following case:
		// 1. wipe_tower is enabled after creating new plates
		// 2. wipe tower is not enabled
		if (index < wipe_tower_x->values.size())
			wipe_tower_x->values.erase(wipe_tower_x->values.begin() + index);
		if (index < wipe_tower_y->values.size())
			wipe_tower_y->values.erase(wipe_tower_y->values.begin() + index);
	}

	int cols = compute_colum_count(m_plate_list.size() - 1);
	int old_cols = compute_colum_count(m_plate_list.size());

	m_plate_list.erase(m_plate_list.begin() + index);
	update_plate_cols();
	//update this plate
	//move this plate's instance to the end
	Vec3d current_origin;
	current_origin = compute_origin_for_unprintable();
	plate->set_pos_and_size(current_origin, m_plate_width, m_plate_depth, m_plate_height, true);

	//update the plates after it
	for (unsigned int i = index; i < (unsigned int)m_plate_list.size(); ++i)
	{
		PartPlate* plate = m_plate_list[i];
		assert(plate != NULL);

		plate->set_index(i);
		Vec3d origin = compute_origin(i, m_plate_cols);
		plate->set_pos_and_size(origin, m_plate_width, m_plate_depth, m_plate_height, true);

		//update render shapes
		Vec2d pos = compute_shape_position(i, m_plate_cols);
		plate->set_shape(m_shape_group, m_exclude_areas_group, pos, m_height_to_lid, m_height_to_rod);
	}

	//update current_plate if delete current
	if (m_current_plate == index && index == 0) {
		select_plate(0);
	}
	else if (m_current_plate >= index) {
		select_plate(m_current_plate - 1);
	}

	unprintable_plate.set_index(m_plate_list.size());

	if (old_cols != cols)
	{
		//update the origin of each plate
		update_all_plates_pos_and_size();
		set_shapes(m_shape_group, m_exclude_areas_group, m_logo_texture_filename, m_height_to_lid, m_height_to_rod);
	}
	else
	{
		//update the position of the unprintable plate
		Vec3d origin2 = compute_origin_for_unprintable();
		unprintable_plate.set_pos_and_size(origin2, m_plate_width, m_plate_depth, m_plate_height, true);

		//update bounding_boxes
		calc_bounding_boxes();
	}

	plate->move_instances_to(*(m_plate_list[m_plate_list.size()-1]), unprintable_plate);
	//destroy the print object
	int print_index;
	plate->get_print(nullptr, &print_index);

	delete plate;
	return ret;
}

void PartPlateList::delete_selected_plate()
{
	delete_plate(m_current_plate);
}

//get a plate pointer by index
PartPlate* PartPlateList::get_plate(int index)
{
	PartPlate* plate = NULL;

	if (index >= m_plate_list.size())
	{
		BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(":can not find index %1%, size %2%") % index % m_plate_list.size();
		return NULL;
	}

	plate = m_plate_list[index];
	assert(plate != NULL);

	return plate;
}

PartPlate* PartPlateList::get_selected_plate()
{
	if (m_current_plate < 0 || m_current_plate >= m_plate_list.size()) {
		BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(":can not find m_current_plate  %1%, size %2%") % m_current_plate % m_plate_list.size();
		return NULL;
	}
	return m_plate_list[m_current_plate];
}

std::vector<PartPlate*> PartPlateList::get_nonempty_plate_list()
{
	std::vector<PartPlate*> nonempty_plate_list;
	for (auto plate : m_plate_list){
		if (plate->get_extruders().size() != 0) {
			nonempty_plate_list.push_back(plate);
		}
	}
	return nonempty_plate_list;
}

std::vector<const GCodeProcessorResult*> PartPlateList::get_nonempty_plates_slice_results() {
	std::vector<const GCodeProcessorResult*> nonempty_plates_slice_result;
	for (auto plate : get_nonempty_plate_list()) {
		auto result_wrapper = plate->get_slice_result_wrapper();
		std::vector<const GCodeResult*> result_list = result_wrapper->get_const_all_result();
		for (auto result : result_list)
			nonempty_plates_slice_result.push_back(result);
	}
	return nonempty_plates_slice_result;
}

std::set<int> PartPlateList::get_extruders(bool conside_custom_gcode) const
{
    int plate_count = get_plate_count();
    std::set<int> extruder_ids;

    for (size_t i = 0; i < plate_count; i++) {
        auto plate_extruders = m_plate_list[i]->get_extruders(conside_custom_gcode);
        extruder_ids.insert(plate_extruders.begin(), plate_extruders.end());
    }

    return extruder_ids;
}


//select plate
int PartPlateList::select_plate(int index)
{
	const std::lock_guard<std::mutex> local_lock(m_plates_mutex);
	if (m_plate_list.empty() || index >= m_plate_list.size()) {
		return -1;
	}

	// BBS: erase unnecessary snapshot
	if (get_curr_plate_index() != index && m_intialized) {
		if (m_plater)
			m_plater->take_snapshot("select partplate!");
	}

	std::vector<PartPlate *>::iterator it = m_plate_list.begin();
	for (it = m_plate_list.begin(); it != m_plate_list.end(); it++) {
		(*it)->set_unselected();
	}

	m_current_plate = index;
	m_plate_list[m_current_plate]->set_selected();

	//BBS
	if(m_model)
		m_model->curr_plate_index = index;

	return 0;
}

void PartPlateList::set_hover_id(int id)
{
	int index = id / PlateBed::GRABBER_COUNT;
	int sub_hover_id = id % PlateBed::GRABBER_COUNT;
	m_plate_list[index]->set_hover_id(sub_hover_id);
}

void PartPlateList::reset_hover_id()
{
	const std::lock_guard<std::mutex> local_lock(m_plates_mutex);
	std::vector<PartPlate*>::iterator it = m_plate_list.begin();
	for (it = m_plate_list.begin(); it != m_plate_list.end(); it++) {
		(*it)->set_hover_id(-1);
	}
}

bool PartPlateList::intersects(const BoundingBoxf3& bb)
{
	bool result = false;
	std::vector<PartPlate*>::iterator it = m_plate_list.begin();
	for (it = m_plate_list.begin(); it != m_plate_list.end(); it++) {
		if ((*it)->intersects(bb)) {
			result = true;
		}
	}
	return result;
}

bool PartPlateList::contains(const BoundingBoxf3& bb)
{
	bool result = false;
	std::vector<PartPlate*>::iterator it = m_plate_list.begin();
	for (it = m_plate_list.begin(); it != m_plate_list.end(); it++) {
		if ((*it)->contains(bb)) {
			result = true;
		}
	}
	return result;
}

double PartPlateList::plate_stride_x()
{
	return m_plate_width * (1. + LOGICAL_PART_PLATE_GAP);
}

double PartPlateList::plate_stride_y()
{
	return m_plate_depth * (1. + LOGICAL_PART_PLATE_GAP);
}

//get the plate counts, not including the invalid plate
int PartPlateList::get_plate_count() const
{
	int ret = 0;

	ret = m_plate_list.size();

	return ret;
}

//update the plate cols due to plate count change
void PartPlateList::update_plate_cols()
{
	m_plate_count = m_plate_list.size();

	m_plate_cols = compute_colum_count(m_plate_count);
	BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":m_plate_count %1%, m_plate_cols change to %2%") % m_plate_count % m_plate_cols;
	return;
}

void PartPlateList::update_all_plates_pos_and_size(bool adjust_position, bool with_unprintable_move, bool switch_plate_type, bool do_clear)
{
	Vec3d origin1, origin2;
	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		PartPlate* plate = m_plate_list[i];
		assert(plate != NULL);

		//compute origin1 for PartPlate
		origin1 = compute_origin(i, m_plate_cols);
		plate->set_pos_and_size(origin1, m_plate_width, m_plate_depth, m_plate_height, adjust_position, do_clear);

		// set default wipe pos when switch plate
        if (switch_plate_type && m_plater && plate->get_used_extruders().size() <= 0) {
			set_default_wipe_tower_pos_for_plate(i);
		}
	}

	origin2 = compute_origin_for_unprintable();
	unprintable_plate.set_pos_and_size(origin2, m_plate_width, m_plate_depth, m_plate_height, with_unprintable_move);
}

//move the plate to position index
int PartPlateList::move_plate_to_index(int old_index, int new_index)
{
	int ret = 0, delta;
	Vec3d origin;


	if (old_index == new_index)
	{
		BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(":should not happen, the same index %1%") % old_index;
		return -1;
	}

	if (old_index < new_index)
	{
		delta = 1;
	}
	else
	{
		delta = -1;
	}

	PartPlate* plate = m_plate_list[old_index];
	//update the plates between old_index and new_index
	for (unsigned int i = (unsigned int)old_index; i != (unsigned int)new_index; i = i + delta)
	{
		m_plate_list[i] = m_plate_list[i + delta];
		m_plate_list[i]->set_index(i);

		origin = compute_origin(i, m_plate_cols);
		m_plate_list[i]->set_pos_and_size(origin, m_plate_width, m_plate_depth, m_plate_height, true);
	}
	origin = compute_origin(new_index, m_plate_cols);
	m_plate_list[new_index] = plate;
	plate->set_index(new_index);
	plate->set_pos_and_size(origin, m_plate_width, m_plate_depth, m_plate_height, true);

	//update the new plate index
	m_current_plate = new_index;

	return ret;
}

//lock plate
int PartPlateList::lock_plate(int index, bool state)
{
	int ret = 0;
	PartPlate* plate = NULL;

	plate = get_plate(index);
	if (!plate)
	{
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":can not get plate for index %1%, size %2%") % index % m_plate_list.size();
		return -1;
	}
	BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":lock plate %1%, to state %2%") % index % state;

	plate->lock(state);

	return ret;
}

//find plate by print index, return -1 if not found
int PartPlateList::find_plate_by_print_index(int print_index)
{
	int plate_index = -1;

	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		PartPlate* plate = m_plate_list[i];

		if (plate->m_print_index == print_index)
		{
			plate_index = i;
			break;
		}
	}

	return plate_index;
}

/*instance related operations*/
//find instance in which plate, return -1 when not found
//this function only judges whether it is intersect with plate
int PartPlateList::find_instance(int obj_id, int instance_id)
{
	int ret = -1;

	//update the plates after it
	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		PartPlate* plate = m_plate_list[i];
		assert(plate != NULL);

		if (plate->contain_instance(obj_id, instance_id))
			return i;
	}

	//return -1 for not found
	return ret;
}

/*instance related operations*/
//find instance in which plate, return -1 when not found
//this function only judges whether it is intersect with plate
int PartPlateList::find_instance(BoundingBoxf3& bounding_box)
{
	int ret = -1;

	//update the plates after it
	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		PartPlate* plate = m_plate_list[i];
		assert(plate != NULL);

		if (plate->intersects(bounding_box))
			return i;
	}

	//return -1 for not found
	return ret;
}

//this function not only judges whether it is intersect with plate, but also judges whether it is fully included in plate
//returns -1 when can not find any plate
int PartPlateList::find_instance_belongs(int obj_id, int instance_id)
{
	int ret = -1;

	//update the plates after it
	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		PartPlate* plate = m_plate_list[i];
		assert(plate != NULL);

		if (plate->contain_instance_totally(obj_id, instance_id))
			return i;
	}

	//return -1 for not found
	return ret;
}

//notify instance's update, need to refresh the instance in plates
//newly added or modified
int PartPlateList::notify_instance_update(int obj_id, int instance_id, bool is_new)
{
	int ret = 0, index;
	PartPlate* plate = NULL;
	ModelObject* object = NULL;

	if ((obj_id >= 0) && (obj_id < m_model->objects.size()))
	{
		object = m_model->objects[obj_id];
	}
	else if (obj_id >= 1000 && obj_id < 1000 + m_plate_count) {
		//wipe tower updates
		PartPlate* plate = m_plate_list[obj_id - 1000];
		plate->update_slice_result_valid_state( false );
		plate->thumbnail_data.reset();
        plate->no_light_thumbnail_data.reset();
		plate->top_thumbnail_data.reset();
		plate->pick_thumbnail_data.reset();

		return 0;
	}
    else
		return -1;

	BoundingBoxf3 boundingbox = object->instance_convex_hull_bounding_box(instance_id);

	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": obj_id %1%, instance_id %2%") % obj_id % instance_id;
	index = find_instance(obj_id, instance_id);
	if (index != -1)
	{
		//found it added before
		BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": found it in previous plate %1%") % index;
		plate = m_plate_list[index];
		if (!plate->intersect_instance(obj_id, instance_id, &boundingbox))
		{
			//not include anymore, remove it from original plate
			BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": not in plate %1% anymore, remove it") % index;
			plate->remove_instance(obj_id, instance_id);
		}
		else
		{
			BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": still in original plate %1%, no need to be updated") % index;
			plate->update_instance_exclude_status(obj_id, instance_id, &boundingbox);
			plate->update_states();
			plate->update_slice_result_valid_state();
			plate->thumbnail_data.reset();
            plate->no_light_thumbnail_data.reset();
			plate->top_thumbnail_data.reset();
			plate->pick_thumbnail_data.reset();
			return 0;
		}
		plate->update_slice_result_valid_state();
		plate->thumbnail_data.reset();
        plate->no_light_thumbnail_data.reset();
		plate->top_thumbnail_data.reset();
		plate->pick_thumbnail_data.reset();
	}
	else if (unprintable_plate.contain_instance(obj_id, instance_id))
	{
		//found it in the unprintable plate
		BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": found it in unprintable plate");
		if (!unprintable_plate.intersect_instance(obj_id, instance_id, &boundingbox))
		{
			//not include anymore, remove it from original plate
			BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": not in unprintable plate anymore, remove it");
			unprintable_plate.remove_instance(obj_id, instance_id);
		}
		else
		{
			BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": still in unprintable plate, no need to be updated");
			return 0;
		}
	}

	auto is_object_config_compatible_with_spiral_vase = [](ModelObject* object) {
		const DynamicPrintConfig& config = object->config.get();
		if (config.has("wall_loops") && config.opt_int("wall_loops") == 1 &&
			config.has("top_shell_layers") && config.opt_int("top_shell_layers") == 0 &&
			config.has("sparse_infill_density") && config.option<ConfigOptionPercent>("sparse_infill_density")->value == 0 &&
			config.has("enable_support") && !config.opt_bool("enable_support") &&
			config.has("enforce_support_layers") && config.opt_int("enforce_support_layers") == 0 &&
			config.has("ensure_vertical_shell_thickness") && config.opt_bool("ensure_vertical_shell_thickness") &&
			config.has("detect_thin_wall") && !config.opt_bool("detect_thin_wall") &&
			config.has("timelapse_type") && config.opt_enum<TimelapseType>("timelapse_type") == TimelapseType::tlTraditional)
			return true;
		else
			return false;
	};

	//try to find a new plate
	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		PartPlate* plate = m_plate_list[i];
		assert(plate != NULL);

		if (plate->intersect_instance(obj_id, instance_id, &boundingbox))
		{
			//found a new plate, add it to plate
			plate->add_instance(obj_id, instance_id, false, &boundingbox);

			// spiral mode, update object setting
			if (plate->config()->has("spiral_mode") && plate->config()->opt_bool("spiral_mode") && !is_object_config_compatible_with_spiral_vase(object)) {
				if (!is_new) {
					auto answer = static_cast<TabPrintPlate*>(AppAdapter::gui_app()->plate_tab)->show_spiral_mode_settings_dialog(true);
					if (answer == wxID_YES) {
						plate->set_vase_mode_related_object_config(obj_id);
					}
				}
				else {
					plate->set_vase_mode_related_object_config(obj_id);
				}
			}

			plate->update_slice_result_valid_state();
			plate->thumbnail_data.reset();
            plate->no_light_thumbnail_data.reset();
			plate->top_thumbnail_data.reset();
			plate->pick_thumbnail_data.reset();
			BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": add it to new plate %1%") % i;
			return 0;
		}
	}

	if (unprintable_plate.intersect_instance(obj_id, instance_id, &boundingbox))
	{
		//found in unprintable plate, add it to plate
		unprintable_plate.add_instance(obj_id, instance_id, false, &boundingbox);
		BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": add it to unprintable plate");
		return 0;
	}

	return 0;
}

//notify instance is removed
int PartPlateList::notify_instance_removed(int obj_id, int instance_id)
{
	int ret = 0, index, instance_to_delete = instance_id;
	PartPlate* plate = NULL;

	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": obj_id %1%, instance_id %2%") % obj_id % instance_id;
	if (instance_id == -1) {
		instance_to_delete = 0;
	}
	index = find_instance(obj_id, instance_to_delete);
	if (index != -1)
	{
		//found it added before
		BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": found it in plate %1%, remove it") % index;
		plate = m_plate_list[index];
		plate->remove_instance(obj_id, instance_to_delete);
		plate->update_slice_result_valid_state();
		plate->thumbnail_data.reset();
        plate->no_light_thumbnail_data.reset();
		plate->top_thumbnail_data.reset();
		plate->pick_thumbnail_data.reset();
	}

	if (unprintable_plate.contain_instance(obj_id, instance_to_delete))
	{
		//found in unprintable plate, add it to plate
		unprintable_plate.remove_instance(obj_id, instance_to_delete);
		BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": found it in unprintable plate, remove it");
	}

	if (instance_id == -1) {
		//update all the obj_ids which is bigger
		for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
		{
			PartPlate* plate = m_plate_list[i];
			assert(plate != NULL);

			plate->update_object_index(obj_id, m_model->objects.size());
		}
		unprintable_plate.update_object_index(obj_id, m_model->objects.size());
	}

	return 0;
}

//add instance to special plate, need to remove from the original plate
//called from the right-mouse menu when a instance selected
int PartPlateList::add_to_plate(int obj_id, int instance_id, int plate_id)
{
	int ret = 0, index;
	PartPlate* plate = NULL;

	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": plate_id %1%, found obj_id %2%, instance_id %3%") % plate_id % obj_id % instance_id;
	index = find_instance(obj_id, instance_id);
	if (index != -1)
	{
		//found it added before
		BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": found it in previous plate %1%") % index;
		if (index != plate_id)
		{
			//remove it from original plate first
			plate = m_plate_list[index];
			plate->remove_instance(obj_id, instance_id);
		}
		else
		{
			BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": already in this plate, no need to be added");
			return 0;
		}
	}
	else
	{
		BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": not added to plate before, add it to center");
	}
	plate = get_plate(plate_id);
	if (!plate)
	{
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":can not get plate for index %1%, size %2%") % index % m_plate_list.size();
		return -1;
	}
	ret = plate->add_instance(obj_id, instance_id, true);

	return ret;
}

//reload all objects
int PartPlateList::reload_all_objects(bool except_locked, int plate_index)
{
	int ret = 0;
	unsigned int i, j, k;

	clear(false, false, except_locked, plate_index);

	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": m_model->objects.size() is %1%") % m_model->objects.size();
	//try to find a new plate
	for (i = 0; i < (unsigned int)m_model->objects.size(); ++i)
	{
		ModelObject* object = m_model->objects[i];
		for (j = 0; j < (unsigned int)object->instances.size(); ++j)
		{
			ModelInstance* instance = object->instances[j];
			BoundingBoxf3 boundingbox = object->instance_convex_hull_bounding_box(j);
			for (k = 0; k < (unsigned int)m_plate_list.size(); ++k)
			{
				PartPlate* plate = m_plate_list[k];
				assert(plate != NULL);

				if (plate->intersect_instance(i, j, &boundingbox))
				{
					//found a new plate, add it to plate
					plate->add_instance(i, j, false, &boundingbox);
					BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": found plate_id %1%, for obj_id %2%, instance_id %3%") % k % i % j;

					break;
				}
			}

			if ((k == m_plate_list.size()) && (unprintable_plate.intersect_instance(i, j, &boundingbox)))
			{
				//found in unprintable plate, add it to plate
				unprintable_plate.add_instance(i, j, false, &boundingbox);
				BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": found in unprintable plate, obj_id %1%, instance_id %2%") % i % j;
			}
		}

	}

	return ret;
}

//reload objects for newly created plate
int PartPlateList::construct_objects_list_for_new_plate(int plate_index)
{
	int ret = 0;
	unsigned int i, j, k;
	PartPlate* new_plate = m_plate_list[plate_index];
	bool already_included;

	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": m_model->objects.size() is %1%") % m_model->objects.size();
	unprintable_plate.clear();
	//try to find a new plate
	for (i = 0; i < (unsigned int)m_model->objects.size(); ++i)
	{
		ModelObject* object = m_model->objects[i];
		for (j = 0; j < (unsigned int)object->instances.size(); ++j)
		{
			ModelInstance* instance = object->instances[j];
			already_included = false;

			for (k = 0; k < (unsigned int)plate_index; ++k)
			{
				PartPlate* plate = m_plate_list[k];
				if (plate->contain_instance(i, j))
				{
					already_included = true;
					break;
				}
			}

			if (already_included)
				continue;

			BoundingBoxf3 boundingbox = object->instance_convex_hull_bounding_box(j);
			if (new_plate->intersect_instance(i, j, &boundingbox))
			{
				//found a new plate, add it to plate
				ret |= new_plate->add_instance(i, j, false, &boundingbox);
				BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": added to plate_id %1%, for obj_id %2%, instance_id %3%") % plate_index % i % j;

				continue;
			}

			if ( (unprintable_plate.intersect_instance(i, j, &boundingbox)))
			{
				//found in unprintable plate, add it to plate
				unprintable_plate.add_instance(i, j, false, &boundingbox);
				BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": found in unprintable plate, obj_id %1%, instance_id %2%") % i % j;
			}
		}
	}

	return ret;
}


//compute the plate index
int PartPlateList::compute_plate_index(arrangement::ArrangePolygon& arrange_polygon)
{
	int row, col;

	float col_value = (unscale<double>(arrange_polygon.translation(X))) / plate_stride_x();
	float row_value = (plate_stride_y() - unscale<double>(arrange_polygon.translation(Y))) / plate_stride_y();

	row = round(row_value);
	col = round(col_value);

	return row * m_plate_cols + col;
}

//preprocess a arrangement::ArrangePolygon, return true if it is in a locked plate
bool PartPlateList::preprocess_arrange_polygon(int obj_index, int instance_index, arrangement::ArrangePolygon& arrange_polygon, bool selected)
{
	bool locked = false;
	int lockplate_cnt = 0;

	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		if (m_plate_list[i]->contain_instance(obj_index, instance_index))
		{
			if (m_plate_list[i]->is_locked())
			{
				locked = true;
				arrange_polygon.bed_idx = i;
				arrange_polygon.row = i / m_plate_cols;
				arrange_polygon.col = i % m_plate_cols;
				arrange_polygon.translation(X) -= scaled<double>(plate_stride_x() * arrange_polygon.col);
				arrange_polygon.translation(Y) += scaled<double>(plate_stride_y() * arrange_polygon.row);
			}
			else
			{
				if (!selected)
				{
					//will be treated as fixeditem later
					arrange_polygon.bed_idx = i - lockplate_cnt;
					arrange_polygon.row = i / m_plate_cols;
					arrange_polygon.col = i % m_plate_cols;
					arrange_polygon.translation(X) -= scaled<double>(plate_stride_x() * arrange_polygon.col);
					arrange_polygon.translation(Y) += scaled<double>(plate_stride_y() * arrange_polygon.row);
				}
			}
			BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": obj_id %1% instance_id %2% already in plate %3%, locked %4%, row %5%, col %6%\n") % obj_index % instance_index % i % locked % arrange_polygon.row % arrange_polygon.col;
			return locked;
		}
		if (m_plate_list[i]->is_locked())
			lockplate_cnt++;
	}
	//not be contained by any plates
	if (!selected)
		arrange_polygon.bed_idx = PartPlateList::MAX_PLATES_COUNT;

	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": not in any plates, bed_idx %1%, translation(x) %2%, (y) %3%") % arrange_polygon.bed_idx % unscale<double>(arrange_polygon.translation(X)) % unscale<double>(arrange_polygon.translation(Y));

	return locked;
}

//preprocess a arrangement::ArrangePolygon, return true if it is not in current plate
bool PartPlateList::preprocess_arrange_polygon_other_locked(int obj_index, int instance_index, arrangement::ArrangePolygon& arrange_polygon, bool selected)
{
	bool locked = false;

	if (selected)
	{
		//arrange_polygon.translation(X) -= scaled<double>(plate_stride_x() * m_current_plate);
	}
	else
	{
		locked = true;
		for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
		{
			if (m_plate_list[i]->contain_instance(obj_index, instance_index))
			{
				arrange_polygon.bed_idx = i;
				arrange_polygon.row = i / m_plate_cols;
				arrange_polygon.col = i % m_plate_cols;
				arrange_polygon.translation(X) -= scaled<double>(plate_stride_x() * arrange_polygon.col);
				arrange_polygon.translation(Y) += scaled<double>(plate_stride_y() * arrange_polygon.row);
				//BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": obj_id %1% instance_id %2% in plate %3%, locked %4%, row %5%, col %6%\n") % obj_index % instance_index % i % locked % arrange_polygon.row % arrange_polygon.col;
				return locked;
			}
		}
		arrange_polygon.bed_idx = PartPlateList::MAX_PLATES_COUNT;
	}
	return locked;
}

bool PartPlateList::preprocess_exclude_areas(arrangement::ArrangePolygons& unselected, int num_plates, float inflation)
{
	bool added = false;

	for (auto& exclude_areas : m_exclude_areas_group)
	{
		if (exclude_areas.size() > 0)
		{
			//has exclude areas
			PartPlate* plate = m_plate_list[0];
			std::vector<BoundingBoxf3>& exclude_bounding_box = plate->get_exclude_bounding_box();
			for (int index = 0; index < exclude_bounding_box.size(); index++)
			{
				Polygon ap({
					{scaled(exclude_bounding_box[index].min.x()), scaled(exclude_bounding_box[index].min.y())},
					{scaled(exclude_bounding_box[index].max.x()), scaled(exclude_bounding_box[index].min.y())},
					{scaled(exclude_bounding_box[index].max.x()), scaled(exclude_bounding_box[index].max.y())},
					{scaled(exclude_bounding_box[index].min.x()), scaled(exclude_bounding_box[index].max.y())}
					});

				for (int j = 0; j < num_plates; j++)
				{
					arrangement::ArrangePolygon ret;
					ret.poly.contour = ap;
					ret.translation = Vec2crd(0, 0);
					ret.rotation = 0.0f;
					ret.is_virt_object = true;
					ret.bed_idx = j;
					ret.height = 1;
					ret.name = "ExcludedRegion" + std::to_string(index);
					ret.inflation = inflation;

					unselected.emplace_back(std::move(ret));
				}
				added = true;
			}
		}
	}


	return added;
}

bool PartPlateList::preprocess_nonprefered_areas(arrangement::ArrangePolygons& regions, int num_plates, float inflation)
{
	bool added = false;

	std::vector<BoundingBoxf> nonprefered_regions;
	nonprefered_regions.emplace_back(Vec2d{ 18,0 }, Vec2d{ 240,15 }); // new extrusion & hand-eye calibration region

	//has exclude areas
	PartPlate* plate = m_plate_list[0];
	for (int index = 0; index < nonprefered_regions.size(); index++)
	{
		Polygon ap = scaled(nonprefered_regions[index]).polygon();
		for (int j = 0; j < num_plates; j++)
		{
			arrangement::ArrangePolygon ret;
			ret.poly.contour = ap;
			ret.translation = Vec2crd(0, 0);
			ret.rotation = 0.0f;
			ret.is_virt_object = true;
            ret.is_extrusion_cali_object = true;
			ret.bed_idx = j;
			ret.height = 1;
			ret.name = "NonpreferedRegion" + std::to_string(index);
			ret.inflation = inflation;

			regions.emplace_back(std::move(ret));
		}
		added = true;
	}
	return added;
}


//postprocess an arrangement::ArrangePolygon's bed index
void PartPlateList::postprocess_bed_index_for_selected(arrangement::ArrangePolygon& arrange_polygon)
{
	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": bed_idx %1%, locked_plate %2%, translation(x) %3%, (y) %4%") % arrange_polygon.bed_idx % arrange_polygon.locked_plate % unscale<double>(arrange_polygon.translation(X)) % unscale<double>(arrange_polygon.translation(Y));

	if (arrange_polygon.bed_idx == -1)
	{
		//outarea for large object, can not process here for the plate number maybe increased later
		BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(": can not be arranged inside plate!");
		return;
	}

	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		if (m_plate_list[i]->is_locked())
		{
			BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": found locked_plate %1%, increate index by 1") % i;
			//arrange_polygon.translation(X) += scaled<double>(plate_stride_x());
			arrange_polygon.bed_idx += 1;
			//offset_x += scaled<double>(plate_stride_x());
		}
		else
		{
			//judge whether it is at the left side of the plate border
			if (arrange_polygon.bed_idx <= i)
			{
				BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(":found in plate_index %1%, bed_idx %2%") % i % arrange_polygon.bed_idx;
				return;
			}
		}
	}

	//create a new plate which can hold this arrange_polygon
	int plate_index = create_plate(false);

	while (plate_index != -1)
	{
		if (arrange_polygon.bed_idx <= plate_index)
		{
			BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(":new plate_index %1%, matches bed_idx %2%") % plate_index % arrange_polygon.bed_idx;
			break;
		}

		plate_index = create_plate(false);
	}

	return;
}

//postprocess an arrangement::ArrangePolygon's bed index
void PartPlateList::postprocess_bed_index_for_unselected(arrangement::ArrangePolygon& arrange_polygon)
{
	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": bed_idx %1%, locked_plate %2%, translation(x) %3%, (y) %4%") % arrange_polygon.bed_idx % arrange_polygon.locked_plate % unscale<double>(arrange_polygon.translation(X)) % unscale<double>(arrange_polygon.translation(Y));

	if (arrange_polygon.bed_idx == PartPlateList::MAX_PLATES_COUNT)
		return;

	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		if (m_plate_list[i]->is_locked())
		{
			BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": found locked_plate %1%, increate index by 1") % i;
			//arrange_polygon.translation(X) += scaled<double>(plate_stride_x());
			arrange_polygon.bed_idx += 1;
			//offset_x += scaled<double>(plate_stride_x());
		}
		else
		{
			//judge whether it is at the left side of the plate border
			if (arrange_polygon.bed_idx <= i)
			{
				BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(":found in plate_index %1%, bed_idx %2%") % i % arrange_polygon.bed_idx;
				return;
			}
		}
	}

	return;
}

//postprocess an arrangement::ArrangePolygon, other instances are under locked states
void PartPlateList::postprocess_bed_index_for_current_plate(arrangement::ArrangePolygon& arrange_polygon)
{
	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": bed_idx %1%, locked_plate %2%, translation(x) %3%, (y) %4%") % arrange_polygon.bed_idx % arrange_polygon.locked_plate % unscale<double>(arrange_polygon.translation(X)) % unscale<double>(arrange_polygon.translation(Y));

	if (arrange_polygon.bed_idx == -1)
	{
		//outarea for large object
		BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(": can not be arranged inside plate!");
	}
	else if (arrange_polygon.bed_idx == 0)
		arrange_polygon.bed_idx += m_current_plate;
	else
		arrange_polygon.bed_idx = m_plate_list.size();

	return;
}

//postprocess an arrangement::ArrangePolygon
void PartPlateList::postprocess_arrange_polygon(arrangement::ArrangePolygon& arrange_polygon, bool selected)
{
	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": bed_idx %1%, selected %2%, translation(x) %3%, (y) %4%") % arrange_polygon.bed_idx % selected % unscale<double>(arrange_polygon.translation(X)) % unscale<double>(arrange_polygon.translation(Y));

	if ((selected) || (arrange_polygon.bed_idx != PartPlateList::MAX_PLATES_COUNT))
	{
		if (arrange_polygon.bed_idx == -1)
		{
			// outarea for large object
			arrange_polygon.bed_idx = m_plate_list.size();
			BoundingBox apbox = get_extents(arrange_polygon.transformed_poly());  // the item may have been rotated
			auto        apbox_size = apbox.size();

			arrange_polygon.translation(X) = 0.5 * apbox_size[0];
			arrange_polygon.translation(Y) = scaled<double>(static_cast<double>(m_plate_depth)) - 0.5 * apbox_size[1];
		}

		arrange_polygon.row = arrange_polygon.bed_idx / m_plate_cols;
		arrange_polygon.col = arrange_polygon.bed_idx % m_plate_cols;
		arrange_polygon.translation(X) += scaled<double>(plate_stride_x() * arrange_polygon.col);
		arrange_polygon.translation(Y) -= scaled<double>(plate_stride_y() * arrange_polygon.row);
	}

	return;
}

/*rendering related functions*/
//render
void PartPlateList::render(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom, bool only_current, bool only_body, int hover_id, bool render_cali, bool show_grid)
{
	const std::lock_guard<std::mutex> local_lock(m_plates_mutex);
	std::vector<PartPlate*>::iterator it = m_plate_list.begin();

	int plate_hover_index = -1;
	int plate_hover_action = -1;
	if (hover_id != -1) {
		plate_hover_index = hover_id / PlateBed::GRABBER_COUNT;
		plate_hover_action = hover_id % PlateBed::GRABBER_COUNT;
	}

	PlateBed::generate_icon_textures();
	for (it = m_plate_list.begin(); it != m_plate_list.end(); it++) {
		int current_index = (*it)->get_index();
		if (only_current && (current_index != m_current_plate))
			continue;
		if (current_index == m_current_plate) {
			PartPlate::HeightLimitMode height_mode = (only_current)?PartPlate::HEIGHT_LIMIT_NONE:m_height_limit_mode;
			if (plate_hover_index == current_index)
                (*it)->render(view_matrix, projection_matrix, bottom, only_body, false, height_mode, plate_hover_action, render_cali, show_grid);
			else
                (*it)->render(view_matrix, projection_matrix, bottom, only_body, false, height_mode, -1, render_cali, show_grid);
		}
		else {
			if (plate_hover_index == current_index)
				(*it)->render(view_matrix, projection_matrix, bottom, only_body, false, PartPlate::HEIGHT_LIMIT_NONE, plate_hover_action, render_cali, show_grid);
			else
                (*it)->render(view_matrix, projection_matrix, bottom, only_body, false, PartPlate::HEIGHT_LIMIT_NONE, -1, render_cali, show_grid);
		}
	}
}

int PartPlateList::select_plate_by_obj(int obj_index, int instance_index)
{
	int ret = 0, index;
	PartPlate* plate = NULL;

	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": obj_id %1%, instance_id %2%") % obj_index % instance_index;
	index = find_instance(obj_index, instance_index);
	if (index != -1)
	{
		//found it in plate
		BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": found it in plate %1%") % index;
		select_plate(index);
		return 0;
	}
	return -1;
}

void PartPlateList::calc_bounding_boxes()
{
	m_bounding_box.reset();
	std::vector<PartPlate*>::iterator it = m_plate_list.begin();
	for (it = m_plate_list.begin(); it != m_plate_list.end(); it++) {
		m_bounding_box.merge((*it)->get_bounding_box(true));
	}
}

void PartPlateList::select_plate_view()
{
	if (m_current_plate < 0 || m_current_plate >= m_plate_list.size()) return;

	Vec3d target = m_plate_list[m_current_plate]->get_bounding_box(false).center();
	Vec3d position(target.x(), target.y(), m_plater->get_camera().get_distance());
	m_plater->get_camera().look_at(position, target, Vec3d::UnitY());
	m_plater->get_camera().select_view("topfront");
}

bool PartPlateList::set_shapes(const std::vector<Pointfs>& shape_group,
	const  std::vector<Pointfs>& exclude_areas_group,
	const std::string& texture_filename,
	float height_to_lid,
	float height_to_rod)
{
	const std::lock_guard<std::mutex> local_lock(m_plates_mutex);
	m_shape_group = shape_group;
	m_exclude_areas_group = exclude_areas_group;
	m_height_to_lid = height_to_lid;
	m_height_to_rod = height_to_rod;

	double stride_x = plate_stride_x();
	double stride_y = plate_stride_y();
	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		PartPlate* plate = m_plate_list[i];
		assert(plate != NULL);

		Vec2d pos;

		pos = compute_shape_position(i, m_plate_cols);
		plate->set_shape(shape_group, exclude_areas_group, pos, height_to_lid, height_to_rod);
	}
	// is_load_bedtype_textures = false;//reload textures
	calc_bounding_boxes();

	PlateBed::update_logo_texture_filename(texture_filename);

	return true;
}

/*slice related functions*/
//update current slice context into backgroud slicing process
void PartPlateList::update_slice_context_to_current_plate(BackgroundSlicingProcess& process)
{
	PartPlate* current_plate;

	current_plate = m_plate_list[m_current_plate];
	assert(current_plate != NULL);

	current_plate->update_slice_context(process);

	return;
}

GCodeResultWrapper* PartPlateList::get_current_slice_result_wrapper() const
{
	PartPlate* current_plate;

	current_plate = m_plate_list[m_current_plate];
	BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":m_current_plate %1%, current_plate %2%") % m_current_plate % current_plate;
	assert(current_plate != NULL);

	return current_plate->get_slice_result_wrapper();
}

//invalid all the plater's slice result
void PartPlateList::invalid_all_slice_result()
{
	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": plates count %1%") % m_plate_list.size();
	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		m_plate_list[i]->update_slice_result_valid_state(false);
	}

	return;
}

//check whether all plates's slice result valid
bool PartPlateList::is_all_slice_results_valid() const
{
	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		if (!m_plate_list[i]->is_slice_result_valid())
			return false;
	}
	return true;
}

//check whether all plates's slice result valid for print
bool PartPlateList::is_all_slice_results_ready_for_print() const
{
    bool res = false;

    for (unsigned int i = 0; i < (unsigned int) m_plate_list.size(); ++i) {
        if (!m_plate_list[i]->empty()) {
            if (m_plate_list[i]->is_all_instances_unprintable()) {
				continue;
			}
            if (!m_plate_list[i]->is_slice_result_ready_for_print()) {
				return false;
			}
        }
        if (m_plate_list[i]->is_slice_result_ready_for_print()) {
			res = true;
		}
    }

    return res;
}

//check whether all plates' slice result valid for export to file
bool PartPlateList::is_all_slice_result_ready_for_export() const
{
	bool res = false;

    for (unsigned int i = 0; i < (unsigned int) m_plate_list.size(); ++i) {
        if (!m_plate_list[i]->empty()) {
            if (m_plate_list[i]->is_all_instances_unprintable()) {
				continue;
			}
            if (!m_plate_list[i]->is_slice_result_ready_for_print()) {
				return false;
			}
        }
        if (m_plate_list[i]->is_slice_result_ready_for_print()) {
			if (!m_plate_list[i]->has_printable_instances()) {
				return false;
			}
			res = true;
		}
    }

    return res;
}

//check whether all plates ready for slice
bool PartPlateList::is_all_plates_ready_for_slice() const
{
    for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		if (m_plate_list[i]->can_slice())
			return true;
	}
	return false;
}

void PartPlateList::get_sliced_result(std::vector<bool>& sliced_result, std::vector<std::string>& gcode_paths)
{
	sliced_result.resize(m_plate_list.size());
	gcode_paths.resize(m_plate_list.size());
}
//rebuild data which are not serialized after de-serialize
int PartPlateList::rebuild_plates_after_deserialize(std::vector<bool>& previous_sliced_result, std::vector<std::string>& previous_gcode_paths)
{
	int ret = 0;

	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": plates count %1%") % m_plate_list.size();
	// SoftFever: assign plater info first
    for (auto partplate : m_plate_list) {
        partplate->m_plater = this->m_plater;
        partplate->m_partplate_list = this;
        partplate->m_model = this->m_model;
    }
	update_plate_cols();
	update_all_plates_pos_and_size(false, false, false, false);
	set_shapes(m_shape_group, m_exclude_areas_group, m_logo_texture_filename, m_height_to_lid, m_height_to_rod);
	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		bool need_reset_print = false;
		//check the previous sliced result
		if (m_plate_list[i]->m_slice_result_valid) {
			if ((i >= previous_sliced_result.size()) || !previous_sliced_result[i])
				m_plate_list[i]->update_slice_result_valid_state(false);
		}

		GCodeResultWrapper* gcode_result_wrapper = new GCodeResultWrapper(m_model);
		gcode_result_wrapper->resize(m_plate_area_count);
		m_plate_list[i]->set_print(gcode_result_wrapper, m_print_index);
		m_print_index++;
	}

	return ret;
}

//retruct plates structures after auto-arrangement
int PartPlateList::rebuild_plates_after_arrangement(bool recycle_plates, bool except_locked, int plate_index)
{
	int ret = 0;

	BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":before rebuild, plates count %1%, recycle_plates %2%") % m_plate_list.size() % recycle_plates;

	// sort by arrange_order
	std::sort(m_model->objects.begin(), m_model->objects.end(), [](auto a, auto b) {return a->instances[0]->arrange_order < b->instances[0]->arrange_order; });

	ret = reload_all_objects(except_locked, plate_index);

	if (recycle_plates)
	{
		for (unsigned int i = (unsigned int)m_plate_list.size() - 1; i > 0; --i)
		{
			if (m_plate_list[i]->empty()
				|| !m_plate_list[i]->has_printable_instances())
			{
				//delete it
				BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":delete plate %1% for empty") % i;
				delete_plate(i);
			}
			else if (m_plate_list[i]->is_locked()) {
				continue;
			}
			else
			{
				break;
			}
		}
	}

	BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":after rebuild, plates count %1%") % m_plate_list.size();
	return ret;
}

int PartPlateList::store_to_3mf_structure(PlateDataPtrs& plate_data_list, bool with_slice_info, int plate_idx)
{
	int ret = 0;

	plate_data_list.clear();
	plate_data_list.reserve(m_plate_list.size());
	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		PlateData* plate_data_item = new PlateData();
		PartPlate* plate = m_plate_list[i];
		plate_data_item->locked = plate->m_locked;
		plate_data_item->plate_index = plate->m_plate_index;
		plate_data_item->plate_name = plate->get_plate_name();
		BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": plate %1% before load, width %2%, height %3%, size %4%!")
			%(i+1) % plate->thumbnail_data.width % plate->thumbnail_data.height %plate->thumbnail_data.pixels.size();
		plate_data_item->plate_thumbnail.load_from(plate->thumbnail_data);
		BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": plate %1% after load, width %2%, height %3%, size %4%!")
			%(i+1) %plate_data_item->plate_thumbnail.width %plate_data_item->plate_thumbnail.height %plate_data_item->plate_thumbnail.pixels.size();
		plate_data_item->config.apply(*plate->config());

		if (plate->no_light_thumbnail_data.is_valid())
			plate_data_item->no_light_thumbnail_file = "valid_no_light";
		if (plate->top_thumbnail_data.is_valid())
			plate_data_item->top_file = "valid_top";
		if (plate->pick_thumbnail_data.is_valid())
			plate_data_item->pick_file = "valid_pick";

		if (plate->obj_to_instance_set.size() > 0)
		{
			for (std::set<std::pair<int, int>>::iterator it = plate->obj_to_instance_set.begin(); it != plate->obj_to_instance_set.end(); ++it)
				plate_data_item->objects_and_instances.emplace_back(it->first, it->second);
		}

		BOOST_LOG_TRIVIAL(info) << __FUNCTION__ <<boost::format(": plate %1%, gcode_filename=%2%, with_slice_info=%3%, slice_valid %4%, object item count %5%.")
			%i % plate->m_gcode_result->filename() % with_slice_info % plate->is_slice_result_valid()%plate_data_item->objects_and_instances.size();

		if (with_slice_info) {
			GCodeResultWrapper* slice_result_wrapper = plate->get_slice_result_wrapper();
			if (slice_result_wrapper && slice_result_wrapper->is_valid() /*&& plate->is_slice_result_valid()*/) {
				// BBS only include current palte_idx
				if (plate_idx == i || plate_idx == PLATE_CURRENT_IDX || plate_idx == PLATE_ALL_IDX) {
					//load calibration thumbnail
					if (plate->cali_bboxes_data.is_valid())
						plate_data_item->pattern_bbox_file = "valid_pattern_bbox";

					plate_data_item->gcode_file = plate->m_gcode_result->get_result()->filename;
					plate_data_item->sub_gcode_files.clear();
					for (int i = 0, count = plate->m_gcode_result->size(); i < count; ++i)
						plate_data_item->sub_gcode_files.push_back(plate->m_gcode_result->get_result(i)->filename);

					plate_data_item->is_sliced_valid  = true;
					plate_data_item->gcode_prediction = std::to_string(
						(int)plate->get_slice_result_wrapper()->max_time(static_cast<size_t>(PrintEstimatedStatistics::ETimeMode::Normal)));
					plate_data_item->toolpath_outside = plate->m_gcode_result->toolpath_outside();
                    plate_data_item->timelapse_warning_code = plate->m_gcode_result->timelapse_warning_code();
					plate->set_timelapse_warning_code(plate_data_item->timelapse_warning_code);
					plate_data_item->is_label_object_enabled = plate->m_gcode_result->label_object_enabled();
					plate->get_print(nullptr, nullptr);

					//parse filament info
					plate_data_item->parse_filament_info(plate->get_slice_result_wrapper()->get_result());
				}
			}
		}

		plate_data_list.push_back(plate_data_item);
	}
	BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":stored %1% plates!") % m_plate_list.size();

	return ret;
}

int PartPlateList::load_from_3mf_structure(PlateDataPtrs& plate_data_list)
{
	int ret = 0;

	if (plate_data_list.size() <= 0)
	{
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":no plates, should not happen!");
		return -1;
	}
	clear(true, true);
	for (unsigned int i = 0; i < (unsigned int)plate_data_list.size(); ++i)
	{
		int index = create_plate(false);
		m_plate_list[index]->m_locked = plate_data_list[i]->locked;
		m_plate_list[index]->config()->apply(plate_data_list[i]->config);
		m_plate_list[index]->set_plate_name(plate_data_list[i]->plate_name);
		if (plate_data_list[i]->plate_index != index)
		{
			BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(":plate index %1% seems invalid, skip it")% plate_data_list[i]->plate_index;
		}
		BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": plate %1%, gcode_file %2%, is_sliced_valid %3%, toolpath_outside %4%, is_support_used %5% is_label_object_enabled %6%")
			%i %plate_data_list[i]->gcode_file %plate_data_list[i]->is_sliced_valid %plate_data_list[i]->toolpath_outside %plate_data_list[i]->is_support_used %plate_data_list[i]->is_label_object_enabled;
		//load object and instance from 3mf
		//just test for file correct or not, we will rebuild later
		/*for (std::vector<std::pair<int, int>>::iterator it = plate_data_list[i]->objects_and_instances.begin(); it != plate_data_list[i]->objects_and_instances.end(); ++it)
			m_plate_list[index]->obj_to_instance_set.insert(std::pair(it->first, it->second));*/
		if (!plate_data_list[i]->gcode_file.empty()) {
			m_plate_list[index]->m_gcode_path_from_3mf = plate_data_list[i]->gcode_file;
		}
		m_plate_list[index]->m_area_gcode_files_from_3mf.clear();
		for (std::string& file : plate_data_list[i]->sub_gcode_files)
			m_plate_list[index]->m_area_gcode_files_from_3mf.push_back(file);

		GCodeResultWrapper* gcode_result = nullptr;
		m_plate_list[index]->get_print(&gcode_result, nullptr);
		PrintStatistics ps;
		gcode_result->print_statistics().modes[static_cast<size_t>(PrintEstimatedStatistics::ETimeMode::Normal)].time = atoi(plate_data_list[i]->gcode_prediction.c_str());
		ps.total_weight = atof(plate_data_list[i]->gcode_weight.c_str());
		ps.total_used_filament = 0.f;
		for (auto filament_item: plate_data_list[i]->slice_filaments_info)
		{
			ps.total_used_filament += filament_item.used_m;
		}
		ps.total_used_filament *= 1000; //koef
		gcode_result->toolpath_outside() = plate_data_list[i]->toolpath_outside;
		gcode_result->label_object_enabled() = plate_data_list[i]->is_label_object_enabled;
        gcode_result->timelapse_warning_code() = plate_data_list[i]->timelapse_warning_code;
        m_plate_list[index]->set_timelapse_warning_code(plate_data_list[i]->timelapse_warning_code);
		m_plate_list[index]->slice_filaments_info = plate_data_list[i]->slice_filaments_info;
		gcode_result->warnings() = plate_data_list[i]->warnings;
		if (m_plater && !plate_data_list[i]->thumbnail_file.empty()) {
			BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": plate %1%, load thumbnail from %2%.")%(i+1) %plate_data_list[i]->thumbnail_file;
			if (boost::filesystem::exists(plate_data_list[i]->thumbnail_file)) {
				m_plate_list[index]->load_thumbnail_data(plate_data_list[i]->thumbnail_file, m_plate_list[index]->thumbnail_data);
				BOOST_LOG_TRIVIAL(info) << __FUNCTION__ <<boost::format(": plate %1% after load, width %2%, height %3%, size %4%!")
					%(i+1) %m_plate_list[index]->thumbnail_data.width %m_plate_list[index]->thumbnail_data.height %m_plate_list[index]->thumbnail_data.pixels.size();
			}
		}

		if (m_plater && !plate_data_list[i]->no_light_thumbnail_file.empty()) {
			if (boost::filesystem::exists(plate_data_list[i]->no_light_thumbnail_file)) {
				BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": plate %1%, load no_light_thumbnail_file from %2%.")%(i+1) %plate_data_list[i]->no_light_thumbnail_file;
				m_plate_list[index]->load_thumbnail_data(plate_data_list[i]->no_light_thumbnail_file, m_plate_list[index]->no_light_thumbnail_data);
			}
		}

		if (m_plater && !plate_data_list[i]->top_file.empty()) {
			if (boost::filesystem::exists(plate_data_list[i]->top_file)) {
				BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": plate %1%, load top_thumbnail from %2%.")%(i+1) %plate_data_list[i]->top_file;
				m_plate_list[index]->load_thumbnail_data(plate_data_list[i]->top_file, m_plate_list[index]->top_thumbnail_data);
			}
		}
		if (m_plater && !plate_data_list[i]->pick_file.empty()) {
			if (boost::filesystem::exists(plate_data_list[i]->pick_file)) {
				BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": plate %1%, load pick_thumbnail from %2%.")%(i+1) %plate_data_list[i]->pick_file;
				m_plate_list[index]->load_thumbnail_data(plate_data_list[i]->pick_file, m_plate_list[index]->pick_thumbnail_data);
			}
		}
		if (m_plater && !plate_data_list[i]->pattern_bbox_file.empty()) {
			if (boost::filesystem::exists(plate_data_list[i]->pattern_bbox_file)) {
				m_plate_list[index]->load_pattern_box_data(plate_data_list[i]->pattern_bbox_file);
			}
		}

	}

	ret = reload_all_objects();

	return ret;
}

//load gcode files
int PartPlateList::load_gcode_files()
{
	int ret = 0;

	//only do this while m_plater valid for gui mode
	if (!m_plater)
		return ret;

	for (unsigned int i = 0; i < (unsigned int)m_plate_list.size(); ++i)
	{
		if (!m_plate_list[i]->m_gcode_path_from_3mf.empty()) {
			m_model->update_print_volume_state({ {m_plate_list[i]->get_shape()}, (double)this->m_plate_height });

		 	if (!m_plate_list[i]->load_gcode_from_file(m_plate_list[i]->m_gcode_path_from_3mf))
		 		ret ++;
		 }

		PartPlate* plate = m_plate_list[i];
		PlateGCodeFile gcode_file;
		if (plate->m_area_gcode_files_from_3mf.empty())
		{
			gcode_file.is_area_gcode = false;
			gcode_file.file = plate->m_gcode_path_from_3mf;
		}
		else 
		{
			gcode_file.is_area_gcode = true;
			gcode_file.areas = plate->m_area_gcode_files_from_3mf;
		}
		m_gcode_import_exporter->import_plate_gcode_files(i, gcode_file);
	}

	BOOST_LOG_TRIVIAL(trace) << boost::format("totally got %1% gcode files") % ret;

	return ret;
}

}
}