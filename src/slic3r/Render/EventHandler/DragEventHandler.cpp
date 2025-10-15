#include "DragEventHandler.hpp"
#include "slic3r/Render/GLCanvas3D.hpp"
#include "slic3r/Scene/PartPlate.hpp"
#include "libslic3r/AppConfig.hpp"
#include "slic3r/GUI/Event/UserCanvasEvent.hpp"

using namespace HMS;
using namespace GUI;
using namespace Slic3r;
 
void DragEventHandler::reset()
{
  
}

void DragEventHandler::set_move_start_threshold_position_2D_as_invalid() 
{ 
	move_start_threshold_position_2D =  Vec2d(INT_MAX, INT_MAX); 
}


bool DragEventHandler::handle(const wxEvent &ea, GUIActionAdapte* aa)
{
	// const wxMouseEvent* mouseEvent = dynamic_cast<const wxMouseEvent*>(&ea);
	// GLCanvas3D* pGLCanvas3D = dynamic_cast<GLCanvas3D*>(&aa);
	// Point pos(mouseEvent->GetX(), mouseEvent->GetY());
	// auto mouse_ray = [&](const Point& mouse_pos)
	// 	{
	// 		float z0 = 0.0f;
	// 		float z1 = 1.0f;
	// 		return Linef3(pGLCanvas3D->_mouse_to_3d(mouse_pos, &z0), pGLCanvas3D->_mouse_to_3d(mouse_pos, &z1));
	// 	};

	// if (mouseEvent == nullptr || pGLCanvas3D == nullptr)
	// 	return false;

	// GLGizmosManager&  gizmos = pGLCanvas3D->get_gizmos_manager();
	// Selection* selection = pGLCanvas3D->get_selection();
	// if (mouseEvent->LeftDown())
	// {
	// 	int volume_idx = pGLCanvas3D->get_first_hover_volume_idx();
	// 	if (volume_idx != -1)
	// 	{
	// 		bool already_selected = selection->contains_volume(volume_idx);
	// 		bool ctrl_down = mouseEvent->CmdDown();
	// 		Selection::IndicesList curr_idxs = selection->get_volume_idxs();

	// 		if (already_selected && ctrl_down)
	// 			selection->remove(volume_idx);
	// 		else {
	// 			selection->add(volume_idx, !ctrl_down, true);
	// 			   move_requires_threshold = !already_selected;
	// 			if (already_selected)
	// 				set_move_start_threshold_position_2D_as_invalid();
	// 			else
	// 			    move_start_threshold_position_2D = Vec2d(pos.x(), pos.y());
	// 		}

	// 		// propagate event through callback
	// 		if (curr_idxs != selection->get_volume_idxs()) {
	// 			if (selection->is_empty())
	// 				gizmos.reset_all_states();
	// 			else
	// 				gizmos.refresh_on_off_state();

	// 			gizmos.update_data();
				
	// 			pGLCanvas3D->post_event(SimpleEvent(EVT_GLCANVAS_OBJECT_SELECT));
	// 			pGLCanvas3D->dirty();
	// 		}

	// 	}
	// 	const GLVolumeCollection& volumes = pGLCanvas3D->get_volumes();
	// 	if (volume_idx != -1) {
	// 		if (mouseEvent->LeftDown() && move_volume_idx == -1) {
	// 			// Only accept the initial position, if it is inside the volume bounding box.
	// 			BoundingBoxf3 volume_bbox = volumes.volumes[volume_idx]->transformed_bounding_box();
	// 			volume_bbox.offset(1.0);
	// 			const bool is_cut_connector_selected = selection->is_any_connector();
	// 			if ((/*!any_gizmo_active ||*/ !mouseEvent->CmdDown()) && volume_bbox.contains(scene_position) && !is_cut_connector_selected) {
	// 				volumes.volumes[volume_idx]->hover = GLVolume::HS_None;
	// 				// The dragging operation is initiated.
	// 				move_volume_idx = volume_idx;
	// 				selection->setup_cache();
	// 				start_position_3D = scene_position;
	// 				//m_sequential_print_clearance_first_displacement = true;
	// 				m_moving = true;
	// 			}
	// 		}
	// 	}
	// }

	// if (mouseEvent->Dragging()

	// 	&& mouseEvent->LeftIsDown()
	// 	&& move_volume_idx != -1
	// 	/*&& m_layers_editing.state == LayersEditing::Unknown*/) {

	// 	//if (m_canvas_type != ECanvasType::CanvasAssembleView) {
	// 		if (!move_requires_threshold) {
	// 			dragging = true;
	// 			Vec3d cur_pos = start_position_3D;
	// 			// we do not want to translate objects if the user just clicked on an object while pressing shift to remove it from the selection and then drag
	// 			if (selection->contains_volume(pGLCanvas3D->get_first_hover_volume_idx())) {
	// 				const Camera& camera = AppAdapter::plater()->get_camera();
	// 				if (std::abs(camera.get_dir_forward()(2)) < EPSILON) {
	// 					// side view -> move selected volumes orthogonally to camera view direction
	// 					Linef3 ray = mouse_ray(pos);
	// 					Vec3d dir = ray.unit_vector();
	// 					// finds the intersection of the mouse ray with the plane parallel to the camera viewport and passing throught the starting position
	// 					// use ray-plane intersection see i.e. https://en.wikipedia.org/wiki/Line%E2%80%93plane_intersection algebric form
	// 					// in our case plane normal and ray direction are the same (orthogonal view)
	// 					// when moving to perspective camera the negative z unit axis of the camera needs to be transformed in world space and used as plane normal
	// 					Vec3d inters = ray.a + (start_position_3D - ray.a).dot(dir) / dir.squaredNorm() * dir;
	// 					// vector from the starting position to the found intersection
	// 					Vec3d inters_vec = inters - start_position_3D;

	// 					Vec3d camera_right = camera.get_dir_right();
	// 					Vec3d camera_up = camera.get_dir_up();

	// 					// finds projection of the vector along the camera axes
	// 					double projection_x = inters_vec.dot(camera_right);
	// 					double projection_z = inters_vec.dot(camera_up);

	// 					// apply offset
	// 					cur_pos = start_position_3D + projection_x * camera_right + projection_z * camera_up;
	// 				}
	// 				else {
	// 					// Generic view
	// 					// Get new position at the same Z of the initial click point.
	// 					float z0 = 0.0f;
	// 					float z1 = 1.0f;
	// 					cur_pos = Linef3(pGLCanvas3D->_mouse_to_3d(pos, &z0), pGLCanvas3D->_mouse_to_3d(pos, &z1)).intersect_plane(start_position_3D(2));
	// 				}
	// 			}

	// 			TransformationType trafo_type;
	// 			trafo_type.set_relative();
	// 			selection->translate(cur_pos - start_position_3D, trafo_type);
	// /*			if ((fff_print()->config().print_sequence == PrintSequence::ByObject))
	// 				update_sequential_clearance();*/
	// 			pGLCanvas3D->dirty();
	// 		}
	// 	//}

	// }
	 
    return true;
}
