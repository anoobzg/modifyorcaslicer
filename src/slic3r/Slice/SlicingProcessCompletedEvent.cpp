#include "SlicingProcessCompletedEvent.hpp"
#include "libslic3r/Exception.hpp"

#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/format.hpp"

namespace Slic3r {

bool SlicingProcessCompletedEvent::critical_error() const
{
	try {
		this->rethrow_exception();
	} catch (const Slic3r::SlicingError &) {
		// Exception derived from SlicingError is non-critical.
		return false;
    } catch (const Slic3r::SlicingErrors &) {
        return false;
    } catch (...) {}
    return true;
}

bool SlicingProcessCompletedEvent::invalidate_plater() const
{
	if (critical_error())
	{
		try {
			this->rethrow_exception();
		}
		catch (const Slic3r::ExportError&) {
			// Exception thrown by copying file does not ivalidate plater
			return false;
		}
		catch (...) {
		}
		return true;
	}
	return false;
}

std::pair<std::string, std::vector<size_t>> SlicingProcessCompletedEvent::format_error_message() const
{
	std::string error;
    size_t      monospace = 0;
	try {
		this->rethrow_exception();
    } catch (const std::bad_alloc &ex) {
        wxString errmsg = GUI::from_u8(boost::format(_utf8(L("A error occurred. Maybe memory of system is not enough or it's a bug "
			                  "of the program"))).str());
        error = std::string(errmsg.ToUTF8()) + "\n" + std::string(ex.what());
    } catch (const HardCrash &ex) {
        error = GUI::format(_u8L("A fatal error occurred: \"%1%\""), ex.what()) + "\n" +
                            _u8L("Please save project and restart the program.");
    } catch (PlaceholderParserError &ex) {
		error = ex.what();
		monospace = 1;
    } catch (SlicingError &ex) {
		error = ex.what();
		monospace = ex.objectId();
    } catch (SlicingErrors &exs) {
        std::vector<size_t> ids;
        for (auto &ex : exs.errors_) {
            error     = ex.what();
            monospace = ex.objectId();
            ids.push_back(monospace);
        }
        return std::make_pair(std::move(error), ids);
    } catch (std::exception &ex) {
        error = ex.what();
    } catch (...) {
        error = "Unknown C++ exception.";
    }
    return std::make_pair(std::move(error), std::vector<size_t>{monospace});
}

}