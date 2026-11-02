cmake_minimum_required(VERSION 3.21)

# Old subprojects (ogg, zlib) declare very old minimum versions
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Static CRT: the plugin must not depend on the Visual C++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")
include(target_add_versioninfo)

project(GothicTrainer
	DESCRIPTION "Trainer pro Gothic II: Noc Havrana (Union plugin)"
	VERSION 0.1.0.0
)

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
