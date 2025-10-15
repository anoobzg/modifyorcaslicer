#ifndef slic3r_calib_dlg_hpp_
#define slic3r_calib_dlg_hpp_

#include "slic3r/GUI/wxExtensions.hpp"
#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/Widgets/RadioBox.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/CheckBox.hpp"
#include "slic3r/GUI/Widgets/ComboBox.hpp"
#include "slic3r/GUI/Widgets/TextInput.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "wx/hyperlink.h"
#include <wx/radiobox.h>
#include "libslic3r/calib.hpp"

namespace Slic3r { namespace GUI {

class Plater;

class PA_Calibration_Dlg : public DPIDialog
{
public:
    PA_Calibration_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    ~PA_Calibration_Dlg();
    void on_dpi_changed(const wxRect& suggested_rect) override;
	void on_show(wxShowEvent& event);
protected:
    void reset_params();
	virtual void on_start(wxCommandEvent& event);
	virtual void on_extruder_type_changed(wxCommandEvent& event);
	virtual void on_method_changed(wxCommandEvent& event);

protected:
	bool m_bDDE;
	Calib_Params m_params;


	wxRadioBox* m_rbExtruderType;
	wxRadioBox* m_rbMethod;
	TextInput* m_tiStartPA;
	TextInput* m_tiEndPA;
	TextInput* m_tiPAStep;
	CheckUIBox* m_cbPrintNum;
	TextInput* m_tiBMAccels;
	TextInput* m_tiBMSpeeds;
	Button* m_btnStart;

	Plater* m_plater;
};

class Temp_Calibration_Dlg : public DPIDialog
{
public:
    Temp_Calibration_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    ~Temp_Calibration_Dlg();
    void on_dpi_changed(const wxRect& suggested_rect) override;

protected:
    
    virtual void on_start(wxCommandEvent& event);
    virtual void on_filament_type_changed(wxCommandEvent& event);
    Calib_Params m_params;

    wxRadioBox* m_rbFilamentType;
    TextInput* m_tiStart;
    TextInput* m_tiEnd;
    TextInput* m_tiStep;
    Button* m_btnStart;
    Plater* m_plater;
};

class MaxVolumetricSpeed_Test_Dlg : public DPIDialog
{
public:
    MaxVolumetricSpeed_Test_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    ~MaxVolumetricSpeed_Test_Dlg();
    void on_dpi_changed(const wxRect& suggested_rect) override;

protected:

    virtual void on_start(wxCommandEvent& event);
    Calib_Params m_params;

    TextInput* m_tiStart;
    TextInput* m_tiEnd;
    TextInput* m_tiStep;
    Button* m_btnStart;
    Plater* m_plater;
};

class VFA_Test_Dlg : public DPIDialog {
public:
    VFA_Test_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    ~VFA_Test_Dlg();
    void on_dpi_changed(const wxRect& suggested_rect) override;

protected:
    virtual void on_start(wxCommandEvent& event);
    Calib_Params m_params;

    TextInput* m_tiStart;
    TextInput* m_tiEnd;
    TextInput* m_tiStep;
    Button* m_btnStart;
    Plater* m_plater;
};


class Retraction_Test_Dlg : public DPIDialog
{
public:
    Retraction_Test_Dlg (wxWindow* parent, wxWindowID id, Plater* plater);
    ~Retraction_Test_Dlg ();
    void on_dpi_changed(const wxRect& suggested_rect) override;

protected:

    virtual void on_start(wxCommandEvent& event);
    Calib_Params m_params;

    TextInput* m_tiStart;
    TextInput* m_tiEnd;
    TextInput* m_tiStep;
    Button* m_btnStart;
    Plater* m_plater;
};

class MultiNozzle_Calibration_Dlg : public DPIDialog
{
public:
    MultiNozzle_Calibration_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    ~MultiNozzle_Calibration_Dlg();
    void on_dpi_changed(const wxRect& suggested_rect) override;

protected:
    virtual void on_start(wxCommandEvent& event);
    Calib_Params m_params;

    TextInput* m_tiLength;
    TextInput* m_tiWidth;
    TextInput* m_tiPrintHeight;
    TextInput* m_tiSpacing;
    Button* m_btnStart;
    Plater* m_plater;
};

}} // namespace Slic3r::GUI

#endif
