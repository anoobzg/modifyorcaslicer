#ifndef slic3r_GUI_NetworkTestDialog_hpp_
#define slic3r_GUI_NetworkTestDialog_hpp_

#include <boost/thread.hpp>
#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/wxExtensions.hpp"

namespace Slic3r { 
namespace GUI {

enum TestJob {
	TEST_BING_JOB = 0,
	TEST_ORCA_JOB = 1,
	TEST_PING_JOB,
	TEST_JOB_MAX
};

class Button;
class NetworkTestDialog : public DPIDialog
{
protected:
	Button* btn_start;
	Button* btn_start_sequence;
	Button* btn_download_log;
	wxStaticText* text_basic_info;
	wxStaticText* text_version_title;
	wxStaticText* text_version_val;
	wxStaticText* txt_sys_info_title;
	wxStaticText* txt_sys_info_value;
	wxStaticText* txt_dns_info_title;
	wxStaticText* txt_dns_info_value;
	Button*     btn_link;
	wxStaticText* text_link_title;
	wxStaticText* text_link_val;
	Button*     btn_bing;
	wxStaticText* text_bing_title;
	wxStaticText* text_bing_val;
	wxStaticText* text_ping_title;
	wxStaticText* text_ping_value;
	wxStaticText* text_result;
	wxTextCtrl* txt_log;

	wxBoxSizer* create_top_sizer(wxWindow* parent);
	wxBoxSizer* create_info_sizer(wxWindow* parent);
	wxBoxSizer* create_content_sizer(wxWindow* parent);
	wxBoxSizer* create_result_sizer(wxWindow* parent);

	boost::thread* test_job[TEST_JOB_MAX];
	boost::thread* m_sequence_job { nullptr };
	bool		   m_in_testing[TEST_JOB_MAX];
	bool           m_download_cancel = false;
	bool           m_closing = false;

	void init_bind();

public:
	NetworkTestDialog(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxEmptyString, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(605, 375), long style = wxDEFAULT_DIALOG_STYLE);

	~NetworkTestDialog();

	void on_dpi_changed(const wxRect& suggested_rect);

	void set_default();
	wxString get_studio_version();
	wxString get_os_info();
	wxString get_dns_info();

	void start_all_job();
	void start_all_job_sequence();
	void start_test_bing_thread();
	void start_test_github_thread();
	void start_test_ping_thread();

	void start_test_url(TestJob job, wxString name, wxString url);

	void on_close(wxCloseEvent& event);

	void update_status(int job_id, wxString info);
};

} // namespace GUI
} // namespace Slic3r

#endif
