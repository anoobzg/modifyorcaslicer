cmake_minimum_required(VERSION 3.13)
project(libslic3r)

include(PrecompiledHeader)

string(TIMESTAMP COMPILE_TIME %Y%m%d-%H%M%S)
set(SLIC3R_BUILD_TIME ${COMPILE_TIME})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libslic3r_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/libslic3r_version.cpp @ONLY)

if (MINGW)
    add_compile_options(-Wa,-mbig-obj)
endif ()

set(lisbslic3r_sources
    Config/ThemeDef.hpp
    Config/AppConfigImpl.cpp
    Config/CalibrationConfig.hpp
    Config/PresetLoad.hpp
    Config/PresetLoad.cpp

    Base/AnyPtr.hpp
    Base/clonable_ptr.hpp
    Base/TryCatchSignal.hpp
    Base/TryCatchSignal.cpp
    Base/MutablePriorityQueue.hpp
    Base/Time.cpp
    Base/Time.hpp
    Base/Timer.cpp
    Base/Timer.hpp
    Base/Thread.cpp
    Base/Thread.hpp
    Base/Execution.hpp
    Base/ExecutionSeq.hpp
    Base/ExecutionTBB.hpp
    Base/Concurrency.hpp
    Base/BlacklistedLibraryCheck.cpp
    Base/BlacklistedLibraryCheck.hpp
    Base/Platform.cpp
    Base/Platform.hpp

    Algorithm/LineSplit.hpp
    Algorithm/LineSplit.cpp
    Algorithm/RegionExpansion.hpp
    Algorithm/RegionExpansion.cpp
    Algorithm/Optimizer.hpp
    Algorithm/NLoptOptimizer.hpp
    Algorithm/BruteforceOptimizer.hpp

    ArcFitter.cpp
    ArcFitter.hpp
    pchheader.cpp
    pchheader.hpp
    AABBTreeIndirect.hpp
    AABBTreeLines.hpp
    AABBMesh.hpp
    AABBMesh.cpp
    BoundingBox.cpp
    BoundingBox.hpp
    BridgeDetector.cpp
    BridgeDetector.hpp
    FaceDetector.cpp
    FaceDetector.hpp
    Brim.cpp
    Brim.hpp
    BrimEarsPoint.hpp
    BuildVolume.cpp
    BuildVolume.hpp
    Circle.cpp
    Circle.hpp
    clipper.cpp
    clipper.hpp
    ClipperUtils.cpp
    ClipperUtils.hpp
    Clipper2Utils.cpp
    Clipper2Utils.hpp
    ClipperZUtils.hpp
    Color.cpp
    Color.hpp
    Config.cpp
    Config.hpp
    CurveAnalyzer.cpp
    CurveAnalyzer.hpp
    EdgeGrid.cpp
    EdgeGrid.hpp
    ElephantFootCompensation.cpp
    ElephantFootCompensation.hpp
    Emboss.cpp
    Emboss.hpp
    EmbossShape.hpp
    enum_bitmask.hpp
    ExPolygon.cpp
    ExPolygon.hpp
    ExPolygonSerialize.hpp
    ExPolygonsIndex.cpp
    ExPolygonsIndex.hpp
    Extruder.cpp
    Extruder.hpp
    ExtrusionEntity.cpp
    ExtrusionEntity.hpp
    ExtrusionEntityCollection.cpp
    ExtrusionEntityCollection.hpp
    FileParserError.hpp
    Fill/Fill.cpp
    Fill/Fill.hpp
    Fill/Fill3DHoneycomb.cpp
    Fill/Fill3DHoneycomb.hpp
    Fill/FillAdaptive.cpp
    Fill/FillAdaptive.hpp
    Fill/FillBase.cpp
    Fill/FillBase.hpp
    Fill/FillConcentric.cpp
    Fill/FillConcentric.hpp
    Fill/FillConcentricInternal.cpp
    Fill/FillConcentricInternal.hpp
    Fill/FillCrossHatch.cpp
    Fill/FillCrossHatch.hpp
    Fill/FillHoneycomb.cpp
    Fill/FillHoneycomb.hpp
    Fill/FillGyroid.cpp
    Fill/FillGyroid.hpp
    Fill/FillPlanePath.cpp
    Fill/FillPlanePath.hpp
    Fill/FillLine.cpp
    Fill/FillLine.hpp
    Fill/FillLightning.cpp
    Fill/FillLightning.hpp
    Fill/Lightning/DistanceField.cpp
    Fill/Lightning/DistanceField.hpp
    Fill/Lightning/Generator.cpp
    Fill/Lightning/Generator.hpp
    Fill/Lightning/Layer.cpp
    Fill/Lightning/Layer.hpp
    Fill/Lightning/TreeNode.cpp
    Fill/Lightning/TreeNode.hpp
    Fill/FillRectilinear.cpp
    Fill/FillRectilinear.hpp
    Flow.cpp
    Flow.hpp
    FlushVolCalc.cpp
    FlushVolCalc.hpp
    format.hpp
    Format/3mf.cpp
    Format/3mf.hpp
    Format/bbs_3mf.cpp
    Format/bbs_3mf.hpp
    Format/AMF.cpp
    Format/AMF.hpp
    Format/OBJ.cpp
    Format/OBJ.hpp
    Format/objparser.cpp
    Format/objparser.hpp
    Format/STEP.cpp
    Format/STEP.hpp
    Format/STL.cpp
    Format/STL.hpp
	Format/svg.hpp
    Format/svg.cpp
    Format/ZipperArchiveImport.hpp
    Format/ZipperArchiveImport.cpp
    GCode/ThumbnailData.cpp
    GCode/ThumbnailData.hpp
    GCode/CoolingBuffer.cpp
    GCode/CoolingBuffer.hpp
	GCode/FanMover.cpp
    GCode/FanMover.hpp
    GCode/PostProcessor.cpp
    GCode/PostProcessor.hpp
    GCode/PressureEqualizer.cpp
    GCode/PressureEqualizer.hpp
    GCode/PrintExtents.cpp
    GCode/PrintExtents.hpp
    GCode/RetractWhenCrossingPerimeters.cpp
    GCode/RetractWhenCrossingPerimeters.hpp
    GCode/SmallAreaInfillFlowCompensator.cpp
    GCode/SmallAreaInfillFlowCompensator.hpp
    GCode/PchipInterpolatorHelper.cpp
    GCode/PchipInterpolatorHelper.hpp
    GCode/AdaptivePAInterpolator.cpp
    GCode/AdaptivePAInterpolator.hpp
    GCode/AdaptivePAProcessor.cpp
    GCode/AdaptivePAProcessor.hpp
    GCode/SpiralVase.cpp
    GCode/SpiralVase.hpp
    GCode/SeamPlacer.cpp
    GCode/SeamPlacer.hpp
    GCode/ToolOrdering.cpp
    GCode/ToolOrdering.hpp
    GCode/WipeTower.cpp
    GCode/WipeTower.hpp
    GCode/WipeTower2.cpp
    GCode/WipeTower2.hpp
    GCode/GCodeProcessor.cpp
    GCode/GCodeProcessor.hpp
    GCode/AvoidCrossingPerimeters.cpp
    GCode/AvoidCrossingPerimeters.hpp
    GCode/ExtrusionProcessor.hpp
    GCode/ConflictChecker.cpp
    GCode/ConflictChecker.hpp
    GCode.cpp
    GCode.hpp
    GCodeReader.cpp
    GCodeReader.hpp
    GCodeWriter.cpp
    GCodeWriter.hpp
    Geometry.cpp
    Geometry.hpp
    Geometry/Bicubic.hpp
    Geometry/Circle.cpp
    Geometry/Circle.hpp
    Geometry/ConvexHull.cpp
    Geometry/ConvexHull.hpp
    Geometry/Curves.hpp
    Geometry/MedialAxis.cpp
    Geometry/MedialAxis.hpp
	Geometry/Voronoi.cpp
    Geometry/Voronoi.hpp
    Geometry/VoronoiOffset.cpp
    Geometry/VoronoiOffset.hpp
	Geometry/VoronoiUtils.hpp
    Geometry/VoronoiUtils.cpp
    Geometry/VoronoiUtilsCgal.cpp
    Geometry/VoronoiUtilsCgal.hpp
    Geometry/VoronoiVisualUtils.hpp
    Int128.hpp
    KDTreeIndirect.hpp
    Layer.cpp
    Layer.hpp
    LayerRegion.cpp
    libslic3r.h
    libslic3r.cpp
    Line.cpp
    Line.hpp
    LocalesUtils.cpp
    LocalesUtils.hpp
    CutUtils.cpp
    CutUtils.hpp
    Model.cpp
    Model.hpp
    ModelArrange.hpp
    ModelArrange.cpp
    MultiMaterialSegmentation.cpp
    MultiMaterialSegmentation.hpp
    Measure.hpp
    Measure.cpp
    MeasureUtils.hpp
    CustomGCode.cpp
    CustomGCode.hpp
    Arrange.hpp
    Arrange.cpp
    NormalUtils.cpp
    NormalUtils.hpp
    ObjColorUtils.hpp
    Orient.hpp
    Orient.cpp
    MultiPoint.cpp
    MultiPoint.hpp
    NSVGUtils.cpp
    NSVGUtils.hpp
    ObjectID.cpp
    ObjectID.hpp
	ParameterUtils.cpp
    ParameterUtils.hpp
    PerimeterGenerator.cpp
    PerimeterGenerator.hpp
    PlaceholderParser.cpp
    PlaceholderParser.hpp
    Point.cpp
    Point.hpp
    Polygon.cpp
    Polygon.hpp
    MutablePolygon.cpp
    MutablePolygon.hpp
    PolygonTrimmer.cpp
    PolygonTrimmer.hpp
    Polyline.cpp
    Polyline.hpp
    Preset.cpp
    Preset.hpp
    PresetBundle.cpp
    PresetBundle.hpp
    ProjectTask.cpp
    ProjectTask.hpp
    PrincipalComponents2D.hpp
    PrincipalComponents2D.cpp
    AppConfig.cpp
    AppConfig.hpp
    Print.cpp
    Print.hpp
    PrintApply.cpp
    PrintBase.cpp
    PrintBase.hpp
    PrintConfig.cpp
    PrintConfig.hpp
    PrintObject.cpp
    PrintObjectSlice.cpp
    PrintRegion.cpp
    PNGReadWrite.hpp
    PNGReadWrite.cpp
    QuadricEdgeCollapse.cpp
    QuadricEdgeCollapse.hpp
    Semver.cpp
    ShortEdgeCollapse.cpp
    ShortEdgeCollapse.hpp
    ShortestPath.cpp
    ShortestPath.hpp
    Slicing.cpp
    Slicing.hpp
    SlicesToTriangleMesh.hpp
    SlicesToTriangleMesh.cpp
    SlicingAdaptive.cpp
    SlicingAdaptive.hpp
    Support/SupportCommon.cpp
    Support/SupportCommon.hpp
    Support/SupportLayer.hpp
    Support/SupportMaterial.cpp
    Support/SupportMaterial.hpp
    Support/SupportSpotsGenerator.cpp
    Support/SupportSpotsGenerator.hpp
    Support/TreeSupport.hpp
    Support/TreeSupport.cpp
    Support/TreeSupport3D.hpp
    Support/TreeSupport3D.cpp
    Support/TreeModelVolumes.hpp
    Support/TreeModelVolumes.cpp
    Support/TreeSupportCommon.hpp
    Support/SupportParameters.hpp
    PrincipalComponents2D.cpp
    PrincipalComponents2D.hpp
    MinimumSpanningTree.hpp
    MinimumSpanningTree.cpp
    Surface.cpp
    Surface.hpp
    SurfaceCollection.cpp
    SurfaceCollection.hpp
    SurfaceMesh.hpp
    SVG.cpp
    SVG.hpp
    Technologies.hpp
    Tesselate.cpp
    Tesselate.hpp
    TextConfiguration.hpp
    TriangleMesh.cpp
    TriangleMesh.hpp
    TriangleMeshSlicer.cpp
    TriangleMeshSlicer.hpp
    MeshSplitImpl.hpp
    TriangulateWall.hpp
    TriangulateWall.cpp
    utils.cpp
    Utils.hpp
    TriangleSelector.cpp
    TriangleSelector.hpp
    TriangleSetSampling.cpp
    TriangleSetSampling.hpp
    MTUtils.hpp
    VariableWidth.cpp
    VariableWidth.hpp
    MinAreaBoundingBox.hpp
    MinAreaBoundingBox.cpp
    MarchingSquares.hpp
    SLA/IndexedMesh.hpp
    SLA/IndexedMesh.cpp
    Arachne/BeadingStrategy/BeadingStrategy.hpp
    Arachne/BeadingStrategy/BeadingStrategy.cpp
    Arachne/BeadingStrategy/BeadingStrategyFactory.hpp
    Arachne/BeadingStrategy/BeadingStrategyFactory.cpp
    Arachne/BeadingStrategy/DistributedBeadingStrategy.hpp
    Arachne/BeadingStrategy/DistributedBeadingStrategy.cpp
    Arachne/BeadingStrategy/LimitedBeadingStrategy.hpp
    Arachne/BeadingStrategy/LimitedBeadingStrategy.cpp
    Arachne/BeadingStrategy/OuterWallInsetBeadingStrategy.hpp
    Arachne/BeadingStrategy/OuterWallInsetBeadingStrategy.cpp
    Arachne/BeadingStrategy/RedistributeBeadingStrategy.hpp
    Arachne/BeadingStrategy/RedistributeBeadingStrategy.cpp
    Arachne/BeadingStrategy/WideningBeadingStrategy.hpp
    Arachne/BeadingStrategy/WideningBeadingStrategy.cpp
    Arachne/utils/ExtrusionJunction.hpp
    Arachne/utils/ExtrusionLine.hpp
    Arachne/utils/ExtrusionLine.cpp
    Arachne/utils/HalfEdge.hpp
    Arachne/utils/HalfEdgeGraph.hpp
    Arachne/utils/HalfEdgeNode.hpp
    Arachne/utils/SparseGrid.hpp
    Arachne/utils/SparsePointGrid.hpp
    Arachne/utils/SparseLineGrid.hpp
    Arachne/utils/SquareGrid.hpp
    Arachne/utils/SquareGrid.cpp
    Arachne/utils/PolygonsPointIndex.hpp
    Arachne/utils/PolygonsSegmentIndex.hpp
    Arachne/utils/PolylineStitcher.hpp
    Arachne/utils/PolylineStitcher.cpp
    Arachne/SkeletalTrapezoidation.hpp
    Arachne/SkeletalTrapezoidation.cpp
    Arachne/SkeletalTrapezoidationEdge.hpp
    Arachne/SkeletalTrapezoidationGraph.hpp
    Arachne/SkeletalTrapezoidationGraph.cpp
    Arachne/SkeletalTrapezoidationJoint.hpp
    Arachne/WallToolPaths.hpp
    Arachne/WallToolPaths.cpp
    calib.hpp
    calib.cpp
    GCode/Thumbnails.cpp
    GCode/Thumbnails.hpp
    Interlocking/InterlockingGenerator.hpp
    Interlocking/InterlockingGenerator.cpp
    Interlocking/VoxelUtils.hpp
    Interlocking/VoxelUtils.cpp

    FileSystem/DataDir.hpp
    FileSystem/DataDir.cpp
    FileSystem/Log.hpp
    FileSystem/Log.cpp
    FileSystem/ASCIIFolding.hpp
    FileSystem/ASCIIFolding.cpp
    FileSystem/FileHelp.hpp
    FileSystem/FileHelp.cpp
    FileSystem/HexFile.hpp
    FileSystem/HexFile.cpp
    FileSystem/LogUtil.hpp
    FileSystem/LogUtil.cpp
    FileSystem/json_diff.hpp
    FileSystem/json_diff.cpp

    Zip/Zipper.hpp
    Zip/Zipper.cpp
    Zip/miniz_extension.hpp
    Zip/miniz_extension.cpp
    Zip/minilzo_extension.hpp
    Zip/minilzo_extension.cpp
)

if (APPLE)
    list(APPEND lisbslic3r_sources
			MacUtils.mm
            Format/ModelIO.hpp
            Format/ModelIO.mm
        )
endif ()

if (TARGET OpenVDB::openvdb)
    list(APPEND lisbslic3r_sources OpenVDBUtils.cpp OpenVDBUtils.hpp)
endif()

list(APPEND lisbslic3r_sources 
    CutSurface.hpp CutSurface.cpp
    IntersectionPoints.hpp IntersectionPoints.cpp
    MeshBoolean.hpp MeshBoolean.cpp
    Triangulation.hpp Triangulation.cpp
)

add_library(libslic3r STATIC ${lisbslic3r_sources} 
    "${CMAKE_CURRENT_BINARY_DIR}/libslic3r_version.cpp"
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${lisbslic3r_sources})

if (SLIC3R_STATIC)
    set(CGAL_Boost_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
endif ()
set(CGAL_DO_NOT_WARN_ABOUT_CMAKE_BUILD_TYPE ON CACHE BOOL "" FORCE)

get_target_property(_cgal_tgt CGAL::CGAL ALIASED_TARGET)
if (NOT TARGET ${_cgal_tgt})
    set (_cgal_tgt CGAL::CGAL)
endif ()
get_target_property(_opts ${_cgal_tgt} INTERFACE_COMPILE_OPTIONS)
if (_opts)
    set(_opts_bad "${_opts}")
    set(_opts_good "${_opts}")
    list(FILTER _opts_bad INCLUDE REGEX frounding-math)
    list(FILTER _opts_good EXCLUDE REGEX frounding-math)
    set_target_properties(${_cgal_tgt} PROPERTIES INTERFACE_COMPILE_OPTIONS "${_opts_good}")
    target_compile_options(libslic3r PRIVATE "${_opts_bad}")
endif()

if (MSVC AND "${CMAKE_SIZEOF_VOID_P}" STREQUAL "4") # 32 bit MSVC workaround
    target_compile_definitions(libslic3r PRIVATE CGAL_DO_NOT_USE_MPZF)
endif ()

encoding_check(libslic3r)

target_compile_definitions(libslic3r PUBLIC -DUSE_TBB -DTBB_USE_CAPTURED_EXCEPTION=0)
target_include_directories(libslic3r PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(libslic3r PUBLIC ${OpenCASCADE_INCLUDE_DIR})

target_link_libraries(libslic3r
    libnest2d
    admesh
    cereal::cereal
    libigl
    miniz
    boost_libs
    clipper
    nowide
    ${EXPAT_LIBRARIES}
    glu-libtess
    qhull
    semver
    TBB::tbb
    TBB::tbbmalloc
    ${CMAKE_DL_LIBS}
    PNG::PNG
    ZLIB::ZLIB
    ${OCCT_LIBS}
    Clipper2
    ${_cgal_tgt}
    mcut
    JPEG::JPEG
    qoi
    opencv_world
    noise::noise
    OpenSSL::SSL
    OpenSSL::Crypto
    )

if(NOT WIN32)
    target_link_libraries(libslic3r freetype)
    if (NOT APPLE)
        target_link_libraries(libslic3r fontconfig)
    endif()
endif()

if (APPLE)
    find_library(FOUNDATION Foundation REQUIRED)
    find_library(MODELIO ModelIO REQUIRED)
    target_link_libraries(libslic3r ${FOUNDATION} ${MODELIO})
endif ()

if (TARGET OpenVDB::openvdb)
    target_link_libraries(libslic3r OpenVDB::openvdb)
endif()

if(WIN32)
    target_link_libraries(libslic3r Psapi.lib)
endif()

if (SLIC3R_PCH)
    add_precompiled_header(libslic3r pchheader.hpp FORCEINCLUDE)
endif ()
