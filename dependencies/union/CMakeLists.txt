# Only Gothic II: Night of the Raven (2.6 fix) is supported
set(GOTHIC_API_G2A ON CACHE BOOL "" FORCE)

add_subdirectory(union-api)
add_subdirectory(gothic-api)

add_library(union INTERFACE)

target_link_libraries(union
	INTERFACE
		gothic_api
		union_api_lib
)
