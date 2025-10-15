#include <boost/nowide/fstream.hpp>
#include "LightSlicerApp.hpp"

int win32_main(int argc, wchar_t** argv)
{
     // Convert wchar_t arguments to UTF8.
     std::vector<std::string> 	argv_narrow;
     std::vector<char*>			argv_ptrs(argc + 1, nullptr);
     for (size_t i = 0; i < argc; ++i)
         argv_narrow.emplace_back(boost::nowide::narrow(argv[i]));
     for (size_t i = 0; i < argc; ++i)
         argv_ptrs[i] = argv_narrow[i].data();
  
     LightSlicerApp app;
     return app.exec(argc, argv_ptrs.data());
}
