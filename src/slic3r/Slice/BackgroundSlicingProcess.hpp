#ifndef slic3r_GUI_BackgroundSlicingProcess_hpp_
#define slic3r_GUI_BackgroundSlicingProcess_hpp_

#include "libslic3r/PrintBase.hpp"

namespace boost { namespace filesystem { class path; } }

namespace Slic3r {
namespace GUI {
	class Plater;
	class PartPlate;
	class GCodeResultWrapper;
}
class DynamicPrintConfig;
class Model;
class SLAPrint;

class SlicingStatusEvent : public wxEvent
{
public:
	SlicingStatusEvent(wxEventType eventType, int winid, const PrintBase::SlicingStatus& status) :
		wxEvent(winid, eventType), status(std::move(status)) {}
	virtual wxEvent *Clone() const { return new SlicingStatusEvent(*this); }

	PrintBase::SlicingStatus status;
};

// Support for the GUI background processing (Slicing and G-code generation).
// As of now this class is not declared in Slic3r::GUI due to the Perl bindings limits.
class BackgroundSlicingProcess
{
public:
	BackgroundSlicingProcess();
	// Stop the background processing and finalize the bacgkround processing thread, remove temp files.
	~BackgroundSlicingProcess();

	void set_thumbnail_cb(ThumbnailsGeneratorCallback cb);
	//BBS: add partplate related logic
	bool can_switch_print();

	void set_current_plate(GUI::PartPlate* plate, Model* model, const DynamicPrintConfig* plater_config);
	GUI::PartPlate* get_current_plate();

	// Get the current print. It is either m_fff_print.
	const PrintBase*    current_print() const;
	const Print* 		fff_print() const;
	Print* 				fff_print();
	GUI::GCodeResultWrapper* gcode_result_wrapper();

	// Start the background processing. Returns false if the background processing was already running.
	bool start(bool force_start = false);
	// Cancel the background processing. Returns false if the background processing was not running.
	// A stopped background processing may be restarted with start().
	bool stop();
	// Cancel the background processing and reset the print. Returns false if the background processing was not running.
	// Useful when the Model or configuration is being changed drastically.
	bool reset();

	// Apply config over the print. Returns false, if the new config values caused any of the already
	// processed steps to be invalidated, therefore the task will need to be restarted.
    PrintBase::ApplyStatus apply();

	// After calling apply, the empty() call will report whether there is anything to slice.
	bool 		empty() const;
	// Validate the print. Returns an empty string if valid, returns an error message if invalid.
	// Call validate before calling start().
    StringObjectException validate(StringObjectException *warning = nullptr, Polygons* collison_polygons = nullptr, std::vector<std::pair<Polygon, float>>* height_polygons = nullptr);

	enum State {
		// m_thread  is not running yet, or it did not reach the STATE_IDLE yet (it does not wait on the condition yet).
		STATE_INITIAL = 0,
		// m_thread is waiting for the task to execute.
		STATE_IDLE,
		STATE_STARTED,
		// m_thread is executing a task.
		STATE_RUNNING,
		// m_thread finished executing a task, and it is waiting until the UI thread picks up the results.
		STATE_FINISHED,
		// m_thread finished executing a task, the task has been canceled by the UI thread, therefore the UI thread will not be notified.
		STATE_CANCELED,
		// m_thread exited the loop and it is going to finish. The UI thread should join on m_thread.
		STATE_EXIT,
		STATE_EXITED,
	};
	State 	state() 	const { return m_state; }
	bool    idle() 		const { return m_state == STATE_IDLE; }
	bool    running() 	const { return m_state == STATE_STARTED || m_state == STATE_RUNNING || m_state == STATE_FINISHED || m_state == STATE_CANCELED; }
    // Returns true if the last step of the active print was finished with success.
    // The "finished" flag is reset by the apply() method, if it changes the state of the print.
    // This "finished" flag does not account for the final export of the output file (.gcode or zipped PNGs),
    // and it does not account for the OctoPrint scheduling.
    //BBS: improve the finished logic, also judge the m_gcode_result
    //bool    finished() const { return m_print->finished(); }
	bool    finished() const;
    bool    is_internal_cancelled() { return m_internal_cancelled; }

    //BBS: add Plater to friend class
    //need to call stop_internal in ui thread
    friend class GUI::Plater;

private:
	void 	thread_proc();
	// Calls thread_proc(), catches all C++ exceptions and shows them using wxApp::OnUnhandledException().
	void 	thread_proc_safe() throw();
#ifdef _WIN32
	// Wrapper for Win32 structured exceptions. Win32 structured exception blocks and C++ exception blocks cannot be mixed in the same function.
	// Catch a SEH exception and return its ID or zero if no SEH exception has been catched.
	unsigned long 	thread_proc_safe_seh() throw();
	// Calls thread_proc_safe_seh(), rethrows a Slic3r::HardCrash exception based on SEH exception
	// returned by thread_proc_safe_seh() and lets wxApp::OnUnhandledException() display it.
	void 			thread_proc_safe_seh_throw() throw();
#endif // _WIN32
	void 	join_background_thread();
	// To be called by Print::apply() through the Print::m_cancel_callback to stop the background
	// processing before changing any data of running or finalized milestones.
	// This function shall not trigger any UI update through the wxWidgets event.
	void	stop_internal();

	// Helper to wrap the FFF slicing & G-code generation.
	void	process_fff();

    // Call Print::process() and catch all exceptions into ex, thus no exception could be thrown
    // by this method. This exception behavior is required to combine C++ exceptions with Win32 SEH exceptions
    // on the same thread.
	void    call_process(std::exception_ptr &ex) throw();

#ifdef _WIN32
	// Wrapper for Win32 structured exceptions. Win32 structured exception blocks and C++ exception blocks cannot be mixed in the same function.
	// Catch a SEH exception and return its ID or zero if no SEH exception has been catched.
	unsigned long call_process_seh(std::exception_ptr &ex) throw();
	// Calls call_process_seh(), rethrows a Slic3r::HardCrash exception based on SEH exception
	// returned by call_process_seh().
	void    	  call_process_seh_throw(std::exception_ptr &ex) throw();
#endif // _WIN32

	Model* 						m_model				= nullptr;
	DynamicPrintConfig*         m_plater_config     = nullptr;

	// Data structure, to which the G-code export writes its annotations.
	GUI::GCodeResultWrapper     *m_gcode_result_wrapper 		 = nullptr;
	// Callback function, used to write thumbnails into gcode.
	ThumbnailsGeneratorCallback m_thumbnail_cb 	     = nullptr;

	// Thread, on which the background processing is executed. The thread will always be present
	// and ready to execute the slicing process.
	boost::thread		 		m_thread;
	// Mutex and condition variable to synchronize m_thread with the UI thread.
	std::mutex 		 			m_mutex;
	std::condition_variable		m_condition;
	State 						m_state = STATE_INITIAL;

	// For executing tasks from the background thread on UI thread synchronously (waiting for result) using wxWidgets CallAfter().
	// When the background proces is canceled, the UITask has to be invalidated as well, so that it will not be
	// executed on the UI thread referencing invalid data.
    struct UITask {
        enum State {
            Planned,
            Finished,
            Canceled,
        };
        State  					state = Planned;
        std::mutex 				mutex;
    	std::condition_variable	condition;
    };
    // Only one UI task may be planned by the background thread to be executed on the UI thread, as the background
    // thread is blocking until the UI thread calculation finishes.
    std::shared_ptr<UITask> 	m_ui_task;

	//BBS: partplate related
	GUI::PartPlate* m_current_plate;
	bool m_internal_cancelled = false;

    // If the background processing stop was requested, throw CanceledException.
    void                throw_if_canceled() const;
    // To be executed at the background thread.
	ThumbnailsList		render_thumbnails(const ThumbnailsParams &params);
	// Execute task from background thread on the UI thread synchronously. Returns true if processed, false if cancelled before executing the task.
	bool 				execute_ui_task(std::function<void()> task);
	// To be called from inside m_mutex to cancel a planned UI task.
	static void			cancel_ui_task(std::shared_ptr<BackgroundSlicingProcess::UITask> task);
};

}; // namespace Slic3r

#endif /* slic3r_GUI_BackgroundSlicingProcess_hpp_ */
