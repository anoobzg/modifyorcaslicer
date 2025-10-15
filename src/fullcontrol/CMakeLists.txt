cmake_minimum_required(VERSION 3.16)
project(FullControlCpp VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 包含目录
include_directories(include)

# 源文件
set(SOURCES
    src/core/point.cpp
    src/core/printer.cpp
    src/core/state.cpp
    src/geometry/shapes.cpp
    src/geometry/arcs.cpp
    src/geometry/vector.cpp
    src/gcode/gcode_generator.cpp
    src/gcode/commands.cpp
    src/multi_nozzle_calibration/multi_nozzle_calibration.cpp
)

# 头文件
set(HEADERS
    include/fullcontrol/core/point.h
    include/fullcontrol/core/printer.h
    include/fullcontrol/core/state.h
    include/fullcontrol/geometry/shapes.h
    include/fullcontrol/geometry/arcs.h
    include/fullcontrol/geometry/vector.h
    include/fullcontrol/gcode/gcode_generator.h
    include/fullcontrol/gcode/commands.h
    include/fullcontrol/multi_nozzle_calibration/multi_nozzle_calibration.h
    include/fullcontrol/fullcontrol.h
)

# 创建库
add_library(fullcontrol_cpp STATIC ${SOURCES} ${HEADERS})

# 创建示例程序
#add_executable(example_basic examples/basic_example.cpp)
#target_link_libraries(example_basic fullcontrol_cpp)

#add_executable(example_shapes examples/shapes_example.cpp)
#target_link_libraries(example_shapes fullcontrol_cpp)

#add_executable(simple_test examples/simple_test.cpp)
#target_link_libraries(simple_test fullcontrol_cpp)

# 创建整合测试程序
#add_executable(test_integration test_integration.cpp)
#target_link_libraries(test_integration fullcontrol_cpp)

# 安装规则
install(TARGETS fullcontrol_cpp
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)

# 测试
#enable_testing()
# add_subdirectory(tests)  # 注释掉，因为tests目录不存在
