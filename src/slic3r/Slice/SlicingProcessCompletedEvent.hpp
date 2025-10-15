#pragma once 

namespace Slic3r {

class SlicingProcessCompletedEvent : public wxEvent
{
public:
	enum StatusType {
		Finished,
		Cancelled,
		Error
	};

	SlicingProcessCompletedEvent(wxEventType eventType, int winid, StatusType status, std::exception_ptr exception) :
		wxEvent(winid, eventType), m_status(status), m_exception(exception) {}
	virtual wxEvent* Clone() const { return new SlicingProcessCompletedEvent(*this); }

	StatusType 	status()    const { return m_status; }
	bool 		finished()  const { return m_status == Finished; }
	bool 		success()   const { return m_status == Finished; }
	bool 		cancelled() const { return m_status == Cancelled; }
	bool		error() 	const { return m_status == Error; }
	// Unhandled error produced by stdlib or a Win32 structured exception, or unhandled Slic3r's own critical exception.
	bool 		critical_error() const;
	// Critical errors does invalidate plater except CopyFileError.
	bool        invalidate_plater() const;
	// Only valid if error()
	void 		rethrow_exception() const { assert(this->error()); assert(m_exception); std::rethrow_exception(m_exception); }
	// Produce a human readable message to be displayed by a notification or a message box.
	// 2nd parameter defines whether the output should be displayed with a monospace font.
    std::pair<std::string, std::vector<size_t>> format_error_message() const;

private:
	StatusType 			m_status;
	std::exception_ptr 	m_exception;
};

}