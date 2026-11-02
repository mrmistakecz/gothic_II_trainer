add_library(imgui STATIC
	"imgui/imgui.cpp"
	"imgui/imgui_draw.cpp"
	"imgui/imgui_tables.cpp"
	"imgui/imgui_widgets.cpp"
)

target_include_directories(imgui PUBLIC "imgui/")

target_compile_definitions(imgui
	PUBLIC
		IMGUI_DISABLE_OBSOLETE_FUNCTIONS
)
