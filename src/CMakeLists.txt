project(Upspring)

# Git Version
include(GetGitRevisionDescription)
git_describe(UPSPRING_VERSION)

add_executable (${PROJECT_NAME})

set(CXX_STANDARD_REQUIRED ON)
set(CXX_STANDARD 20)
set(CXX_EXTENSIONS ON)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC _HAS_STD_BYTE=0)
endif ()

target_compile_definitions(${PROJECT_NAME} PUBLIC UPSPRING_VERSION="${UPSPRING_VERSION}")

find_package(GLEW REQUIRED)
find_package(DevIL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Boost REQUIRED COMPONENTS regex)
find_package(OpenGL REQUIRED)
find_package(Lua REQUIRED)

add_subdirectory(swig)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../scripts DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../runscripts DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries (${PROJECT_NAME} fltk2)

target_link_libraries (${PROJECT_NAME}
    ZLIB::ZLIB
    GLEW::GLEW
    OpenGL::GL
    OpenGL::GLU
    DevIL::IL
    DevIL::ILU
    DevIL::ILUT

    # IL
    # ILU
    # ILUT

    lib3ds
    7zip

    minizip-ng
    spdlog
    simdutf::simdutf
    CLI11
    TXPKCore
)

target_link_libraries (${PROJECT_NAME}
    ${Boost_LIBRARIES}
    ${LUA_LIBRARIES}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

if ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
    target_compile_definitions(${PROJECT_NAME} PUBLIC "DEBUG")
endif ()

target_sources(${PROJECT_NAME} PRIVATE
    Atlas/atlas.cpp
    Atlas/atlas.hpp
    FileIO/3DO.cpp
    FileIO/3DS.cpp
    FileIO/OBJ.cpp
    FileIO/S3O.cpp
    FileIO/S3O.h
    FileSystem/CSevenZipArchive.cpp
    FileSystem/CSevenZipArchive.h
    FileSystem/CZipArchive.cpp
    FileSystem/CZipArchive.h
    FileSystem/IArchive.h
    FileSystem/IArchive.cpp
    math/hash.h
    math/Mathlib.cpp
    math/Mathlib.h
    AVIGenerator.h
    CfgParser.cpp
    CfgParser.h
    config.cpp
    config.h
    AnimKeyTrackView.h
    AnimTrackEditor.cpp
    AnimTrackEditorCB.h
    Animation.cpp
    Animation.h
    AnimationUI.cpp
    AnimationUI.h
    CurvedSurface.cpp
    CurvedSurface.h
    DebugTrace.cpp
    DebugTrace.h
    Editor.cpp
    EditorDef.h
    EditorIncl.h
    EditorUI.cpp
    EditorUI.h
    FileDialog.cpp
    FileSearch.h
    Fltk.h
    IEditor.h
    IK.cpp
    IKViewer.h
    IK_CB.h
    IK_UI.cpp
    IView.h
    Image.cpp
    Image.h
    MappingCB.h
    MdlObject.cpp
    MeshIterators.h
    Model.cpp
    Model.h
    ModelDrawer.cpp
    ModelDrawer.h
    ObjectView.cpp
    ObjectView.h
    PolyMesh.cpp
    Referenced.h
    RotatorUI.cpp
    Spline.h
    string_util.h
    TexBuilderCB.h
    TexBuilderUI.cpp
    TexGroupCB.h
    TexGroupUI.cpp
    Texture.cpp
    Texture.h
    TextureBrowser.cpp
    TextureBrowser.h
    Timeline.cpp
    TimelineCB.h
    Tools.cpp
    Tools.h
    UVMappingUI.cpp
    Util.cpp
    Util.h
    VertexBuffer.cpp
    VertexBuffer.h
    View.cpp
    View.h
    ViewsGroup.h
    nv_dds.cpp
    nv_dds.h
    pch.cpp
    ptrvec.h
    swig/ScriptInterface.h
)

message(STATUS "Build Upspring: ${UPSPRING_VERSION}")