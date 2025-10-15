#include "BackgroundSlicingProcess.hpp"

#include "libslic3r/Utils.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/Base/Thread.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/GCode/PostProcessor.hpp"

#include "slic3r/GUI/Event/UserPlaterEvent.hpp"

#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/format.hpp"
#include "slic3r/Global/AppModule.hpp"
#include "slic3r/Config/AppPreset.hpp"
#include "slic3r/Scene/PartPlate.hpp"

#include "slic3r/Slice/GCodeResultWrapper.hpp"

#include "SlicingProcessCompletedEvent.hpp"

namespace Slic3r {
using namespace GUI;

BackgroundSlicingProcess::BackgroundSlicingProcess()
{
}

BackgroundSlicingProcess::~BackgroundSlicingProcess()
{
	this->stop();
	this->join_background_thread();
}

void BackgroundSlicingProcess::set_thumbnail_cb(ThumbnailsGeneratorCallback cb) 
{ 
	m_thumbnail_cb = cb; 
}

//BBS: judge whether can switch the print
bool BackgroundSlicingProcess::can_switch_print()
{
	bool result = true;

	if (m_state == STATE_RUNNING)
	{
		result = false;
	}

	return result;
}

void BackgroundSlicingProcess::set_current_plate(GUI::PartPlate* plate, Model* model, const DynamicPrintConfig* plater_config) 
{ 
	m_current_plate = plate; 
	m_gcode_result_wrapper = m_current_plate->get_gcode_result();

	m_model = model;
	m_plater_config = (DynamicPrintConfig*)plater_config;
}

GUI::PartPlate* BackgroundSlicingProcess::get_current_plate() 
{ 
	return m_current_plate; 
}

const PrintBase*    BackgroundSlicingProcess::current_print() const 
{ 
	return nullptr; 
}

GUI::GCodeResultWrapper* BackgroundSlicingProcess::gcode_result_wrapper()
{
	return m_gcode_result_wrapper;
}

const Print* 		BackgroundSlicingProcess::fff_print() const 
{ 
	std::vector<Print*> prints = m_gcode_result_wrapper->get_prints();
	return prints.at(0); 
}

Print* 				BackgroundSlicingProcess::fff_print() 
{ 
	std::vector<Print*> prints = m_gcode_result_wrapper->get_prints();
	return prints.at(0); 
}

// This function may one day be merged into the Print, but historically the print was separated
// from the G-code generator.
void BackgroundSlicingProcess::process_fff()
{
	get_current_plate()->set_slicing_progress_index(get_current_plate()->get_index());

	std::vector<Print*> prints = m_gcode_result_wrapper->get_prints();
	std::vector<GCodeResult*> gcode_results = m_gcode_result_wrapper->get_all_result();
	std::vector<std::string> area_gcode_paths = m_gcode_result_wrapper->get_area_gcode_paths();

	int num_prints = (int)prints.size();

	for (size_t i = 0; i < num_prints; i++)
	{
		Print* print = prints[i];
		GCodeResult* gcode_result = gcode_results[i];
		std::string area_gcode_path = area_gcode_paths[i];

		if (print->empty()) {
			if (i == num_prints - 1)
			{
				BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": export gcode finished");
				print->set_status(100, _utf8(L("Slicing complete")));
			}
			continue;
		}
				
		print->set_cancel_callback([this](){
			this->stop_internal(); 
		});

		get_current_plate()->set_slicing_progress_index(i);
		get_current_plate()->set_slicing_progress();

		//BBS: add the logic to process from an existed gcode file
		if (print->finished()) {
			/* previous load gcode */
		}
		else 
		{
			BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: gcode_result reseted, will start print::process") % __LINE__;
			print->process();
			BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: after print::process, send slicing complete event to gui...") % __LINE__;

			wxCommandEvent evt(EVT_SLICING_COMPLETED);
			// Post the Slicing Finished message for the G-code viewer to update.
			// Passing the timestamp
			evt.SetInt((int)(print->step_state_with_timestamp(PrintStep::psSlicingFinished).timestamp));
			queue_plater_event(evt.Clone());

			//BBS: add plate index into render params
			print->export_gcode(area_gcode_path, gcode_result, [this](const ThumbnailsParams& params) {
				return this->render_thumbnails(params);
				});

			run_post_process_scripts(area_gcode_path, false, "File", area_gcode_path, print->full_print_config());
			// BBS: to be checked. Whether use export_path or output_path.
			gcode_add_line_number(area_gcode_path, print->full_print_config());

			if (i == num_prints - 1) 
			{
				BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": export gcode finished");
				print->set_status(100, _utf8(L("Slicing complete")));
			}
		}

		m_state = print->canceled() ? STATE_CANCELED : STATE_FINISHED;
		BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": process finished, state %1%, print cancel_status %2%")%m_state %print->cancel_status();
		if (print->cancel_status() != Print::CANCELED_INTERNAL) {

		}
		else {
			//BBS: internal cancel
			m_internal_cancelled = true;
		}

		print->set_cancel_callback([this](){});
		print->restart();
	}

	if (m_state == STATE_FINISHED)
	{
		// Only post the canceled event, if canceled by user.
		// Don't post the canceled event, if canceled from Print::apply().
		std::exception_ptr exception = std::current_exception();
		SlicingProcessCompletedEvent evt(EVT_PROCESS_COMPLETED, 0,
			(m_state == STATE_CANCELED) ? SlicingProcessCompletedEvent::Cancelled :
			exception ? SlicingProcessCompletedEvent::Error : SlicingProcessCompletedEvent::Finished, exception);
		BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": send SlicingProcessCompletedEvent to main, status %1%")%evt.status();
		queue_plater_event(evt.Clone());
	}
}

bool BackgroundSlicingProcess::finished() const 
{ 
	bool finished = true;

	std::vector<Print*> prints = m_gcode_result_wrapper->get_prints();
	std::vector<GCodeResult*> gcode_results = m_gcode_result_wrapper->get_all_result();

	int num_prints = (int)prints.size();

	for (size_t i = 0; i < num_prints; i++)
	{
		Print* print = prints[i];
		GCodeResult* gcode_result = gcode_results[i];

		if (!print->finished() || gcode_result->moves.empty())
		{
			finished = false;
			break;
		}
	}

	return finished;
}

void BackgroundSlicingProcess::thread_proc()
{
	//BBS: thread name
	set_current_thread_name("bbl_BgSlcPcs");
    name_tbb_thread_pool_threads_set_locale();

	std::unique_lock<std::mutex> lck(m_mutex);
	// Let the caller know we are ready to run the background processing task.
	m_state = STATE_IDLE;
	lck.unlock();
	m_condition.notify_one();
	for (;;) {
		lck.lock();
		m_condition.wait(lck, [this](){ return m_state == STATE_STARTED || m_state == STATE_EXIT; });
		if (m_state == STATE_EXIT)
			// Exiting this thread.
			break;
		// Process the background slicing task.
		m_state = STATE_RUNNING;
		//BBS: internal cancel
		m_internal_cancelled = false;
		lck.unlock();
		std::exception_ptr exception;
#ifdef _WIN32
		this->call_process_seh_throw(exception);
#else
		this->call_process(exception);
#endif
		// Let the UI thread wake up if it is waiting for the background task to finish.
		m_condition.notify_one();
		// Let the UI thread see the result.
	}
	m_state = STATE_EXITED;
	lck.unlock();
	// End of the background processing thread. The UI thread should join m_thread now.
}

#ifdef _WIN32
// Only these SEH exceptions will be catched and turned into Slic3r::HardCrash C++ exceptions.
static bool is_win32_seh_harware_exception(unsigned long ex) throw() {
	return
		ex == STATUS_ACCESS_VIOLATION ||
		ex == STATUS_DATATYPE_MISALIGNMENT ||
		ex == STATUS_FLOAT_DIVIDE_BY_ZERO ||
		ex == STATUS_FLOAT_OVERFLOW ||
		ex == STATUS_FLOAT_UNDERFLOW ||
#ifdef STATUS_FLOATING_RESEVERED_OPERAND
		ex == STATUS_FLOATING_RESEVERED_OPERAND ||
#endif // STATUS_FLOATING_RESEVERED_OPERAND
		ex == STATUS_ILLEGAL_INSTRUCTION ||
		ex == STATUS_PRIVILEGED_INSTRUCTION ||
		ex == STATUS_INTEGER_DIVIDE_BY_ZERO ||
		ex == STATUS_INTEGER_OVERFLOW ||
		ex == STATUS_STACK_OVERFLOW;
}

// Rethrow some SEH exceptions as Slic3r::HardCrash C++ exceptions.
static void rethrow_seh_exception(unsigned long win32_seh_catched)
{
	if (win32_seh_catched) {
		// Rethrow SEH exception as Slicer::HardCrash.
		if (win32_seh_catched == STATUS_ACCESS_VIOLATION || win32_seh_catched == STATUS_DATATYPE_MISALIGNMENT)
			throw Slic3r::HardCrash(_u8L("Access violation"));
		if (win32_seh_catched == STATUS_ILLEGAL_INSTRUCTION || win32_seh_catched == STATUS_PRIVILEGED_INSTRUCTION)
			throw Slic3r::HardCrash(_u8L("Illegal instruction"));
		if (win32_seh_catched == STATUS_FLOAT_DIVIDE_BY_ZERO || win32_seh_catched == STATUS_INTEGER_DIVIDE_BY_ZERO)
			throw Slic3r::HardCrash(_u8L("Divide by zero"));
		if (win32_seh_catched == STATUS_FLOAT_OVERFLOW || win32_seh_catched == STATUS_INTEGER_OVERFLOW)
			throw Slic3r::HardCrash(_u8L("Overflow"));
		if (win32_seh_catched == STATUS_FLOAT_UNDERFLOW)
			throw Slic3r::HardCrash(_u8L("Underflow"));
#ifdef STATUS_FLOATING_RESEVERED_OPERAND
		if (win32_seh_catched == STATUS_FLOATING_RESEVERED_OPERAND)
			throw Slic3r::HardCrash(_u8L("Floating reserved operand"));
#endif // STATUS_FLOATING_RESEVERED_OPERAND
		if (win32_seh_catched == STATUS_STACK_OVERFLOW)
			throw Slic3r::HardCrash(_u8L("Stack overflow"));
	}
}

// Wrapper for Win32 structured exceptions. Win32 structured exception blocks and C++ exception blocks cannot be mixed in the same function.
unsigned long BackgroundSlicingProcess::call_process_seh(std::exception_ptr &ex) throw()
{
	unsigned long win32_seh_catched = 0;
	__try {
		this->call_process(ex);
	} __except (is_win32_seh_harware_exception(GetExceptionCode())) {
		win32_seh_catched = GetExceptionCode();
	}
	return win32_seh_catched;
}
void BackgroundSlicingProcess::call_process_seh_throw(std::exception_ptr &ex) throw()
{
	unsigned long win32_seh_catched = this->call_process_seh(ex);
	if (win32_seh_catched) {
		// Rethrow SEH exception as Slicer::HardCrash.
		try {
			rethrow_seh_exception(win32_seh_catched);
		} catch (...) {
			ex = std::current_exception();
		}
	}
}
#endif // _WIN32

void BackgroundSlicingProcess::call_process(std::exception_ptr &ex) throw()
{
	try {
		this->process_fff();
	} catch (CanceledException& /* ex */) {
		// Canceled, this is all right.
		ex = std::current_exception();
		BOOST_LOG_TRIVIAL(error) <<__FUNCTION__ << ":got cancelled exception" << std::endl;
	} catch (...) {
		ex = std::current_exception();
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ":got other exception" << std::endl;
	}
}

#ifdef _WIN32
unsigned long BackgroundSlicingProcess::thread_proc_safe_seh() throw()
{
	unsigned long win32_seh_catched = 0;
	__try {
		this->thread_proc_safe();
	} __except (is_win32_seh_harware_exception(GetExceptionCode())) {
		win32_seh_catched = GetExceptionCode();
	}
	return win32_seh_catched;
}
void BackgroundSlicingProcess::thread_proc_safe_seh_throw() throw()
{
	unsigned long win32_seh_catched = this->thread_proc_safe_seh();
	if (win32_seh_catched) {
		// Rethrow SEH exception as Slicer::HardCrash.
		try {
			rethrow_seh_exception(win32_seh_catched);
		} catch (...) {
			wxTheApp->OnUnhandledException();
		}
	}
}
#endif // _WIN32

void BackgroundSlicingProcess::thread_proc_safe() throw()
{
	try {
		this->thread_proc();
	} catch (...) {
		wxTheApp->OnUnhandledException();
   	}
}

void BackgroundSlicingProcess::join_background_thread()
{
	std::unique_lock<std::mutex> lck(m_mutex);
	if (m_state == STATE_INITIAL) {
		// Worker thread has not been started yet.
		assert(! m_thread.joinable());
	} else {
		assert(m_state == STATE_IDLE);
		assert(m_thread.joinable());
		// Notify the worker thread to exit.
		m_state = STATE_EXIT;
		lck.unlock();
		m_condition.notify_one();
		// Wait until the worker thread exits.
		m_thread.join();
	}
}

bool BackgroundSlicingProcess::start(bool force_start)
{
	std::unique_lock<std::mutex> lck(m_mutex);
	if (m_state == STATE_INITIAL) {
		// The worker thread is not running yet. Start it.
		assert(! m_thread.joinable());
		m_thread = create_thread([this]{
#ifdef _WIN32
			this->thread_proc_safe_seh_throw();
#else // _WIN32
			this->thread_proc_safe();
#endif // _WIN32
		});
		// Wait until the worker thread is ready to execute the background processing task.
		m_condition.wait(lck, [this](){ return m_state == STATE_IDLE; });
	}
	assert(m_state == STATE_IDLE || this->running());
	if (this->running())
		// The background processing thread is already running.
		return false;
	if (! this->idle())
		throw Slic3r::RuntimeError("Cannot start a background task, the worker thread is not idle.");
	m_state = STATE_STARTED;
	lck.unlock();
	m_condition.notify_one();
	return true;
}

// To be called on the UI thread.
bool BackgroundSlicingProcess::stop()
{
	BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< ", enter"<<std::endl;
	// m_print->state_mutex() shall NOT be held. Unfortunately there is no interface to test for it.
	std::unique_lock<std::mutex> lck(m_mutex);
	if (m_state == STATE_INITIAL) {
		return false;
	}

	if (m_state == STATE_STARTED || m_state == STATE_RUNNING) {
		// Cancel any task planned by the background thread on UI thread.
		cancel_ui_task(m_ui_task);
		// Wait until the background processing stops by being canceled.
		m_condition.wait(lck, [this](){ return m_state == STATE_CANCELED; });
		// In the "Canceled" state. Reset the state to "Idle".
		m_state = STATE_IDLE;
	} else if (m_state == STATE_FINISHED || m_state == STATE_CANCELED) {
		// In the "Finished" or "Canceled" state. Reset the state to "Idle".
		m_state = STATE_IDLE;
	}
	BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< ", exit"<<std::endl;
	return true;
}

bool BackgroundSlicingProcess::reset()
{
	bool stopped = this->stop();
	return stopped;
}

// To be called by Print::apply() on the UI thread through the Print::m_cancel_callback to stop the background
// processing before changing any data of running or finalized milestones.
// This function shall not trigger any UI update through the wxWidgets event.
void BackgroundSlicingProcess::stop_internal()
{
	BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< ", enter"<<std::endl;
	// m_print->state_mutex() shall be held. Unfortunately there is no interface to test for it.
	if (m_state == STATE_IDLE)
		// The worker thread is waiting on m_mutex/m_condition for wake up. The following lock of the mutex would block.
		return;
	std::unique_lock<std::mutex> lck(m_mutex);
	assert(m_state == STATE_STARTED || m_state == STATE_RUNNING || m_state == STATE_FINISHED || m_state == STATE_CANCELED);
	if (m_state == STATE_STARTED || m_state == STATE_RUNNING) {
		// Cancel any task planned by the background thread on UI thread.
		cancel_ui_task(m_ui_task);
		// At this point of time the worker thread may be blocking on m_print->state_mutex().
		// Set the print state to canceled before unlocking the state_mutex(), so when the worker thread wakes up,
		// it throws the CanceledException().
		// m_print->cancel_internal();
		// Allow the worker thread to wake up if blocking on a milestone.
		// m_print->state_mutex().unlock();
		// Wait until the background processing stops by being canceled.
		m_condition.wait(lck, [this](){ return m_state == STATE_CANCELED; });
		// Lock it back to be in a consistent state.
		// m_print->state_mutex().lock();
	}
	// In the "Canceled" state. Reset the state to "Idle".
	m_state = STATE_IDLE;
	BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< ", exit"<<std::endl;
}

// Execute task from background thread on the UI thread. Returns true if processed, false if cancelled.
bool BackgroundSlicingProcess::execute_ui_task(std::function<void()> task)
{
	bool running = false;
	if (m_mutex.try_lock()) {
		// Cancellation is either not in process, or already canceled and waiting for us to finish.
		// There must be no UI task planned.
		assert(! m_ui_task);
		if (true) {
			running = true;
			m_ui_task = std::make_shared<UITask>();
		}
		m_mutex.unlock();
	} else {
		// Cancellation is in process.
	}

	bool result = false;
	if (running) {
		std::shared_ptr<UITask> ctx = m_ui_task;
		plater_call_after([task, ctx]() {
			// Running on the UI thread, thus ctx->state does not need to be guarded with mutex against ::cancel_ui_task().
			assert(ctx->state == UITask::Planned || ctx->state == UITask::Canceled);
			if (ctx->state == UITask::Planned) {
				task();
				std::unique_lock<std::mutex> lck(ctx->mutex);
	    		ctx->state = UITask::Finished;
	    	}
	    	// Wake up the worker thread from the UI thread.
    		ctx->condition.notify_all();
	    });

	    {
			std::unique_lock<std::mutex> lock(ctx->mutex);
	    	ctx->condition.wait(lock, [&ctx]{ return ctx->state == UITask::Finished || ctx->state == UITask::Canceled; });
	    }
	    result = ctx->state == UITask::Finished;
		m_ui_task.reset();
	}

	return result;
}

// To be called on the UI thread from ::stop() and ::stop_internal().
void BackgroundSlicingProcess::cancel_ui_task(std::shared_ptr<UITask> task)
{
	if (task) {
		std::unique_lock<std::mutex> lck(task->mutex);
		task->state = UITask::Canceled;
		lck.unlock();
		task->condition.notify_all();
	}
}

bool BackgroundSlicingProcess::empty() const
{
	bool rt = true;
	std::vector<Print*> prints = m_gcode_result_wrapper->get_prints();
	int num_prints = (int)prints.size();

	for (size_t i = 0; i < num_prints; i++)
	{
		Print* print = prints[i];
		if (!print->empty())
		{
			rt = false;
			break;
		}
	}
	return rt;
}

StringObjectException BackgroundSlicingProcess::validate(StringObjectException *warning, Polygons* collison_polygons, std::vector<std::pair<Polygon, float>>* height_polygons)
{
	StringObjectException e;
	std::vector<Print*> prints = m_gcode_result_wrapper->get_prints();
	int num_prints = (int)prints.size();

	for (size_t i = 0; i < num_prints; i++)
	{
		Print* print = prints[i];
		e = print->validate(warning, collison_polygons, height_polygons);
		if (!warning->string.empty())
		{
			return e;
		}
	}

	return e;
}

// Apply config over the print. Returns false, if the new config values caused any of the already
// processed steps to be invalidated, therefore the task will need to be restarted.
Print::ApplyStatus BackgroundSlicingProcess::apply()
{
	DynamicPrintConfig new_config = app_preset_bundle()->full_config();
	new_config.apply(*m_plater_config);

	assert(m_current_plate);
	assert(m_gcode_result_wrapper);

	std::vector<Print*> prints = m_gcode_result_wrapper->get_prints();
	std::vector<GCodeResult*> gcode_results = m_gcode_result_wrapper->get_all_result();

	int num_prints = (int)prints.size();

	Print::ApplyStatus invalidated = PrintBase::APPLY_STATUS_UNCHANGED;
	for (int i = 0; i < num_prints; i++)
	{
		Print* print = prints[i];
		GCodeResult* gcode_result = gcode_results[i];

		if (!print || !gcode_result)
		{
			assert(false);
			continue;
		}

		double  print_height = new_config.opt_float("printable_height");
		const std::vector<Pointfs>& pp_bed_shape = m_current_plate->get_shape();

		Pointfs bedfs;
		if(num_prints == 1)
		{
			assert(pp_bed_shape.size() == 1);
			bedfs = pp_bed_shape.at(0);

		}else if(num_prints == 2)
		{
			// IDEX 2-area mode: Print[0]→top-left, Print[1]→top-right
			assert(pp_bed_shape.size() == 4);
			bedfs = i == 0 ? pp_bed_shape.at(3) : pp_bed_shape.at(2);
		}

		if (bedfs.empty()) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << " - bedfs is empty";
			continue;
		}

		BuildVolume build_volume({ bedfs }, print_height);
		m_model->update_print_volume_state(build_volume);
		print->set_plate_origin(Vec3d(bedfs[0][0], bedfs[0][1], 0.f));

		Print::ApplyStatus invalidated_one = print->apply(*m_model, new_config);

		if ((invalidated_one & PrintBase::APPLY_STATUS_INVALIDATED) != 0) {
			if (gcode_result != nullptr)
				gcode_result->reset();
		}

		if ( (invalidated_one != PrintBase::APPLY_STATUS_UNCHANGED ) && 
			((int)invalidated_one > (int)invalidated) )
			invalidated = invalidated_one;
	}

	return invalidated;
}

void BackgroundSlicingProcess::throw_if_canceled() const 
{ 
	// if (m_print->canceled()) 
	// 	throw CanceledException(); 
}

// Executed by the background thread, to start a task on the UI thread.
ThumbnailsList BackgroundSlicingProcess::render_thumbnails(const ThumbnailsParams &params)
{
	ThumbnailsList thumbnails;
	if (m_thumbnail_cb)
		this->execute_ui_task([this, &params, &thumbnails](){
			thumbnails = m_thumbnail_cb(params); 
		});
	return thumbnails;
}

}; // namespace Slic3r
