#pragma once
#include "imgui.h"
namespace GT {
inline void DrawPointer(ImDrawList* draw, ImVec2 p) {
 draw->AddTriangleFilled(p, ImVec2(p.x+5,p.y+23), ImVec2(p.x+17,p.y+16), IM_COL32(0,0,0,255));
 draw->AddTriangleFilled(ImVec2(p.x+2,p.y+3), ImVec2(p.x+6,p.y+19), ImVec2(p.x+14,p.y+15), IM_COL32(255,225,145,255));
}
}
