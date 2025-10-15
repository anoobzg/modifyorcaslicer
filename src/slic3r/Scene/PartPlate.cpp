#include <cstddef>
#include <algorithm>
#include <numeric>
#include <vector>
#include <string>
#include <regex>
#include <future>
#include <GL/glew.h>
#include "slic3r/Render/AppRender.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/convert.hpp>
#include <boost/nowide/cstdio.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "libslic3r/libslic3r.h"
#include "libslic3r/BoundingBox.hpp"
#include "libslic3r/Geometry.hpp"
#include "libslic3r/GCode/ThumbnailData.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/FileSystem/DataDir.hpp"
#include "libslic3r/FileSystem/Log.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "slic3r/Config/AppPreset.hpp"
 #include "libslic3r/PresetBundle.hpp"
 #include "slic3r/Slice/BackgroundSlicingProcess.hpp"
 #include "PartPlate.hpp"
 #include "slic3r/Slice/GCodeResultWrapper.hpp"
 #include "slic3r/GUI/Config/GUI_ObjectList.hpp"
 #include "slic3r/Scene/PartPlateList.hpp"
 #include "slic3r/GUI/Tab.hpp"
 #include "slic3r/GUI/format.hpp"
 #include "slic3r/GUI/GUI.hpp"
#include <wx/dcgraph.h>
using boost::optional;
namespace fs = boost::filesystem;

static unsigned int GLOBAL_PLATE_INDEX = 0;

namespace Slic3r {
namespace GUI {

PartPlate::PartPlate()
	: ObjectBase(-1), m_plater(nullptr), m_model(nullptr)
{
	m_plate_bed.set_model(&(AppAdapter::gui_app()->model()));
	assert(this->id().invalid());
	init();
}

PartPlate::PartPlate(PartPlateList *partplate_list, Vec3d origin, int width, int depth, int height, Plater* platerObj, Model* modelObj, bool printable)
	:m_partplate_list(partplate_list), m_plater(platerObj), m_model(modelObj),  m_printable(printable)
{
	m_plate_bed.set_model(&(AppAdapter::gui_app()->model()));
	m_plate_bed.set_selected(false);
	m_plate_bed.m_origin = origin;
	m_plate_bed.m_width = width;
	m_plate_bed.m_depth = depth;
	m_plate_bed.m_height = height;
	m_plate_bed.m_position = Vec2d(origin[0], origin[1]);
	init();
}

PartPlate::~PartPlate()
{
	clear();
}

void PartPlate::init()
{
	m_selected = false;

	m_locked = false;
	m_ready_for_slice = true;
	m_slice_result_valid = false;
	m_slice_percent = 0.0f;
	m_hover_id = -1;

	m_print_index = -1;
}

void PartPlate::set_plate_area_count(int count)
{
	get_slice_result_wrapper()->resize(count);

	std::vector<Print*> prints = get_slice_result_wrapper()->get_prints();

	for (Print* print : prints)
	{
		auto statuscb = [this](const Slic3r::PrintBase::SlicingStatus& status) {
			Slic3r::SlicingStatusEvent *event = new Slic3r::SlicingStatusEvent(EVT_SLICING_UPDATE, 0, status);
			//BBS: GUI refactor: add plate info befor message
			if (status.message_type == Slic3r::PrintStateBase::SlicingDefaultNotification) {
				auto temp = Slic3r::format(_u8L(" plate %1%:"), std::to_string(m_progress_palte_index + 1));
				event->status.text = temp + event->status.text;
			}
			wxQueueEvent(m_plater, event);
		};
		print->set_status_callback(statuscb);
	}
}

BedType PartPlate::get_bed_type(bool load_from_project) const
{
	std::string bed_type_key = "curr_bed_type";

	if (m_config.has(bed_type_key)) {
		BedType bed_type = m_config.opt_enum<BedType>(bed_type_key);
		return bed_type;
	}

	if (!load_from_project || !m_plater || !app_preset_bundle())
		return btDefault;

	DynamicConfig& proj_cfg = app_preset_bundle()->project_config;
	if (proj_cfg.has(bed_type_key))
		return proj_cfg.opt_enum<BedType>(bed_type_key);
	return btDefault;
}

void PartPlate::set_bed_type(BedType bed_type)
{
    std::string bed_type_key = "curr_bed_type";

    // should be called in GUI context
    assert(m_plater != nullptr);

    // update slice state
    BedType old_real_bed_type = get_bed_type();
    if (old_real_bed_type == btDefault) {
        DynamicConfig& proj_cfg = app_preset_bundle()->project_config;
        if (proj_cfg.has(bed_type_key))
            old_real_bed_type = proj_cfg.opt_enum<BedType>(bed_type_key);
    }
    BedType new_real_bed_type = bed_type;
    if (bed_type == BedType::btDefault) {
        DynamicConfig& proj_cfg = app_preset_bundle()->project_config;
        if (proj_cfg.has(bed_type_key))
            new_real_bed_type = proj_cfg.opt_enum<BedType>(bed_type_key);
    }
    if (old_real_bed_type != new_real_bed_type) {
        update_slice_result_valid_state(false);
    }

    if (bed_type == BedType::btDefault)
        m_config.erase(bed_type_key);
    else
        m_config.set_key_value("curr_bed_type", new ConfigOptionEnum<BedType>(bed_type));
}

void PartPlate::reset_bed_type()
{
    m_config.erase("curr_bed_type");
}

void PartPlate::set_print_seq(PrintSequence print_seq)
{
    std::string print_seq_key = "print_sequence";

    // should be called in GUI context
    assert(m_plater != nullptr);

    // update slice state
    PrintSequence old_real_print_seq = get_print_seq();
    if (old_real_print_seq == PrintSequence::ByDefault) {
        auto curr_preset_config = app_preset_bundle()->prints.get_edited_preset().config;
        if (curr_preset_config.has(print_seq_key))
            old_real_print_seq = curr_preset_config.option<ConfigOptionEnum<PrintSequence>>(print_seq_key)->value;
    }

    PrintSequence new_real_print_seq = print_seq;

    if (print_seq == PrintSequence::ByDefault) {
        auto curr_preset_config = app_preset_bundle()->prints.get_edited_preset().config;
        if (curr_preset_config.has(print_seq_key))
            new_real_print_seq = curr_preset_config.option<ConfigOptionEnum<PrintSequence>>(print_seq_key)->value;
    }

    if (old_real_print_seq != new_real_print_seq) {
        update_slice_result_valid_state(false);
    }

    //print_seq_same_global = same_global;
    if (print_seq == PrintSequence::ByDefault)
        m_config.erase(print_seq_key);
    else
        m_config.set_key_value(print_seq_key, new ConfigOptionEnum<PrintSequence>(print_seq));
}

PrintSequence PartPlate::get_print_seq() const
{
    std::string print_seq_key = "print_sequence";

    if (m_config.has(print_seq_key)) {
        PrintSequence print_seq = m_config.opt_enum<PrintSequence>(print_seq_key);
        return print_seq;
    }

    return PrintSequence::ByDefault;
}

PrintSequence PartPlate::get_real_print_seq(bool* plate_same_as_global) const
{
	PrintSequence global_print_seq = AppAdapter::gui_app()->global_print_sequence();
    PrintSequence curr_plate_seq = get_print_seq();
    if (curr_plate_seq == PrintSequence::ByDefault) {
		curr_plate_seq = global_print_seq;
    }

	if(plate_same_as_global)
		*plate_same_as_global = (curr_plate_seq == global_print_seq);

    return curr_plate_seq;
}

bool PartPlate::has_spiral_mode_config() const
{
	std::string key = "spiral_mode";
	return m_config.has(key);
}

bool PartPlate::get_spiral_vase_mode() const
{
	std::string key = "spiral_mode";
	if (m_config.has(key)) {
		return m_config.opt_bool(key);
	}
	else {
		DynamicPrintConfig* global_config = &app_preset_bundle()->prints.get_edited_preset().config;
		if (global_config->has(key))
			return global_config->opt_bool(key);
	}
	return false;
}

void PartPlate::set_spiral_vase_mode(bool spiral_mode, bool as_global)
{
	std::string key = "spiral_mode";
	if (as_global)
		m_config.erase(key);
	else {
		if (spiral_mode) {
			if (get_spiral_vase_mode())
				return;
			// Secondary confirmation
			auto answer = static_cast<TabPrintPlate*>(AppAdapter::gui_app()->plate_tab)->show_spiral_mode_settings_dialog(false);
			if (answer == wxID_YES) {
				m_config.set_key_value(key, new ConfigOptionBool(true));
				set_vase_mode_related_object_config();
			}
		}
		else
			m_config.set_key_value(key, new ConfigOptionBool(false));
	}
}

bool PartPlate::valid_instance(int obj_id, int instance_id)
{
	if ((obj_id >= 0) && (obj_id < m_model->objects.size()))
	{
		ModelObject* object = m_model->objects[obj_id];
		if ((instance_id >= 0) && (instance_id < object->instances.size()))
			return true;
	}

	return false;
}

void PartPlate::register_raycasters_for_picking(SceneRaycaster* raycaster)
{
	m_plate_bed.register_raycasters_for_picking(raycaster, m_plate_index);
}

std::vector<int> PartPlate::get_extruders(bool conside_custom_gcode) const
{
	std::vector<int> plate_extruders;
	// if gcode.3mf file
	if (m_model->objects.empty()) {
		for (int i = 0; i < slice_filaments_info.size(); i++) {
			plate_extruders.push_back(slice_filaments_info[i].id + 1);
		}
		return plate_extruders;
	}

	// if 3mf file
	const DynamicPrintConfig& glb_config = app_preset_bundle()->prints.get_edited_preset().config;
	int glb_support_intf_extr = glb_config.opt_int("support_interface_filament");
	int glb_support_extr = glb_config.opt_int("support_filament");
	int glb_wall_extr = glb_config.opt_int("wall_filament");
	int glb_sparse_infill_extr = glb_config.opt_int("sparse_infill_filament");
	int glb_solid_infill_extr = glb_config.opt_int("solid_infill_filament");
	bool glb_support = glb_config.opt_bool("enable_support");
    glb_support |= glb_config.opt_int("raft_layers") > 0;

	for (int obj_idx = 0; obj_idx < m_model->objects.size(); obj_idx++) {
		if (!contain_instance_totally(obj_idx, 0))
			continue;

		ModelObject* mo = m_model->objects[obj_idx];
		for (ModelVolume* mv : mo->volumes) {
			std::vector<int> volume_extruders = mv->get_extruders();
			plate_extruders.insert(plate_extruders.end(), volume_extruders.begin(), volume_extruders.end());
		}

		// layer range
        for (auto layer_range : mo->layer_config_ranges) {
            if (layer_range.second.has("extruder")) {
                if (auto id = layer_range.second.option("extruder")->getInt(); id > 0)
					plate_extruders.push_back(id);
			}
		}

		bool obj_support = false;
		const ConfigOption* obj_support_opt = mo->config.option("enable_support");
        const ConfigOption *obj_raft_opt    = mo->config.option("raft_layers");
		if (obj_support_opt != nullptr || obj_raft_opt != nullptr) {
            if (obj_support_opt != nullptr)
				obj_support = obj_support_opt->getBool();
            if (obj_raft_opt != nullptr)
				obj_support |= obj_raft_opt->getInt() > 0;
        }
		else
			obj_support = glb_support;

        if (obj_support) {
            int                 obj_support_intf_extr = 0;
            const ConfigOption* support_intf_extr_opt = mo->config.option("support_interface_filament");
            if (support_intf_extr_opt != nullptr)
                obj_support_intf_extr = support_intf_extr_opt->getInt();
            if (obj_support_intf_extr != 0)
                plate_extruders.push_back(obj_support_intf_extr);
            else if (glb_support_intf_extr != 0)
                plate_extruders.push_back(glb_support_intf_extr);

            int                 obj_support_extr = 0;
            const ConfigOption* support_extr_opt = mo->config.option("support_filament");
            if (support_extr_opt != nullptr)
                obj_support_extr = support_extr_opt->getInt();
            if (obj_support_extr != 0)
                plate_extruders.push_back(obj_support_extr);
            else if (glb_support_extr != 0)
                plate_extruders.push_back(glb_support_extr);
        }

        int obj_wall_extr = 1;
		const ConfigOption* wall_opt = mo->config.option("wall_filament");
		if (wall_opt != nullptr)
			obj_wall_extr = wall_opt->getInt();
		if (obj_wall_extr != 1)
			plate_extruders.push_back(obj_wall_extr);
		else if (glb_wall_extr != 1)
			plate_extruders.push_back(glb_wall_extr);

		int obj_sparse_infill_extr = 1;
		const ConfigOption* sparse_infill_opt = mo->config.option("sparse_infill_filament");
		if (sparse_infill_opt != nullptr)
			obj_sparse_infill_extr = sparse_infill_opt->getInt();
		if (obj_sparse_infill_extr != 1)
			plate_extruders.push_back(obj_sparse_infill_extr);
		else if (glb_sparse_infill_extr != 1)
			plate_extruders.push_back(glb_sparse_infill_extr);

		int obj_solid_infill_extr = 1;
		const ConfigOption* solid_infill_opt = mo->config.option("solid_infill_filament");
		if (solid_infill_opt != nullptr)
			obj_solid_infill_extr = solid_infill_opt->getInt();
		if (obj_solid_infill_extr != 1)
			plate_extruders.push_back(obj_solid_infill_extr);
		else if (glb_solid_infill_extr != 1)
			plate_extruders.push_back(glb_solid_infill_extr);

	}

	if (conside_custom_gcode) {
		//BBS
        int nums_extruders = 0;
        if (const ConfigOptionStrings *color_option = dynamic_cast<const ConfigOptionStrings *>(app_preset_bundle()->project_config.option("filament_colour"))) {
            nums_extruders = color_option->values.size();
			if (m_model->plates_custom_gcodes.find(m_plate_index) != m_model->plates_custom_gcodes.end()) {
				for (auto item : m_model->plates_custom_gcodes.at(m_plate_index).gcodes) {
					if (item.type == CustomGCode::Type::ToolChange && item.extruder <= nums_extruders)
						plate_extruders.push_back(item.extruder);
				}
			}
		}
	}

	std::sort(plate_extruders.begin(), plate_extruders.end());
	auto it_end = std::unique(plate_extruders.begin(), plate_extruders.end());
	plate_extruders.resize(std::distance(plate_extruders.begin(), it_end));
	return plate_extruders;
}

std::vector<int> PartPlate::get_extruders_under_cli(bool conside_custom_gcode, DynamicPrintConfig& full_config) const
{
    std::vector<int> plate_extruders;

    // if 3mf file
    int glb_support_intf_extr = full_config.opt_int("support_interface_filament");
    int glb_support_extr = full_config.opt_int("support_filament");
	int glb_wall_extr = full_config.opt_int("wall_filament");
	int glb_sparse_infill_extr = full_config.opt_int("sparse_infill_filament");
	int glb_solid_infill_extr = full_config.opt_int("solid_infill_filament");

    bool glb_support = full_config.opt_bool("enable_support");
    glb_support |= full_config.opt_int("raft_layers") > 0;

    for (std::set<std::pair<int, int>>::iterator it = obj_to_instance_set.begin(); it != obj_to_instance_set.end(); ++it)
    {
        int obj_id = it->first;
        int instance_id = it->second;

        if ((obj_id >= 0) && (obj_id < m_model->objects.size()))
        {
            ModelObject* object = m_model->objects[obj_id];
            ModelInstance* instance = object->instances[instance_id];

            if (!instance->printable)
                continue;

            for (ModelVolume* mv : object->volumes) {
                std::vector<int> volume_extruders = mv->get_extruders();
                plate_extruders.insert(plate_extruders.end(), volume_extruders.begin(), volume_extruders.end());
            }

            // layer range
            for (auto layer_range : object->layer_config_ranges) {
                if (layer_range.second.has("extruder")) {
                    if (auto id = layer_range.second.option("extruder")->getInt(); id > 0)
                        plate_extruders.push_back(id);
                }
            }

            bool obj_support = false;
            const ConfigOption* obj_support_opt = object->config.option("enable_support");
            const ConfigOption *obj_raft_opt    = object->config.option("raft_layers");
            if (obj_support_opt != nullptr || obj_raft_opt != nullptr) {
                if (obj_support_opt != nullptr)
                    obj_support = obj_support_opt->getBool();
                if (obj_raft_opt != nullptr)
                    obj_support |= obj_raft_opt->getInt() > 0;
            }
            else
                obj_support = glb_support;

            if (!obj_support)
                continue;

            int obj_support_intf_extr = 0;
            const ConfigOption* support_intf_extr_opt = object->config.option("support_interface_filament");
            if (support_intf_extr_opt != nullptr)
                obj_support_intf_extr = support_intf_extr_opt->getInt();
            if (obj_support_intf_extr != 0)
                plate_extruders.push_back(obj_support_intf_extr);
            else if (glb_support_intf_extr != 0)
                plate_extruders.push_back(glb_support_intf_extr);

            int obj_support_extr = 0;
            const ConfigOption* support_extr_opt = object->config.option("support_filament");
            if (support_extr_opt != nullptr)
                obj_support_extr = support_extr_opt->getInt();
            if (obj_support_extr != 0)
                plate_extruders.push_back(obj_support_extr);
            else if (glb_support_extr != 0)
                plate_extruders.push_back(glb_support_extr);

			int obj_wall_extr = 1;
			const ConfigOption* wall_opt = object->config.option("wall_filament");
			if (wall_opt != nullptr)
				obj_wall_extr = wall_opt->getInt();
			if (obj_wall_extr != 1)
				plate_extruders.push_back(obj_wall_extr);
			else if (glb_wall_extr != 1)
				plate_extruders.push_back(glb_wall_extr);

			int obj_sparse_infill_extr = 1;
			const ConfigOption* sparse_infill_opt = object->config.option("sparse_infill_filament");
			if (sparse_infill_opt != nullptr)
				obj_sparse_infill_extr = sparse_infill_opt->getInt();
			if (obj_sparse_infill_extr != 1)
				plate_extruders.push_back(obj_sparse_infill_extr);
			else if (glb_sparse_infill_extr != 1)
				plate_extruders.push_back(glb_sparse_infill_extr);

			int obj_solid_infill_extr = 1;
			const ConfigOption* solid_infill_opt = object->config.option("solid_infill_filament");
			if (solid_infill_opt != nullptr)
				obj_solid_infill_extr = solid_infill_opt->getInt();
			if (obj_solid_infill_extr != 1)
				plate_extruders.push_back(obj_solid_infill_extr);
			else if (glb_solid_infill_extr != 1)
				plate_extruders.push_back(glb_solid_infill_extr);
        }
    }

    if (conside_custom_gcode) {
        //BBS
        int nums_extruders = 0;
        if (const ConfigOptionStrings *color_option = dynamic_cast<const ConfigOptionStrings *>(full_config.option("filament_colour"))) {
            nums_extruders = color_option->values.size();
            if (m_model->plates_custom_gcodes.find(m_plate_index) != m_model->plates_custom_gcodes.end()) {
                for (auto item : m_model->plates_custom_gcodes.at(m_plate_index).gcodes) {
                    if (item.type == CustomGCode::Type::ToolChange && item.extruder <= nums_extruders)
                        plate_extruders.push_back(item.extruder);
                }
            }
        }
    }

    std::sort(plate_extruders.begin(), plate_extruders.end());
    auto it_end = std::unique(plate_extruders.begin(), plate_extruders.end());
    plate_extruders.resize(std::distance(plate_extruders.begin(), it_end));
    return plate_extruders;
}

std::vector<int> PartPlate::get_extruders_without_support(bool conside_custom_gcode) const
{
	std::vector<int> plate_extruders;
	// if gcode.3mf file
	if (m_model->objects.empty()) {
		for (int i = 0; i < slice_filaments_info.size(); i++) {
			plate_extruders.push_back(slice_filaments_info[i].id + 1);
		}
		return plate_extruders;
	}

	// if 3mf file
	const DynamicPrintConfig& glb_config = app_preset_bundle()->prints.get_edited_preset().config;

	for (int obj_idx = 0; obj_idx < m_model->objects.size(); obj_idx++) {
		if (!contain_instance_totally(obj_idx, 0))
			continue;

		ModelObject* mo = m_model->objects[obj_idx];
		for (ModelVolume* mv : mo->volumes) {
			std::vector<int> volume_extruders = mv->get_extruders();
			plate_extruders.insert(plate_extruders.end(), volume_extruders.begin(), volume_extruders.end());
		}
	}

	if (conside_custom_gcode) {
		//BBS
		int nums_extruders = 0;
		if (const ConfigOptionStrings* color_option = dynamic_cast<const ConfigOptionStrings*>(app_preset_bundle()->project_config.option("filament_colour"))) {
			nums_extruders = color_option->values.size();
			if (m_model->plates_custom_gcodes.find(m_plate_index) != m_model->plates_custom_gcodes.end()) {
				for (auto item : m_model->plates_custom_gcodes.at(m_plate_index).gcodes) {
					if (item.type == CustomGCode::Type::ToolChange && item.extruder <= nums_extruders)
						plate_extruders.push_back(item.extruder);
				}
			}
		}
	}

	std::sort(plate_extruders.begin(), plate_extruders.end());
	auto it_end = std::unique(plate_extruders.begin(), plate_extruders.end());
	plate_extruders.resize(std::distance(plate_extruders.begin(), it_end));
	return plate_extruders;
}

std::vector<int> PartPlate::get_used_extruders()
{
	std::vector<int> used_extruders;
	// if gcode.3mf file
	if (m_model->objects.empty()) {
		for (int i = 0; i < slice_filaments_info.size(); i++) {
			used_extruders.push_back(slice_filaments_info[i].id + 1);
		}
		return used_extruders;
	}

	GCodeResultWrapper* result = get_slice_result_wrapper();
	if (!result)
		return used_extruders;

	std::set<int> used_extruders_set;
	PrintEstimatedStatistics& ps = result->print_statistics();
	for (const auto& item : ps.total_volumes_per_extruder)
		used_extruders_set.emplace(item.first + 1);

	return std::vector(used_extruders_set.begin(), used_extruders_set.end());
}

Vec3d PartPlate::estimate_wipe_tower_size(const DynamicPrintConfig & config, const double w, const double d, int plate_extruder_size, bool use_global_objects) const
{
	Vec3d wipe_tower_size;

	double layer_height = 0.08f; // hard code layer height
	double max_height = 0.f;
	wipe_tower_size.setZero();
	wipe_tower_size(0) = w;

	const ConfigOption* layer_height_opt = config.option("layer_height");
	if (layer_height_opt)
		layer_height = layer_height_opt->getFloat();

	// empty plate
	if (plate_extruder_size == 0)
    {
        std::vector<int> plate_extruders = get_extruders(true);
        plate_extruder_size = plate_extruders.size();
    }
	if (plate_extruder_size == 0)
		return wipe_tower_size;

	for (int obj_idx = 0; obj_idx < m_model->objects.size(); obj_idx++) {
		if (!use_global_objects && !contain_instance_totally(obj_idx, 0))
			continue;

		BoundingBoxf3 bbox = m_model->objects[obj_idx]->bounding_box_exact();
		max_height = std::max(bbox.size().z(), max_height);
	}
	wipe_tower_size(2) = max_height;

	//const DynamicPrintConfig &dconfig = app_preset_bundle()->prints.get_edited_preset().config;
    auto timelapse_type    = config.option<ConfigOptionEnum<TimelapseType>>("timelapse_type");
    bool timelapse_enabled = timelapse_type ? (timelapse_type->value == TimelapseType::tlSmooth) : false;

    double depth = plate_extruder_size == 1 ? 0 : d;
    if (timelapse_enabled || depth > EPSILON) {
		float min_wipe_tower_depth = 0.f;
		auto iter = WipeTower::min_depth_per_height.begin();
		while (iter != WipeTower::min_depth_per_height.end()) {
			auto curr_height_to_depth = *iter;

			// This is the case that wipe tower height is lower than the first min_depth_to_height member.
			if (curr_height_to_depth.first >= max_height) {
				min_wipe_tower_depth = curr_height_to_depth.second;
				break;
			}

			iter++;

			// If curr_height_to_depth is the last member, use its min_depth.
			if (iter == WipeTower::min_depth_per_height.end()) {
				min_wipe_tower_depth = curr_height_to_depth.second;
				break;
			}

			// If wipe tower height is between the current and next member, set the min_depth as linear interpolation between them
			auto next_height_to_depth = *iter;
			if (next_height_to_depth.first > max_height) {
				float height_base = curr_height_to_depth.first;
				float height_diff = next_height_to_depth.first - curr_height_to_depth.first;
				float min_depth_base = curr_height_to_depth.second;
				float depth_diff = next_height_to_depth.second - curr_height_to_depth.second;

				min_wipe_tower_depth = min_depth_base + (max_height - curr_height_to_depth.first) / height_diff * depth_diff;
				break;
			}
		}
		depth = std::max((double)min_wipe_tower_depth, depth);
	}
	wipe_tower_size(1) = depth;
	return wipe_tower_size;
}

arrangement::ArrangePolygon PartPlate::estimate_wipe_tower_polygon(const DynamicPrintConfig& config, int plate_index, int plate_extruder_size, bool use_global_objects) const
{
	float x = dynamic_cast<const ConfigOptionFloats*>(config.option("wipe_tower_x"))->get_at(plate_index);
	float y = dynamic_cast<const ConfigOptionFloats*>(config.option("wipe_tower_y"))->get_at(plate_index);
	float w = dynamic_cast<const ConfigOptionFloat*>(config.option("prime_tower_width"))->value;
	//float a = dynamic_cast<const ConfigOptionFloat*>(config.option("wipe_tower_rotation_angle"))->value;
	float v = dynamic_cast<const ConfigOptionFloat*>(config.option("prime_volume"))->value;
    float tower_brim_width = dynamic_cast<const ConfigOptionFloat*>(config.option("prime_tower_brim_width"))->value;
    Vec3d wipe_tower_size = estimate_wipe_tower_size(config, w, v, plate_extruder_size, use_global_objects);
	int plate_width = m_plate_bed.m_width, plate_depth = m_plate_bed.m_depth;
	float depth = wipe_tower_size(1);
	float margin = WIPE_TOWER_MARGIN + tower_brim_width, wp_brim_width = 0.f;
	const ConfigOption* wipe_tower_brim_width_opt = config.option("prime_tower_brim_width");
	if (wipe_tower_brim_width_opt) {
		wp_brim_width = wipe_tower_brim_width_opt->getFloat();
		BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format("arrange wipe_tower: wp_brim_width %1%") % wp_brim_width;
	}

	x = std::clamp(x, margin, (float)plate_width - w - margin - wp_brim_width);
	y = std::clamp(y, margin, (float)plate_depth - depth - margin - wp_brim_width);

	arrangement::ArrangePolygon wipe_tower_ap;
	Polygon ap({
		{scaled(x - wp_brim_width), scaled(y - wp_brim_width)},
		{scaled(x + w + wp_brim_width), scaled(y - wp_brim_width)},
		{scaled(x + w + wp_brim_width), scaled(y + depth + wp_brim_width)},
		{scaled(x - wp_brim_width), scaled(y + depth + wp_brim_width)}
		});
	wipe_tower_ap.bed_idx = plate_index;
	wipe_tower_ap.setter = NULL; // do not move wipe tower

	wipe_tower_ap.poly.contour = std::move(ap);
	wipe_tower_ap.translation = { scaled(0.f), scaled(0.f) };
	//wipe_tower_ap.rotation = a;
	wipe_tower_ap.name = "WipeTower";
	wipe_tower_ap.is_virt_object = true;
	wipe_tower_ap.is_wipe_tower = true;

	return wipe_tower_ap;
}

bool PartPlate::operator<(PartPlate& plate) const
{
	int index = plate.get_index();
	return (this->m_plate_index < index);
}

//set the plate's index
void PartPlate::set_index(int index)
{
	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": plate_id update from %1% to %2%") % m_plate_index % index;

	m_plate_index = index;
	m_progress_palte_index = index;
}

void PartPlate::set_slicing_progress()
{
	if (m_plater)
		m_plater->set_notification_manager();
}

void PartPlate::set_slicing_progress_index(int index)
{
	m_progress_palte_index = index;
}

void PartPlate::clear(bool clear_sliced_result)
{
	obj_to_instance_set.clear();
	instance_outside_set.clear();
	if (clear_sliced_result) {
		m_ready_for_slice = true;
		update_slice_result_valid_state(false);
		m_has_external_gcode = false;  // Clear external gcode flag
	}
	m_plate_bed.m_name_texture.reset();
	return;
}

/* size and position related functions*/
//set position and size
void PartPlate::set_pos_and_size(Vec3d& origin, int width, int depth, int height, bool with_instance_move, bool do_clear)
{
	bool size_changed = false; //size changed means the machine changed
	bool pos_changed = false;

	size_changed = ((width != m_plate_bed.m_width) || (depth != m_plate_bed.m_depth) || (height != m_plate_bed.m_height));
	pos_changed = (m_plate_bed.m_origin != origin);

	if ((!size_changed) && (!pos_changed))
	{
		//size and position the same with before, just return
		return;
	}

	if (with_instance_move && m_model)
	{
		for (std::set<std::pair<int, int>>::iterator it = obj_to_instance_set.begin(); it != obj_to_instance_set.end(); ++it) {
			int obj_id = it->first;
			int instance_id = it->second;
			ModelObject* object = m_model->objects[obj_id];
			ModelInstance* instance = object->instances[instance_id];

			//move this instance into the new plate's same position
			Vec3d offset = instance->get_transformation().get_offset();
			int off_x, off_y;

			if (size_changed)
			{
				//change position due to the bed size changes
				off_x = origin.x() - m_plate_bed.m_origin.x() + (width - m_plate_bed.m_width) / 2;
				off_y = origin.y() - m_plate_bed.m_origin.y() + (depth - m_plate_bed.m_depth) / 2;
			}
			else
			{
				//change position due to the plate moves
				off_x = origin.x() - m_plate_bed.m_origin.x();
				off_y = origin.y() - m_plate_bed.m_origin.y();
			}
			offset.x() = offset.x() + off_x;
			offset.y() = offset.y() + off_y;

			BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": object %1%, instance %2%, moved {%3%,%4%} to {%5%, %6%}")\
				% obj_id % instance_id % off_x % off_y % offset.x() % offset.y();

			instance->set_offset(offset);
			object->invalidate_bounding_box();
		}
	}
	else if (do_clear)
	{
		clear();
	}

	m_plate_bed.m_origin = origin;
	m_plate_bed.m_width = width;
	m_plate_bed.m_depth = depth;
	m_plate_bed.m_height = height;
	m_plate_bed.m_position = Vec2d(origin[0], origin[1]);

	return;
}

Vec2d PartPlate::get_size() const 
{ 
	return m_plate_bed.get_size(); 
}

Vec3d& PartPlate::get_origin()
{
	return m_plate_bed.m_origin;
}

int& PartPlate::get_width()
{
	return m_plate_bed.m_width;
}

int& PartPlate::get_depth()
{
	return m_plate_bed.m_depth;
}

int& PartPlate::get_height()
{
	return m_plate_bed.m_height;
}

//get the plate's center point origin
Vec3d PartPlate::get_center_origin()
{
	return  m_plate_bed.get_center_origin();
}

void PartPlate::generate_plate_name_texture()
{
	m_plate_bed.generate_plate_name_texture();
}

void PartPlate::set_plate_name(const std::string& name) 
{ 
	// compare if name equal to m_name, case sensitive
    if (boost::equals(m_name, name))
        return;

	m_name = name;

	generate_plate_name_texture();
}

//get the print's object, result and index
void PartPlate::get_print(GCodeResultWrapper** result, int* index)
{
	if (result)
		*result = m_gcode_result;

	if (index)
		*index = m_print_index;

	return;
}

GCodeResultWrapper* PartPlate::get_gcode_result()
{
	return m_gcode_result;
}

//set the print object, result and it's index
void PartPlate::set_print(GCodeResultWrapper* result, int index)
{
	m_gcode_result = result;
	if (index >= 0)
		m_print_index = index;
}

std::string PartPlate::get_gcode_filename()
{
	if (is_slice_result_valid() && get_slice_result_wrapper()) {
		return m_gcode_result->filename();
	}
	return "";
}

bool PartPlate::is_valid_gcode_file()
{
	if (get_gcode_filename().empty())
		return false;
	boost::filesystem::path gcode_file(m_gcode_result->filename());
	if (!boost::filesystem::exists(gcode_file)) {
		BOOST_LOG_TRIVIAL(info) << "invalid gcode file, file is missing, file = " << m_gcode_result->filename();
		return false;
	}
	return true;
}

ModelObjectPtrs PartPlate::get_objects() 
{ 
	return m_model->objects; 
}

ModelObjectPtrs PartPlate::get_objects_on_this_plate() {
    ModelObjectPtrs objects_ptr;
    int obj_id;
    for (auto it = obj_to_instance_set.begin(); it != obj_to_instance_set.end(); it++) {
        obj_id = it->first;
        objects_ptr.push_back(m_model->objects[obj_id]);
    }
    return objects_ptr;
}

ModelInstance* PartPlate::get_instance(int obj_id, int instance_id)
{
	if (!contain_instance(obj_id, instance_id))
		return nullptr;
	else
		return m_model->objects[obj_id]->instances[instance_id];
}

/* instance related operations*/
//judge whether instance is bound in plate or not
bool PartPlate::contain_instance(int obj_id, int instance_id)
{
	bool result = false;
	std::set<std::pair<int, int>>::iterator it;

	it = obj_to_instance_set.find(std::pair(obj_id, instance_id));
	if (it != obj_to_instance_set.end()) {
		result = true;
	}

	return result;
}

//judge whether instance is bound in plate or not
bool PartPlate::contain_instance_totally(ModelObject* object, int instance_id) const
{
	bool result = false;
	int obj_id = -1;

	for (int index = 0; index < m_model->objects.size(); index ++)
	{
		if (m_model->objects[index] == object)
		{
			obj_id = index;
			break;
		}
	}

	if ((obj_id >= 0 ) && (obj_id < m_model->objects.size()))
		result = contain_instance_totally(obj_id, instance_id);

	return result;
}

//judge whether instance is totally included in plate or not
bool PartPlate::contain_instance_totally(int obj_id, int instance_id) const
{
	bool result = false;
	std::set<std::pair<int, int>>::iterator it;

	it = obj_to_instance_set.find(std::pair(obj_id, instance_id));
	if (it != obj_to_instance_set.end()) {
		it = instance_outside_set.find(std::pair(obj_id, instance_id));
		if (it == instance_outside_set.end())
			result = true;
	}

	return result;
}

//check whether instance is outside the plate or not
bool PartPlate::check_outside(int obj_id, int instance_id, BoundingBoxf3* bounding_box)
{
	return m_plate_bed.check_outside(obj_id, instance_id, bounding_box);
}

//judge whether instance is intesected with plate or not
bool PartPlate::intersect_instance(int obj_id, int instance_id, BoundingBoxf3* bounding_box)
{
	bool result = false;

	if (!valid_instance(obj_id, instance_id))
	{
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(": plate_id %1%, invalid obj_id %2%, instance_id %3%") % m_plate_index % obj_id % instance_id;
		return false;
	}

	if (m_printable)
	{
		ModelObject* object = m_model->objects[obj_id];
		ModelInstance* instance = object->instances[instance_id];
		BoundingBoxf3 instance_box = bounding_box? *bounding_box: object->instance_convex_hull_bounding_box(instance_id);
		result = check_intersects(instance_box);
	}
	else
	{
		result = is_left_top_of(obj_id, instance_id);
	}

	return result;
}

//judge whether the plate's origin is at the left of instance or not
bool PartPlate::is_left_top_of(int obj_id, int instance_id)
{
	bool result = false;

	if (!valid_instance(obj_id, instance_id))
	{
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(": plate_id %1%, invalid obj_id %2%, instance_id %3%") % m_plate_index % obj_id % instance_id;
		return false;
	}

	ModelObject* object = m_model->objects[obj_id];
	ModelInstance* instance = object->instances[instance_id];
	std::pair<int, int> pair(obj_id, instance_id);
	BoundingBoxf3 instance_box = object->instance_convex_hull_bounding_box(instance_id);

	result = (m_plate_bed.m_origin.x() <= instance_box.min.x()) && (m_plate_bed.m_origin.y() >= instance_box.min.y());
	return result;
}

//add an instance into plate
int PartPlate::add_instance(int obj_id, int instance_id, bool move_position, BoundingBoxf3* bounding_box)
{
	if (!valid_instance(obj_id, instance_id))
	{
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(": plate_id %1%, invalid obj_id %2%, instance_id %3%, move_position %4%") % m_plate_index % obj_id % instance_id % move_position;
		return -1;
	}

	ModelObject* object = m_model->objects[obj_id];
	ModelInstance* instance = object->instances[instance_id];
	std::pair<int, int> pair(obj_id, instance_id);

    obj_to_instance_set.insert(pair);

	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": plate_id %1%, add instance obj_id %2%, instance_id %3%, move_position %4%") % m_plate_index % obj_id % instance_id % move_position;

	if (move_position)
	{
		//move this instance into the new position
		Vec3d center = get_center_origin();
		center.z() = instance->get_transformation().get_offset(Z);

		instance->set_offset(center);
		object->invalidate_bounding_box();
	}

	//need to judge whether this instance has an outer part
	bool outside = check_outside(obj_id, instance_id, bounding_box);
	if (outside)
		instance_outside_set.insert(pair);
	else
	{
		Vec3d center = m_plate_bed.get_center_origin();
		center[2] = 0;
		instance->set_idex_mirror_center(center);
	}

	if (m_ready_for_slice && outside)
	{
		m_ready_for_slice = false;
	}
	else if ((obj_to_instance_set.size() == 1) && (!m_ready_for_slice) && !outside)
	{
		m_ready_for_slice = true;
	}

	BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": plate %1% , m_ready_for_slice changes to %2%") % m_plate_index %m_ready_for_slice;
	return 0;
}

//remove instance from plate
int PartPlate::remove_instance(int obj_id, int instance_id)
{
	bool result;
	std::set<std::pair<int, int>>::iterator it;

	it = obj_to_instance_set.find(std::pair(obj_id, instance_id));
	if (it != obj_to_instance_set.end()) {
		obj_to_instance_set.erase(it);
		BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(":plate_id %1%, found obj_id %2%, instance_id %3%") % m_plate_index % obj_id % instance_id;
		result = 0;
	}
	else {
		BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": plate_id %1%, can not find obj_id %2%, instance_id %3%") % m_plate_index % obj_id % instance_id;
		result = -1;
		return result;
	}

	it = instance_outside_set.find(std::pair(obj_id, instance_id));
	if (it != instance_outside_set.end()) {
		instance_outside_set.erase(it);
	}
	if (!m_ready_for_slice)
		update_states();

	return result;
}

BoundingBoxf3 PartPlate::get_objects_bounding_box()
{
    BoundingBoxf3 bbox;
    for (std::set<std::pair<int, int>>::iterator it = obj_to_instance_set.begin(); it != obj_to_instance_set.end(); ++it)
    {
        int obj_id = it->first;
        int instance_id = it->second;

        if ((obj_id >= 0) && (obj_id < m_model->objects.size()))
        {
            ModelObject* object = m_model->objects[obj_id];
            if ((instance_id >= 0) && (instance_id < object->instances.size()))
            {
                BoundingBoxf3 instance_bbox = object->instance_bounding_box(instance_id);
                bbox.merge(instance_bbox);
            }
        }
    }
    return bbox;
}

//translate instance on the plate
void PartPlate::translate_all_instance(Vec3d position)
{
    for (std::set<std::pair<int, int>>::iterator it = obj_to_instance_set.begin(); it != obj_to_instance_set.end(); ++it)
    {
        int obj_id = it->first;
        int instance_id = it->second;

        if ((obj_id >= 0) && (obj_id < m_model->objects.size()))
        {
            ModelObject* object = m_model->objects[obj_id];
            if ((instance_id >= 0) && (instance_id < object->instances.size()))
            {
                ModelInstance* instance = object->instances[instance_id];
                const Vec3d& offset =  instance->get_offset();
                instance->set_offset(offset + position);
            }
        }
    }
    return;
}

void PartPlate::duplicate_all_instance(unsigned int dup_count, bool need_skip, std::map<int, bool>& skip_objects)
{
    std::set<std::pair<int, int>> old_obj_list = obj_to_instance_set;
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": plate_id %1%, dup_count %2%") % m_plate_index % dup_count;
    for (std::set<std::pair<int, int>>::iterator it = old_obj_list.begin(); it != old_obj_list.end(); ++it)
    {
        int obj_id = it->first;
        int instance_id = it->second;

        if ((obj_id >= 0) && (obj_id < m_model->objects.size()))
        {
            ModelObject* object = m_model->objects[obj_id];
            ModelInstance* instance = object->instances[instance_id];

            if (need_skip)
            {
                if (skip_objects.find(instance->loaded_id) != skip_objects.end())
                {
                    instance->printable = false;
                    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": skipped object, loaded_id %1%, name %2%, set to unprintable, no need to duplicate") % instance->loaded_id % object->name;
                    continue;
                }
            }
            for (size_t index = 0; index < dup_count; index ++)
            {
                ModelObject* newObj = m_model->add_object(*object);
                newObj->name = object->name +"_"+ std::to_string(index+1);
                int new_obj_id = m_model->objects.size() - 1;
                for ( size_t new_instance_id = 0; new_instance_id < newObj->instances.size(); new_instance_id++ )
                {
                    obj_to_instance_set.emplace(std::pair(new_obj_id, new_instance_id));
                    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": duplicate object into plate: index_pair [%1%,%2%], obj_id %3%") % new_obj_id % new_instance_id % newObj->id().id;
                }
            }
        }
    }

    for (std::set<std::pair<int, int>>::iterator it = obj_to_instance_set.begin(); it != obj_to_instance_set.end(); ++it)
    {
        int obj_id = it->first;
        int instance_id = it->second;

        if ((obj_id >= 0) && (obj_id < m_model->objects.size()))
        {
            ModelObject* object = m_model->objects[obj_id];
            ModelInstance* instance = object->instances[instance_id];

            if (instance->printable)
            {
                instance->loaded_id = instance->id().id;
                if (need_skip) {
                    while (skip_objects.find(instance->loaded_id) != skip_objects.end())
                    {
                        instance->loaded_id ++;
                        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": duplicated id %1% with skip, try new one %2%") %instance->id().id  % instance->loaded_id;
                    }
                }
                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": set obj %1% instance %2%'s loaded_id to its id %3%, name %4%") % obj_id %instance_id %instance->loaded_id  % object->name;
            }
        }
    }

    return;
}



//update instance exclude state
void PartPlate::update_instance_exclude_status(int obj_id, int instance_id, BoundingBoxf3* bounding_box)
{
	bool outside;
	std::set<std::pair<int, int>>::iterator it;

	outside = check_outside(obj_id, instance_id, bounding_box);

	it = instance_outside_set.find(std::pair(obj_id, instance_id));
	if (it == instance_outside_set.end()) {
		if (outside)
			instance_outside_set.insert(std::pair(obj_id, instance_id));
	}
	else {
		if (!outside)
			instance_outside_set.erase(it);
	}
}

//update object's index caused by original object deleted
void PartPlate::update_object_index(int obj_idx_removed, int obj_idx_max)
{
	std::set<std::pair<int, int>> temp_set;
	std::set<std::pair<int, int>>::iterator it;
	//update the obj_to_instance_set
	for (it = obj_to_instance_set.begin(); it != obj_to_instance_set.end(); ++it)
	{
		if (it->first >= obj_idx_removed)
			temp_set.insert(std::pair(it->first-1, it->second));
		else
			temp_set.insert(std::pair(it->first, it->second));
	}
	obj_to_instance_set.clear();
	obj_to_instance_set = temp_set;

	//update the instance_outside_set
	temp_set.clear();
	for (it = instance_outside_set.begin(); it != instance_outside_set.end(); ++it)
	{
		if (it->first >= obj_idx_removed)
			temp_set.insert(std::pair(it->first - 1, it->second));
		else
			temp_set.insert(std::pair(it->first, it->second));
	}
	instance_outside_set.clear();
	instance_outside_set = temp_set;

}

void PartPlate::set_vase_mode_related_object_config(int obj_id) {
	ModelObjectPtrs obj_ptrs;
	if (obj_id != -1) {
		ModelObject* object = m_model->objects[obj_id];
		obj_ptrs.push_back(object);
	}
	else
		obj_ptrs = get_objects_on_this_plate();

	DynamicPrintConfig* global_config = &app_preset_bundle()->prints.get_edited_preset().config;
	DynamicPrintConfig new_conf;
	new_conf.set_key_value("wall_loops", new ConfigOptionInt(1));
	new_conf.set_key_value("top_shell_layers", new ConfigOptionInt(0));
	new_conf.set_key_value("sparse_infill_density", new ConfigOptionPercent(0));
	new_conf.set_key_value("enable_support", new ConfigOptionBool(false));
	new_conf.set_key_value("enforce_support_layers", new ConfigOptionInt(0));
	new_conf.set_key_value("detect_thin_wall", new ConfigOptionBool(false));
	new_conf.set_key_value("timelapse_type", new ConfigOptionEnum<TimelapseType>(tlTraditional));
	new_conf.set_key_value("overhang_reverse", new ConfigOptionBool(false));
	new_conf.set_key_value("wall_direction", new ConfigOptionEnum<WallDirection>(WallDirection::Auto));
	auto applying_keys = global_config->diff(new_conf);

	for (ModelObject* object : obj_ptrs) {
		ModelConfigObject& config = object->config;

		for (auto opt_key : applying_keys) {
			config.set_key_value(opt_key, new_conf.option(opt_key)->clone());
		}

		applying_keys = config.get().diff(new_conf);
		for (auto opt_key : applying_keys) {
			config.set_key_value(opt_key, new_conf.option(opt_key)->clone());
		}
	}
	//AppAdapter::obj_list()->update_selections();
}

int PartPlate::printable_instance_size()
{
    int size = 0;
    for (std::set<std::pair<int, int>>::iterator it = obj_to_instance_set.begin(); it != obj_to_instance_set.end(); ++it) {
        int obj_id      = it->first;
        int instance_id = it->second;

        if (obj_id >= m_model->objects.size())
			continue;

        ModelObject *  object   = m_model->objects[obj_id];
        ModelInstance *instance = object->instances[instance_id];

        if ((instance->printable) && (instance_outside_set.find(std::pair(obj_id, instance_id)) == instance_outside_set.end())) {
            size++;
        }
    }
    return size;
}

//whether it is has printable instances
bool PartPlate::has_printable_instances()
{
	bool result = false;

	for (std::set<std::pair<int, int>>::iterator it = obj_to_instance_set.begin(); it != obj_to_instance_set.end(); ++it)
	{
		int obj_id = it->first;
		int instance_id = it->second;

		if (obj_id >= m_model->objects.size())
			continue;

		ModelObject* object = m_model->objects[obj_id];
		ModelInstance* instance = object->instances[instance_id];

		if ((instance->printable)&&(instance_outside_set.find(std::pair(obj_id, instance_id)) == instance_outside_set.end()))
		{
			result = true;
			break;
		}
	}

	return result;
}

bool PartPlate::is_all_instances_unprintable()
{
    bool result = true;

    for (std::set<std::pair<int, int>>::iterator it = obj_to_instance_set.begin(); it != obj_to_instance_set.end(); ++it) {
        int obj_id      = it->first;
        int instance_id = it->second;

        if (obj_id >= m_model->objects.size()) continue;

        ModelObject *  object   = m_model->objects[obj_id];
        ModelInstance *instance = object->instances[instance_id];

        if ((instance->printable)) {
            result = false;
            break;
        }
    }

    return result;
}

//move instances to left or right PartPlate
void PartPlate::move_instances_to(PartPlate& left_plate, PartPlate& right_plate, BoundingBoxf3* bounding_box)
{
	for (std::set<std::pair<int, int>>::iterator it = obj_to_instance_set.begin(); it != obj_to_instance_set.end(); ++it)
	{
		int obj_id = it->first;
		int instance_id = it->second;

		if (left_plate.intersect_instance(obj_id, instance_id, bounding_box))
			left_plate.add_instance(obj_id, instance_id, false, bounding_box);
		else
			right_plate.add_instance(obj_id, instance_id, false, bounding_box);
	}

	return;
}

const std::vector<Pointfs>& PartPlate::get_shape() const
{ 
	return m_plate_bed.get_shape();
}

bool PartPlate::set_shape(const std::vector<Pointfs>& shape_group, const std::vector<Pointfs>& exclude_areas_group, Vec2d position, float height_to_lid, float height_to_rod)
{
	return m_plate_bed.set_shape(shape_group, exclude_areas_group, position, height_to_lid, height_to_rod);
}

const BoundingBoxf3& PartPlate::get_bounding_box(bool extended)
{
	return m_plate_bed.get_bounding_box(extended);
}

const std::vector<BoundingBoxf3>& PartPlate::get_exclude_areas()
{
	return m_plate_bed.get_exclude_areas();
}

std::vector<BoundingBoxf3> PartPlate::get_exclude_bounding_box()
{
	return m_plate_bed.get_exclude_bounding_box();
}

const BoundingBox PartPlate::get_bounding_box_crd()
{
	return m_plate_bed.get_bounding_box_crd();
}

BoundingBoxf3 PartPlate::get_build_volume()
{
	return m_plate_bed.get_build_volume();
}

BoundingBoxf3 PartPlate::get_plate_box() 
{
	return get_build_volume();
}

bool PartPlate::check_intersects(const BoundingBoxf3& box)
{
	return m_plate_bed.check_intersects(box);
}

bool PartPlate::contains(const Vec3d& point) const
{
	return m_plate_bed.contains(point);
}

bool PartPlate::contains(const GLVolume& v) const
{
	return m_plate_bed.contains(v);
}

bool PartPlate::contains(const BoundingBoxf3& bb) const
{
	return m_plate_bed.contains(bb);
}

bool PartPlate::intersects(const BoundingBoxf3& bb) const
{
	return m_plate_bed.intersects(bb);
}

void PartPlate::render(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom, bool only_body, bool force_background_color, HeightLimitMode mode, int hover_id, bool render_cali, bool show_grid)
{
	PlateBed::RenderConfig config;
	config.view_matrix = view_matrix;
	config.projection_matrix = projection_matrix;
	config.bottom = bottom;
	config.only_body = only_body;
	config.force_background_color = force_background_color;
	config.mode = (PlateBed::HeightLimitMode)mode;
	config.hover_id = hover_id;
	config.render_cali = render_cali;
	config.show_grid = show_grid;
	config.is_locked = is_locked();
	config.plate_id = m_plate_index;
	config.max_count = MAX_PLATE_COUNT;
	config.scale_factor = m_scale_factor;
	config.has_plate_settings = get_bed_type() != BedType::btDefault || get_print_seq() != PrintSequence::ByDefault || !get_first_layer_print_sequence().empty() || !get_other_layers_print_sequence().empty() || has_spiral_mode_config();
	config.is_dark = m_partplate_list->m_is_dark;
	config.bed_type = get_bed_type();

	m_plate_bed.render(config);
}

void PartPlate::set_selected() {
	m_selected = true;
	m_plate_bed.set_selected(true);
	m_plate_bed.set_position(m_plate_bed.m_position);
}

void PartPlate::set_unselected() {
	m_selected = false;
	m_plate_bed.set_selected(false);
}


/*status related functions*/
//update status
void PartPlate::update_states()
{
	//currently let judge outside partplate when plate is empty
	/*if (obj_to_instance_set.size() == 0)
	{
		m_ready_for_slice = false;
		return;
	}*/
	m_ready_for_slice = true;
	for (std::set<std::pair<int, int>>::iterator it = obj_to_instance_set.begin(); it != obj_to_instance_set.end(); ++it) {
		int obj_id = it->first;
		int instance_id = it->second;

		//if (check_outside(obj_id, instance_id))
		if (instance_outside_set.find(std::pair(obj_id, instance_id)) != instance_outside_set.end())
		{
			m_ready_for_slice = false;
			//currently only check whether ready to slice
			break;
		}
	}

	BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": plate %1% , m_ready_for_slice changes to %2%") % m_plate_index %m_ready_for_slice;
	return;
}

bool PartPlate::is_slice_result_ready_for_print() const
{
	bool result = m_slice_result_valid;
	if (result)
		result = m_gcode_result ? (!m_gcode_result->get_toolpath_outside()) : false;// && !m_gcode_result->conflict_result.has_value()  gcode conflict can also print
	return result;
}

/*slice related functions*/
//invalid sliced result
void PartPlate::update_slice_result_valid_state(bool valid)
{
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": plate %1% , update slice result from %2% to %3%") % m_plate_index %m_slice_result_valid %valid;
    m_slice_result_valid = valid;
    if (valid)
        m_slice_percent = 100.0f;
    else {
        m_slice_percent = -1.0f;
    }
}

//update current slice context into backgroud slicing process
void PartPlate::update_slice_context(BackgroundSlicingProcess & process)
{
	process.set_current_plate(this, m_model, m_plater->config());
}

std::vector<const GCodeProcessorResult*> PartPlate::get_slice_result() 
{ 
	return m_gcode_result->get_const_all_result(); 
}

GCodeResultWrapper* PartPlate::get_slice_result_wrapper() 
{ 
	return m_gcode_result; 
}

std::string PartPlate::get_temp_config_3mf_path()
{
	if (m_temp_config_3mf_path.empty()) {
		boost::filesystem::path temp_path(m_model->get_backup_path("Metadata"));
		temp_path /= (boost::format(".%1%.%2%_config.3mf") % get_current_pid() %
			GLOBAL_PLATE_INDEX++).str();
		m_temp_config_3mf_path = temp_path.string();

	}
	return m_temp_config_3mf_path;
}

// load gcode from file
int PartPlate::load_gcode_from_file(const std::string& filename)
{
	int ret = 0;

	// process gcode
	DynamicPrintConfig full_config = app_preset_bundle()->full_config();
	full_config.apply(m_config, true);
	full_config.apply(*m_plater->config());

	if (boost::filesystem::exists(filename)) {
		m_gcode_result->filename() = filename;

		update_slice_result_valid_state(true);

		BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": found valid gcode file %1%") % filename.c_str();
	}
	else {
		BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(": can not find gcode file %1%") % filename.c_str();
		ret = -1;
	}

	m_ready_for_slice = true;
	return ret;
}

int PartPlate::load_thumbnail_data(std::string filename, ThumbnailData& thumb_data)
{
	bool result = true;
	wxImage img;
	if (boost::algorithm::iends_with(filename, ".png")) {
		result = img.LoadFile(wxString::FromUTF8(filename.c_str()), wxBITMAP_TYPE_PNG);
		img = img.Mirror(false);
	}
	if (result) {
		thumb_data.set(img.GetWidth(), img.GetHeight());
		for (int i = 0; i < img.GetWidth() * img.GetHeight(); i++) {
			memcpy(&thumb_data.pixels[4 * i], (unsigned char*)(img.GetData() + 3 * i), 3);
			if (img.HasAlpha()) {
				thumb_data.pixels[4 * i + 3] = *(unsigned char*)(img.GetAlpha() + i);
			}
		}
	} else {
		return -1;
	}
	return 0;
}

//load pattern box data from file
int PartPlate::load_pattern_box_data(std::string filename)
{
    try {
        nlohmann::json j;
        boost::nowide::ifstream ifs(filename);
        ifs >> j;

        PlateBBoxData bbox_data;
        bbox_data.from_json(j);
        cali_bboxes_data = bbox_data;
        return 0;
    }
    catch(std::exception &ex) {
        BOOST_LOG_TRIVIAL(trace) << boost::format("catch an exception %1%")%ex.what();
        return -1;
    }
}

std::vector<int> PartPlate::get_first_layer_print_sequence() const
{
    const ConfigOptionInts *op_print_sequence_1st = m_config.option<ConfigOptionInts>("first_layer_print_sequence");
    if (op_print_sequence_1st)
        return op_print_sequence_1st->values;
    else
        return std::vector<int>();
}

std::vector<LayerPrintSequence> PartPlate::get_other_layers_print_sequence() const
{
	const ConfigOptionInts* other_layers_print_sequence_op = m_config.option<ConfigOptionInts>("other_layers_print_sequence");
	const ConfigOptionInt* other_layers_print_sequence_nums_op = m_config.option<ConfigOptionInt>("other_layers_print_sequence_nums");
	if (other_layers_print_sequence_op && other_layers_print_sequence_nums_op) {
		const std::vector<int>& print_sequence = other_layers_print_sequence_op->values;
		int sequence_nums = other_layers_print_sequence_nums_op->value;
		auto other_layers_seqs = Slic3r::get_other_layers_print_sequence(sequence_nums, print_sequence);
		return other_layers_seqs;
	}
	else
		return {};
}

void PartPlate::set_first_layer_print_sequence(const std::vector<int>& sorted_filaments)
{
    if (sorted_filaments.size() > 0) {
		if (sorted_filaments.size() == 1 && sorted_filaments[0] == 0) {
            m_config.erase("first_layer_print_sequence");
        }
		else {
            ConfigOptionInts *op_print_sequence_1st = m_config.option<ConfigOptionInts>("first_layer_print_sequence");
            if (op_print_sequence_1st)
                op_print_sequence_1st->values = sorted_filaments;
            else
                m_config.set_key_value("first_layer_print_sequence", new ConfigOptionInts(sorted_filaments));
        }
    }
	else {
        m_config.erase("first_layer_print_sequence");
	}
}

void PartPlate::set_other_layers_print_sequence(const std::vector<LayerPrintSequence>& layer_seq_list)
{
	if (layer_seq_list.empty()) {
		m_config.erase("other_layers_print_sequence");
		m_config.erase("other_layers_print_sequence_nums");
		return;
	}

	int sequence_nums;
	std::vector<int> other_layers_seqs;
	Slic3r::get_other_layers_print_sequence(layer_seq_list, sequence_nums, other_layers_seqs);
	ConfigOptionInts* other_layers_print_sequence_op = m_config.option<ConfigOptionInts>("other_layers_print_sequence");
	ConfigOptionInt* other_layers_print_sequence_nums_op = m_config.option<ConfigOptionInt>("other_layers_print_sequence_nums");
	if (other_layers_print_sequence_op)
		other_layers_print_sequence_op->values = other_layers_seqs;
	else
		m_config.set_key_value("other_layers_print_sequence", new ConfigOptionInts(other_layers_seqs));
	if (other_layers_print_sequence_nums_op)
		other_layers_print_sequence_nums_op->value = sequence_nums;
	else
		m_config.set_key_value("other_layers_print_sequence_nums", new ConfigOptionInt(sequence_nums));
}

void PartPlate::update_first_layer_print_sequence(size_t filament_nums)
{
	auto other_layers_seqs = get_other_layers_print_sequence();
	if (!other_layers_seqs.empty()) {
		bool need_update_data = false;
		for (auto& other_layers_seq : other_layers_seqs) {
			std::vector<int>& orders = other_layers_seq.second;
			if (orders.size() > filament_nums) {
				orders.erase(std::remove_if(orders.begin(), orders.end(), [filament_nums](int n) { return n > filament_nums; }), orders.end());
				need_update_data = true;
			}
			if (orders.size() < filament_nums) {
				for (size_t extruder_id = orders.size(); extruder_id < filament_nums; ++extruder_id) {
					orders.push_back(extruder_id + 1);
					need_update_data = true;
				}
			}
		}
		if (need_update_data)
			set_other_layers_print_sequence(other_layers_seqs);
	}


    ConfigOptionInts * op_print_sequence_1st = m_config.option<ConfigOptionInts>("first_layer_print_sequence");
    if (!op_print_sequence_1st) {
		return;
	}

    std::vector<int> &print_sequence_1st = op_print_sequence_1st->values;
    if (print_sequence_1st.size() == 0 || print_sequence_1st[0] == 0)
		return;

	if (print_sequence_1st.size() > filament_nums) {
        print_sequence_1st.erase(std::remove_if(print_sequence_1st.begin(), print_sequence_1st.end(), [filament_nums](int n) { return n > filament_nums; }),
                                 print_sequence_1st.end());
    }
	else if (print_sequence_1st.size() < filament_nums) {
        for (size_t extruder_id = print_sequence_1st.size(); extruder_id < filament_nums; ++extruder_id) {
            print_sequence_1st.push_back(extruder_id + 1);
		}
    }
}

}//end namespace GUI
}//end namespace slic3r
