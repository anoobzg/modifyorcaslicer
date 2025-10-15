# FullControl C++

## 简介

FullControl C++ 是 FullControl Python 库的 C++ 实现版本，用于3D打印机路径控制和G代码生成。它允许用户完全控制打印路径，生成各种几何形状，并输出为标准的G代码文件。

## 特性

- **完全控制**: 精确控制3D打印机的每个移动和挤出动作
- **几何形状生成**: 支持矩形、圆形、椭圆、多边形、螺旋等多种几何形状
- **G代码生成**: 自动生成标准的3D打印机G代码
- **高性能**: C++实现，提供更好的性能和内存效率
- **跨平台**: 支持Windows、Linux、macOS
- **现代C++**: 使用C++17标准，提供类型安全和现代语法

## 系统要求

- C++17 兼容编译器 (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.16+
- Google Test (用于测试，可选)

## 安装

### 从源码构建

```bash
# 克隆仓库
git clone <repository-url>
cd cpp_fullcontrol

# 创建构建目录
mkdir build
cd build

# 配置和构建
cmake ..
make -j$(nproc)

# 运行测试（可选）
make test
```

### Windows (Visual Studio)

```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release
```

## 快速开始

### 基本示例

```cpp
#include "fullcontrol/fullcontrol.h"
#include <iostream>

int main() {
    // 创建点
    fullcontrol::Point origin(0.0, 0.0, 0.0);
    fullcontrol::Point end(10.0, 10.0, 0.0);
    
    // 创建打印机配置
    fullcontrol::Printer printer(60.0, 120.0); // 打印速度60，移动速度120
    
    // 生成矩形
    auto rectangle = fullcontrol::rectangleXY(origin, 20.0, 15.0);
    
    // 生成G代码
    std::vector<std::shared_ptr<void>> steps;
    for (const auto& point : rectangle) {
        steps.push_back(std::make_shared<fullcontrol::Point>(point));
    }
    
    fullcontrol::GcodeControls controls;
    controls.save_as = "my_print";
    
    std::string gcode = fullcontrol::GcodeGenerator::generateGcode(steps, controls);
    
    std::cout << "G代码已生成！" << std::endl;
    return 0;
}
```

### 几何形状示例

```cpp
// 生成圆形
auto circle = fullcontrol::circleXY(center, 10.0, 0.0, 32);

// 生成椭圆
auto ellipse = fullcontrol::ellipseXY(center, 15.0, 8.0, 0.0, 24);

// 生成六边形
auto hexagon = fullcontrol::polygonXY(center, 12.0, 0.0, 6);

// 生成螺旋
auto spiral = fullcontrol::spiralXY(center, 5.0, 20.0, 0.0, 2.0, 40);

// 生成螺旋线
auto helix = fullcontrol::helixZ(center, 10.0, 10.0, 0.0, 3.0, 5.0, 60);
```

## API 文档

### 核心类

#### Point
表示3D空间中的一个点，支持可选坐标。

```cpp
fullcontrol::Point point(1.0, 2.0, 3.0);
double distance = point.distanceTo(other_point);
Point sum = point1 + point2;
```

#### Printer
表示3D打印机的配置参数。

```cpp
fullcontrol::Printer printer(60.0, 120.0); // 打印速度，移动速度
printer.setPrintSpeed(80.0);
```

### 几何函数

#### 基本形状
- `rectangleXY()` - 生成矩形
- `circleXY()` - 生成圆形
- `ellipseXY()` - 生成椭圆
- `polygonXY()` - 生成多边形
- `spiralXY()` - 生成2D螺旋
- `helixZ()` - 生成3D螺旋线

#### 弧线函数
- `arcXY()` - 生成弧线
- `variable_arcXY()` - 生成可变半径弧线
- `elliptical_arcXY()` - 生成椭圆弧线

### G代码生成

```cpp
fullcontrol::GcodeControls controls;
controls.save_as = "output";
controls.include_date = true;
controls.layer_height = 0.2;

std::string gcode = fullcontrol::GcodeGenerator::generateGcode(steps, controls);
```

## 示例程序

项目包含多个示例程序：

- `basic_example` - 基础功能演示
- `shapes_example` - 几何形状生成演示

运行示例：
```bash
./bin/basic_example
./bin/shapes_example
```

## 测试

运行测试套件：
```bash
cd build
make test
```

或者直接运行测试程序：
```bash
./bin/fullcontrol_tests
```

## 项目结构

```
cpp_fullcontrol/
├── include/                 # 头文件
│   └── fullcontrol/
│       ├── core/           # 核心类
│       ├── geometry/       # 几何函数
│       ├── gcode/          # G代码生成
│       └── visualize/      # 可视化
├── src/                    # 源文件
│   ├── core/
│   ├── geometry/
│   ├── gcode/
│   └── visualize/
├── examples/               # 示例程序
├── tests/                  # 测试文件
├── CMakeLists.txt         # CMake配置
└── README.md              # 本文档
```

## 与Python版本的差异

- **类型安全**: C++版本提供编译时类型检查
- **性能**: 更好的运行时性能
- **内存管理**: 更精确的内存控制
- **API设计**: 更符合C++习惯的API设计

## 贡献

欢迎贡献代码！请遵循以下步骤：

1. Fork 项目
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 打开 Pull Request

## 许可证

本项目采用 GPL v3 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情。

## 致谢

- 基于 [FullControl Python](https://github.com/FullControlXYZ/fullcontrol) 项目
- 感谢原作者的杰出工作

## 联系方式

- 项目主页: [GitHub Repository]
- 问题报告: [GitHub Issues]
- 邮箱: [contact@fullcontrol.xyz]

## 版本历史

### v1.0.0
- 初始版本
- 实现核心Point和Printer类
- 实现基本几何形状生成
- 实现G代码生成功能
- 添加示例程序和测试
