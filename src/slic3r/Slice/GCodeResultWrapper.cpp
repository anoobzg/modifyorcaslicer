#include "GCodeResultWrapper.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Print.hpp"

namespace Slic3r {

namespace GUI {

GCodeResultWrapper::GCodeResultWrapper(Model* model)
{
	m_model = model;
	resize(1);
	m_agent = m_results[0];

}


GCodeResult* GCodeResultWrapper::get_result(int id)
{
	if (m_results.size() > id)
		return m_results[id];

	return NULL;
}

Print* GCodeResultWrapper::get_print(int id)
{
	if (m_prints.size() > id)
		return m_prints[id];

	return NULL;
}

void GCodeResultWrapper::resize(int size)
{
	if (size <= 0)
		size = 1;

	while (size < m_results.size())
	{
		const GCodeResult* result = m_results.back();
		m_results.pop_back();
		delete result;

		Print* print = m_prints.back();
		m_prints.pop_back();
		delete print;
	}

	while (size > m_results.size())
	{	
		GCodeResult* result = new GCodeResult;
		m_results.push_back(result);

		Print* print = new Print();
		m_prints.push_back(print);
	}

    if (m_area_path_prefix.empty()) 
	{
        boost::filesystem::path temp_path(m_model->get_backup_path("Metadata"));
        temp_path /= (boost::format(".%1%.") % get_current_pid()).str();
		m_area_path_prefix = temp_path.string();
    }

	m_area_paths.clear();
	for (int i = 0, count = this->size(); i < count; ++i)
	{
		std::string path = m_area_path_prefix + std::to_string(i) + ".gcode";
		m_area_paths.push_back(path);
	}

	m_agent = m_results[0];
}

std::vector<const GCodeResult*> GCodeResultWrapper::get_const_all_result() const
{
	std::vector<const GCodeResult*> all_result;
	for (auto result : m_results)
		all_result.push_back(result);
	return all_result;
}

std::vector<GCodeResult*> GCodeResultWrapper::get_all_result()
{
	return m_results;
}

std::vector<Print*> GCodeResultWrapper::get_prints()
{
	return m_prints;
}

int GCodeResultWrapper::size() const
{
	return m_results.size();
}

void GCodeResultWrapper::reset()
{
	for (auto result : m_results)
		result->reset();
}

float GCodeResultWrapper::max_time(int type)
{
	auto all_result = get_all_result();
	if (all_result.empty())
		return 0;

	float max_time = 0; 
	for (int i = 0; i < all_result.size(); ++i)
	{
		float time = all_result[i]->print_statistics.modes[type].time;
		max_time = max_time > time ? max_time : time;
	}
	return max_time; 
}

bool GCodeResultWrapper::is_valid()
{
	int size = m_results.size();
	if (size <= 0)
		return false;
		
	for (int i = 0; i < size; ++i)
	{
		if (!m_results[i]->moves.empty())
			return true;
	}
	return false;
}

bool GCodeResultWrapper::get_toolpath_outside()
{ 
	bool outside = false;
	for (auto result : m_results)
	{
		if (result->toolpath_outside && result->moves.size() > 0)
		{
			outside = true;
			break;
		}
	}
	return outside;
}

std::vector<std::string> GCodeResultWrapper::get_area_gcode_paths()
{
	return m_area_paths;
}

};
};