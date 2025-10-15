#include "UICustomEventHandler.hpp"
#include "slic3r/Render/GLCanvas3D.hpp"
#include "libslic3r/AppConfig.hpp"
#include "slic3r/Scene/PartPlate.hpp"

using namespace HMS;
using namespace GUI;
using namespace Slic3r;
void UICustomEventHandler::reset()
{
  
}

bool UICustomEventHandler::handle(const wxEvent &ea, GUIActionAdapte* aa)
{
	// const wxMouseEvent* mouseEvent = dynamic_cast<const wxMouseEvent*>(&ea);
	// GLCanvas3D* pGLCanvas3D = dynamic_cast<GLCanvas3D*>(&aa);
	// GLGizmosManager& gizmos = pGLCanvas3D->get_gizmos_manager();

	// Point pos(mouseEvent->GetX(), mouseEvent->GetY());

	// if (mouseEvent == nullptr || pGLCanvas3D == nullptr)
	// 	return false;

	
	//ImGuiWrapper& imgui = global_im_gui();
	//if (imgui.update_mouse_data(*mouseEvent)) {
	//	_tooltip.set_in_imgui(true);
	//	render();
	//	return true;
	//}

	//if (_main_toolbar.on_mouse(*mouseEvent, *pGLCanvas3D))
	//	return true;


	//if (_assemble_view_toolbar.on_mouse(*mouseEvent, *pGLCanvas3D))
	//	return true;

	//if (_collapse_toolbar.on_mouse(*mouseEvent, *pGLCanvas3D))
	//	return true;
	
	return false;
}
