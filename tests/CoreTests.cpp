#include <windows.h>
#include <cmath>
#include <climits>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include "Core/Settings.h"
#include "Core/Text.h"
#include "Core/Overlay.h"
#include "Core/ToggleHotkey.h"
#include "../navigation/Objectives.h"

namespace {
	void Put(const char* section, const char* key, const char* value) {
		if (!WritePrivateProfileStringA(section, key, value, GT::Settings::IniPath().c_str())) {
			std::fprintf(stderr, "Cannot write isolated test settings.\n");
			std::exit(2);
		}
	}
	// Reproduces PluginStartup: Load() runs before main, in a different TU.
	struct EarlyLoad {
		EarlyLoad() {
			Put("GothicTrainer", nullptr, nullptr);
			Put("GothicTrainer", "ToggleKey", "122");
			Put("GothicTrainer", "FontSize", "24");
			GT::Settings::Load();
		}
	} earlyLoad;
	int checks = 0;
	void Check(bool ok, const char* label) {
		if (!ok) {
			std::fprintf(stderr, "FAIL: %s\n", label);
			std::exit(1);
		}
		++checks;
		std::printf("PASS: %s\n", label);
	}
}

int RendererSmokeTest();

int main(int argc, char** argv) {
	using namespace GT;
	if (argc > 1 && std::string(argv[1]) == "--renderer-smoke")
		return RendererSmokeTest();
	Check(Settings::Get().toggleKey == VK_F11 && Settings::Get().fontSize == 24, "settings loaded during static startup survive until main");
	TrainerSettings& settings = Settings::Get();
	settings.toggleKey = VK_HOME;
	settings.fontSize = 22;
	settings.windowAlpha = 0.83f;
	settings.mouseSpeed = 2.25f;
	settings.systemCursor = true;
	settings.codePage = 1250;
	settings.showToasts = false;
	Settings::Save();
	settings = TrainerSettings{};
	Settings::Load();
	Check(settings.toggleKey == VK_HOME && settings.fontSize == 22, "hotkey and font round trip");
	Check(std::fabs(settings.windowAlpha - 0.83f) < 0.001f && settings.mouseSpeed == 2.25f, "floating point settings round trip");
	Check(settings.systemCursor && !settings.showToasts && settings.codePage == 1250, "booleans and encoding round trip");
	Put("GothicTrainer", "ToggleKey", "0");
	Put("GothicTrainer", "CodePage", "-1");
	Put("GothicTrainer", "FontSize", "999");
	Put("GothicTrainer", "WindowAlpha", "nan");
	Put("GothicTrainer", "MouseSpeed", "nonsense");
	Settings::Load();
	Check(settings.toggleKey == VK_F10, "invalid hotkey keeps trainer accessible with F10");
	Check(settings.codePage == 0 && settings.fontSize == 0, "invalid codepage and font recover");
	Check(settings.windowAlpha == TrainerSettings{}.windowAlpha && settings.mouseSpeed == 1.0f, "nonfinite and malformed settings recover");
	Put("GothicTrainer", "WindowAlpha", "0.8garbage");
	Put("GothicTrainer", "MouseSpeed", "9000");
	Settings::Load();
	Check(settings.windowAlpha == TrainerSettings{}.windowAlpha && settings.mouseSpeed == 1.0f, "trailing junk and out-of-range floats rejected");

	std::setlocale(LC_NUMERIC, "cs-CZ");
	settings.mouseSpeed = 1.125f;
	Settings::Save();
	settings.mouseSpeed = 9.0f;
	Settings::Load();
	Check(settings.mouseSpeed == 1.125f, "settings independent of decimal comma locale");
	std::vector<SavedPosition> positions;
	SavedPosition first;
	first.name = u8"Věž žluťoučký|kůň\n";
	first.x = 12.5f; first.y = -123.25f; first.z = 0.0f;
	positions.push_back(first);
	Settings::SavePositions("NEWWORLD.ZEN", positions);
	auto restored = Settings::LoadPositions("NEWWORLD.ZEN");
	Check(restored.size() == 1 && restored[0].x == 12.5f && restored[0].y == -123.25f, "saved coordinates survive comma locale");
	Check(restored[0].name == u8"Věž žluťoučký/kůň/", "UTF-8 position names round trip and delimiters sanitized");
	Check(Settings::LoadPositions("ADDONWORLD.ZEN").empty(), "positions isolated by world");
	Put("Pozice:BROKEN.ZEN", "Count", "4");
	Put("Pozice:BROKEN.ZEN", "P1", "Bad|nan|0|0");
	Put("Pozice:BROKEN.ZEN", "P2", "Bad|1|2|3garbage");
	Put("Pozice:BROKEN.ZEN", "P3", "Bad|1,5|2|3");
	Put("Pozice:BROKEN.ZEN", "P4", "Good|1.5|2|3");
	Check(Settings::LoadPositions("BROKEN.ZEN").size() == 1, "malformed coordinates never become teleport targets");
	positions[0].x = std::numeric_limits<float>::infinity();
	Settings::SavePositions("NONFINITE.ZEN", positions);
	Check(Settings::LoadPositions("NONFINITE.ZEN").empty(), "nonfinite coordinates not saved");
	positions.assign(510, first);
	Settings::SavePositions("LIMIT.ZEN", positions);
	Check(Settings::LoadPositions("LIMIT.ZEN").size() == 500, "save/load use same position limit");
	std::setlocale(LC_NUMERIC, "C");

	Check(Text::Fold(u8"PŘÍŠERNĚ ŽLUŤOUČKÝ KŮŇ") == "priserne zlutoucky kun", "Czech accent-insensitive search");
	Check(Text::Fold(u8"Straße Łódź") == "strasse lodz", "German and Polish search folding");
	Text::SetCodePage(1250);
	const std::string czech = u8"Meč, zbroj, řetěz";
	Check(Text::FromGame(Text::ToGame(czech).c_str()) == czech, "Czech game text conversion round trip");
	Check(Text::Thousands(0) == "0" && Text::Thousands(100000) == "100 000", "gold formatting");
	Check(Text::Thousands(LLONG_MIN) == "-9 223 372 036 854 775 808", "minimum signed integer formats without overflow");
	Check(Overlay::DikForVk(VK_F10) == 0x44 && Overlay::DikForVk(VK_HOME) == 0xC7, "DirectInput hotkey mapping");
	ToggleHotkey polledFirst;
	Check(polledFirst.Update(true), "F10 opens from polled input");
	polledFirst.KeyDown();
	Check(!polledFirst.Update(true), "delayed Windows key-down cannot close the menu one frame later");
	bool repeated = false;
	for (int i = 0; i < 20; ++i) {
		polledFirst.KeyDown(true);
		repeated = repeated || polledFirst.Update(true);
	}
	Check(!repeated, "holding F10 never toggles repeatedly");
	polledFirst.KeyUp();
	Check(!polledFirst.Update(false), "key release only rearms the hotkey");
	polledFirst.KeyDown();
	Check(polledFirst.Update(true), "next physical F10 press closes the menu once");
	Check(!polledFirst.Update(true), "held second press cannot reopen the menu");
	ToggleHotkey messageFirst;
	messageFirst.KeyDown();
	Check(messageFirst.Update(false) && !messageFirst.Update(true), "Windows-first press and delayed DirectInput merge into one toggle");
	messageFirst.KeyUp();
	Check(!messageFirst.Update(true) && !messageFirst.Update(false), "late DirectInput release does not toggle");
	ToggleHotkey shortPress;
	shortPress.KeyDown();
	shortPress.KeyUp();
	Check(shortPress.Update(false) && !shortPress.Update(false), "tap completed between frames is preserved exactly once");
	ToggleHotkey pollingOnly;
	Check(pollingOnly.Update(true) && !pollingOnly.Update(true) && !pollingOnly.Update(false) && pollingOnly.Update(true), "DirectInput-only fallback handles consecutive presses");
	ToggleHotkey duplicateMessage;
	duplicateMessage.KeyDown();
	duplicateMessage.KeyDown();
	Check(duplicateMessage.Update(true) && !duplicateMessage.Update(true), "duplicate key-down messages cannot double-toggle");
	duplicateMessage.Reset();
	Check(!duplicateMessage.Update(false), "focus or hotkey change clears pending input");
	Overlay::SetVisible(true);
	using namespace QuestObjectives;
	State objective;objective.mission=1;
	Check(Resolve(Harvest,objective).item,"harvest points to beets before collection");
	objective.beets=20;
	Check(std::string(Resolve(Harvest,objective).code)=="BAU_950_LOBART","20 beets change goal to Lobart");
	Check(std::string(Resolve(DeliverBeets,objective).code)=="BAU_951_HILDA","delivery points to Hilda");
	Check(std::string(Resolve(FryingPan,objective).code)=="VLK_468_CANTHAR","pan quest points to merchant before purchase");
	objective.pan=1;
	Check(std::string(Resolve(FryingPan,objective).code)=="BAU_951_HILDA","pan purchase changes goal to Hilda");
	Check(std::string(Resolve(HealHilda,objective).code)=="VLK_439_VATRAS","medicine quest initially points to Vatras");
	objective.potion=1;
	Check(std::string(Resolve(HealHilda,objective).code)=="BAU_951_HILDA","medicine changes goal to Hilda");
	Check(std::string(Resolve(Matteo,objective).code)=="VLK_418_GRITTA","debt collection initially points to Gritta");
	objective.goldGiven=1;objective.gold=100;
	Check(std::string(Resolve(Matteo,objective).code)=="VLK_416_MATTEO","collected debt changes goal to Matteo");
	objective.gold=0;
	Check(!Resolve(Matteo,objective).code,"missing payment does not claim ready hand-in");
	objective.mission=2;
	Check(!Resolve(FryingPan,objective).code,"completed mission has no stale destination");
	objective.mission=-1;
	Check(!Resolve(Harvest,objective).code,"missing script state cannot create a guessed destination");
	Check(Resolve(WalkingStick,objective).item,"missing stick points to actual world item");
	objective.stick=1;
	Check(std::string(Resolve(WalkingStick,objective).code)=="BAU_954_MALETH","found stick changes goal to Maleth");
	objective.finished=1;
	Check(!Resolve(WalkingStick,objective).code,"finished stick quest has no destination");
	Check(Overlay::IsVisible() && Overlay::CapturesInput(), "open menu captures input");
	Overlay::SetVisible(false);
	Check(!Overlay::IsVisible() && Overlay::CapturesInput(), "closing menu guards held keys");
	Overlay::Fault("regression test");
	Check(!Overlay::CapturesInput() && Overlay::TakeJustReleased() && !Overlay::TakeJustReleased(), "overlay failure releases game input exactly once");
	std::printf("All %d regression checks passed.\n", checks);
	return 0;
}
