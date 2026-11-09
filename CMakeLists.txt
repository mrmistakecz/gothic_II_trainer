cmake_minimum_required(VERSION 3.21)

# Old subprojects (ogg, zlib) declare very old minimum versions
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Static CRT: the plugin must not depend on the Visual C++ redistributable.
# The policy default makes subprojects with very old cmake_minimum_required
# (ogg, vorbis) honor it as well, so every library uses the same runtime.
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")
include(target_add_versioninfo)

project(GothicTrainer
	DESCRIPTION "Trainer pro Gothic II: Noc Havrana (Union plugin)"
	VERSION 0.6.0.0
)

if (NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "Gothic Trainer requires a Windows x86 compiler (vcvars32.bat).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
	# Sources contain Czech text in UTF-8
	add_compile_options(/utf-8)
endif()

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "src/*.cpp")

add_library(${CMAKE_PROJECT_NAME} SHARED)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${SRC})

target_include_directories(${CMAKE_PROJECT_NAME}
	PRIVATE
		BEFORE "userapi/"
		"src/"
)

target_compile_definitions(${CMAKE_PROJECT_NAME}
	PRIVATE
		TRAINER_VERSION="${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
		_CRT_SECURE_NO_WARNINGS
)

if (MSVC)
	# /EHa: C++ destructors also run when a structured exception (e.g. an
	# access violation inside the engine) is caught by the trainer's guards
	target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /EHa)
endif()

target_add_versioninfo(${CMAKE_PROJECT_NAME}
	VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.${PROJECT_VERSION_TWEAK}"
	COMPANY_NAME "Gothic Trainer"
	PRODUCT_NAME "Gothic Trainer"
	FILE_DESCRIPTION "Gothic Trainer - Union plugin"
)

add_subdirectory(dependencies)

add_library(GothicQuestNav SHARED navigation/Plugin.cpp src/Core/Paths.cpp src/Core/Text.cpp src/Core/Seh.cpp)
target_include_directories(GothicQuestNav PRIVATE src)
target_link_libraries(GothicQuestNav PRIVATE union user32)
target_compile_options(GothicQuestNav PRIVATE /EHa)
target_compile_definitions(GothicQuestNav PRIVATE _CRT_SECURE_NO_WARNINGS)

option(GOTHIC_TRAINER_BUILD_TESTS "Build standalone trainer regression tests" OFF)
if (GOTHIC_TRAINER_BUILD_TESTS)
	enable_testing()
	add_executable(TrainerCoreTests
		tests/CoreTests.cpp tests/TestPaths.cpp tests/RendererSmoke.cpp
		src/Core/Settings.cpp src/Core/Text.cpp src/Core/Log.cpp
		src/Core/Overlay.cpp src/Core/ImGuiDX7.cpp src/Core/Seh.cpp)
	target_include_directories(TrainerCoreTests PRIVATE src dependencies/union/gothic-api)
	target_compile_definitions(TrainerCoreTests PRIVATE TRAINER_VERSION="${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}" _CRT_SECURE_NO_WARNINGS)
	target_compile_options(TrainerCoreTests PRIVATE /EHa)
	target_link_libraries(TrainerCoreTests PRIVATE imgui user32)
	add_test(NAME TrainerCoreRegression COMMAND TrainerCoreTests)
	add_test(NAME TrainerDirectX7Smoke COMMAND TrainerCoreTests --renderer-smoke)
	set_tests_properties(TrainerDirectX7Smoke PROPERTIES SKIP_RETURN_CODE 77)
	add_executable(QuestMapTests tests/MapTests.cpp)
	add_test(NAME QuestMapGeometry COMMAND QuestMapTests)
endif()
