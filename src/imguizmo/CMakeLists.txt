cmake_minimum_required(VERSION 3.15)
project(imguizmo)

add_library(imguizmo STATIC
    ImGuizmo.h
    ImGuizmo.cpp
)

target_link_libraries(imguizmo PRIVATE imgui)
