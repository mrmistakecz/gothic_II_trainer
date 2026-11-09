// Tab "Úkoly" – every quest of the game: what to do next and where, who gives
// and who completes it, what it needs and pays, the diary as a timeline, and
// one click to complete it in the dialog as if the player played it.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	enum : int { kShowRunning = 0, kShowDone, kShowFailed, kShowNotStarted, kShowAll, kShowCount };
	enum : int { kSortProgress = 0, kSortArea, kSortName, kSortCount };

	struct QuestRow {
		int quest = -1;     // index into g_questDb.model.quests, -1 for topics only the diary knows
		int diary = -1;     // index into g_diary.topics, -1 when not in the diary
		int status = 0;     // 0 not started, 1 running, 2 done, 3 failed, 4 obsolete
		int section = 0;    // 0 mission, 1 note
		const std::string* title = nullptr;
		std::string folded;
	};

	struct QuestsUiState {
		char search[96] = {};
		int filter = kShowRunning;
		int sort = kSortProgress;
		bool notes = false;
		std::string selectedRaw;      // selection survives refreshes and world changes; empty: overview
		bool selectedIsDiaryOnly = false;
		int confirmForce = -1;
		double confirmUntil = 0.0;
	};

	static QuestsUiState g_questsUi;

	static const char* QuestStatusName(int status) {
		switch (status) {
			case 1: return u8"Probíhá";
			case 2: return u8"Splněno";
			case 3: return u8"Nesplněno";
			case 4: return u8"Zastaralé";
			default: return u8"Nezačato";
		}
	}

	static ImVec4 QuestStatusColor(int status) {
		switch (status) {
			case 1: return kGold;
			case 2: return kGood;
			case 3: return kWarn;
			default: return kMuted;
		}
	}

	static const char* QuestStatusGlyph(int status) {
		switch (status) {
			case 1: return u8"•";
			case 2: return u8"✓";
			case 3: return u8"✗";
			case 4: return u8"–";
			default: return u8"·";
		}
	}

	static bool AcceptQuestRow(const QuestRow& r, int filter, bool notes, const std::string& needle) {
		if (!notes && r.section == 1)
			return false;
		if (filter == kShowRunning && r.status != 1)
			return false;
		if (filter == kShowDone && r.status != 2)
			return false;
		if (filter == kShowFailed && r.status != 3 && r.status != 4)
			return false;
		if (filter == kShowNotStarted && r.status != 0)
			return false;
		return ::GT::Text::Contains(r.folded, needle);
	}

	// Every quest of the scripts plus topics only the diary knows.
	static std::vector<QuestRow> AllQuestRows() {
		std::vector<QuestRow> rows;
		std::vector<bool> used(g_diary.topics.size(), false);
		for (size_t i = 0; i < g_questDb.model.quests.size(); i++) {
			const QS::Quest& q = g_questDb.model.quests[i];
			if (q.raw.empty())
				continue;
			QuestRow r;
			r.quest = (int)i;
			r.section = q.section == g_questDb.model.logNote ? 1 : 0;
			r.title = &g_questDb.quests[i].title;
			r.folded = g_questDb.quests[i].folded;
			std::unordered_map<std::string, int>::const_iterator it = g_diary.byRaw.find(q.raw);
			if (it != g_diary.byRaw.end()) {
				r.diary = it->second;
				r.status = g_diary.topics[r.diary].status;
				r.section = g_diary.topics[r.diary].section == 1 ? 1 : 0;
				used[r.diary] = true;
			}
			rows.push_back(r);
		}
		for (size_t d = 0; d < g_diary.topics.size(); d++) {
			if (used[d])
				continue;
			QuestRow r;
			r.diary = (int)d;
			r.status = g_diary.topics[d].status;
			r.section = g_diary.topics[d].section == 1 ? 1 : 0;
			r.title = &g_diary.topics[d].title;
			r.folded = ::GT::Text::Fold(g_diary.topics[d].title);
			rows.push_back(r);
		}
		std::sort(rows.begin(), rows.end(), [](const QuestRow& a, const QuestRow& b) {
			auto rank = [](int status) { return status == 1 ? 0 : status == 0 ? 1 : status == 2 ? 2 : 3; };
			int ra = rank(a.status), rb = rank(b.status);
			return ra != rb ? ra < rb : a.folded < b.folded;
		});
		return rows;
	}

	static const std::string& RawOf(const QuestRow& r) {
		static const std::string empty;
		if (r.quest >= 0)
			return g_questDb.model.quests[r.quest].raw;
		if (r.diary >= 0)
			return g_diary.topics[r.diary].raw;
		return empty;
	}

	static bool SameQuestRow(const QuestRow& a, const QuestRow& b) {
		return (a.quest < 0) == (b.quest < 0) && RawOf(a) == RawOf(b);
	}

	static void SelectQuest(int questIndex, int filter) {
		g_questsUi.selectedRaw = g_questDb.model.quests[questIndex].raw;
		g_questsUi.selectedIsDiaryOnly = false;
		if (filter >= 0)
			g_questsUi.filter = filter;
	}

	// ------------------------------------------------------------------ text helpers

	static std::string Plural(int n, const char* one, const char* few, const char* many) {
		const char* word = n == 1 ? one : (n >= 2 && n <= 4) ? few : many;
		return ::GT::Text::Format("%d %s", n, word);
	}

	static std::string GoldText(int n) {
		return Plural(n, u8"zlatý", u8"zlaté", u8"zlatých");
	}

	static std::string XpText(int n) {
		return Plural(n, u8"zkušenost", u8"zkušenosti", u8"zkušeností");
	}

	static std::string MetresText(float metres) {
		if (metres < 0.0f)
			return std::string();
		if (metres >= 1000.0f) {
			std::string km = ::GT::Text::Format("%.1f km", metres / 1000.0f);
			for (char& c : km)
				if (c == '.')
					c = ',';
			return km;
		}
		return ::GT::Text::Format("%.0f m", metres);
	}

	static std::string JoinTexts(const std::vector<std::string>& parts, const char* sep, const char* last) {
		std::string out;
		for (size_t i = 0; i < parts.size(); i++) {
			if (i)
				out += (i + 1 == parts.size()) ? last : sep;
			out += parts[i];
		}
		return out;
	}

	static void AddUnique(std::vector<std::string>& list, const std::string& text) {
		if (!text.empty() && std::find(list.begin(), list.end(), text) == list.end())
			list.push_back(text);
	}

	// The text cut to the width with "…", never in the middle of a character.
	static std::string FitText(const std::string& text, float width) {
		if (width <= 0.0f)
			return std::string();
		if (ImGui::CalcTextSize(text.c_str()).x <= width)
			return text;
		const float dots = ImGui::CalcTextSize(u8"…").x;
		std::vector<size_t> cuts;   // where whole characters end
		for (size_t i = 1; i <= text.size(); i++)
			if (i == text.size() || ((unsigned char)text[i] & 0xC0) != 0x80)
				cuts.push_back(i);
		int lo = -1, hi = (int)cuts.size() - 1;   // the longest prefix that fits
		while (lo < hi) {
			const int mid = (lo + hi + 1) / 2;
			const std::string prefix = text.substr(0, cuts[mid]);
			if (ImGui::CalcTextSize(prefix.c_str()).x + dots <= width)
				lo = mid;
			else
				hi = mid - 1;
		}
		std::string out = lo >= 0 ? text.substr(0, cuts[lo]) : std::string();
		while (!out.empty() && out.back() == ' ')
			out.pop_back();
		return out + u8"…";
	}

	static ImVec4 TierColor(int tier) {
		switch (tier) {
			case 0: return kGood;
			case 1: return kGold;
			case 2: return ImGui::GetStyleColorVec4(ImGuiCol_Text);
			default: return kMuted;
		}
	}

	static std::string GoalWhere(const QuestGoal& g) {
		std::string where = MetresText(g.metres);
		if (!g.area.empty())
			where += (where.empty() ? "" : u8" · ") + g.area;
		if (g.place == kNpcAsleep)
			where += u8" · teď spí, hra ho probudí";
		return where;
	}

	static void WrappedBullet(const std::string& text, const ImVec4* color = nullptr) {
		ImGui::Bullet();
		ImGui::SameLine();
		ImGui::PushTextWrapPos(0.0f);
		if (color)
			ImGui::TextColored(*color, "%s", text.c_str());
		else
			ImGui::TextUnformatted(text.c_str());
		ImGui::PopTextWrapPos();
	}

	static void WrappedText(const std::string& text, const ImVec4& color) {
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextUnformatted(text.c_str());
		ImGui::PopTextWrapPos();
		ImGui::PopStyleColor();
	}

	// The key of the current world, read once per frame (the list asks for it per row).
	static const std::string& HereWorld() {
		static std::string key;
		static int frame = -1;
		if (frame != ImGui::GetFrameCount()) {
			frame = ImGui::GetFrameCount();
			key = WorldKeyFromPath(CurrentWorldKey());
		}
		return key;
	}

	static std::string WhereText(const DialogText& t, const QuestStepLive* live) {
		if (live && live->here) {
			if (live->dead)
				return u8"mrtvý";
			if (live->place == kNpcParked)
				return u8"mimo hru";
			if (live->distance < 0.0f)
				return u8"neznámo kde";
			return MetresText(live->distance / 100.0f) + (live->place == kNpcAsleep ? u8", spí" : "");
		}
		if (!t.world.empty() && t.world != HereWorld())
			return u8"asi " + WorldDisplayName(t.world);
		return u8"není tu";
	}

	static std::string LineText(const DialogText& t, const QS::DialogFacts& f) {
		if (!t.line.empty())
			return u8"„" + t.line + u8"“";
		return f.dialog.important ? std::string(u8"(promluví na hrdinu sám)") : std::string(u8"(bez věty)");
	}

	static std::string AmountText(const QS::Amount& a) {
		if (a.item == g_questDb.goldItem && a.item >= 0)
			return GoldText(a.amount);
		return QuestName(a.item) + (a.amount > 1 ? ::GT::Text::Format(" × %d", a.amount) : std::string());
	}

	static std::string RewardText(const QS::DialogFacts& f) {
		std::vector<std::string> parts;
		if (f.xp > 0)
			parts.push_back(XpText(f.xp));
		for (const QS::Amount& a : f.rewards)
			parts.push_back(AmountText(a));
		std::string text = JoinTexts(parts, ", ", ", ");
		std::vector<std::string> choice;
		if (f.xp <= 0 && f.choiceXp > 0)
			choice.push_back(XpText(f.choiceXp));
		for (const QS::Amount& a : f.choiceRewards)
			choice.push_back(AmountText(a));
		if (!choice.empty())
			text += (text.empty() ? "" : u8"; ") + std::string(u8"podle volby v rozhovoru: ") + JoinTexts(choice, ", ", u8" nebo ");
		return text;
	}

	// Who gives / closes the quest according to the scripts, with the world when it is another one.
	static std::string NpcsWithRole(const QS::Quest& q, unsigned role) {
		std::vector<std::string> names;
		const std::string& here = HereWorld();
		for (const QS::Step& st : q.steps) {
			if (!(st.roles & role) || (role == QS::kRoleSuccess && (st.roles & QS::kRoleFail) && !(st.roles & QS::kRoleSuccess)))
				continue;
			const DialogText& t = g_questDb.dialogs[st.dialog];
			AddUnique(names, t.world.empty() || t.world == here ? t.npcName : t.npcName + " (" + WorldDisplayName(t.world) + ")");
		}
		return JoinTexts(names, ", ", u8" nebo ");
	}

	// The completing (or giving) step the hero can reach best, from the live
	// state: its character here and alive, the dialog not played yet, a plain
	// success before one that can also fail.
	static int BestStepUi(const QS::Quest& q, const QuestLiveState* live, unsigned role) {
		int best = -1, bestScore = -1;
		for (size_t i = 0; i < q.steps.size(); i++) {
			if (!(q.steps[i].roles & role))
				continue;
			const QS::DialogFacts& f = g_questDb.model.dialogs[q.steps[i].dialog];
			const QuestStepLive* sl = live ? &live->steps[i] : nullptr;
			const bool here = sl && sl->here && !sl->dead;
			const bool open = !sl || !sl->told || f.dialog.permanent;
			const bool clean = !(q.steps[i].roles & QS::kRoleFail);
			const int score = (here ? 4 : 0) + (open ? 2 : 0) + (clean ? 1 : 0);
			if (score > bestScore) {
				bestScore = score;
				best = (int)i;
			}
		}
		return best;
	}

	static bool HasRole(const QS::Quest& q, unsigned role) {
		for (const QS::Step& st : q.steps)
			if (st.roles & role)
				return true;
		return false;
	}

	// ------------------------------------------------------------------ buttons

	// Same line for the next widget only when it still fits into the window.
	static void SameLineIfFits(float width) {
		const float right = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width;
		if (right <= ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x)
			ImGui::SameLine();
	}

	static float ButtonWidth(const char* label) {
		return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	}

	static float SmallButtonWidth(const char* label) {
		return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	}

	// The minimap plugin (GothicQuestNav.dll), when it is installed.
	typedef int(__cdecl* QuestNavTrackFn)(const char*);
	static QuestNavTrackFn QuestNavTrack() {
		static QuestNavTrackFn fn = nullptr;
		static DWORD checked = 0;
		if (!fn && (checked == 0 || GetTickCount() - checked > 3000)) {
			checked = GetTickCount();
			if (HMODULE module = GetModuleHandleA("GothicQuestNav.dll"))
				fn = (QuestNavTrackFn)GetProcAddress(module, "GothicQuestNav_Track");
		}
		return fn;
	}

	// Buttons go on one line while they fit.
	struct ButtonRow {
		bool first = true;
		void Next(float width) {
			if (!first)
				SameLineIfFits(width);
			first = false;
		}
		bool Button(const char* label) {
			Next(ButtonWidth(label));
			return ImGui::Button(label);
		}
		bool Colored(const char* label, const ImVec4& color) {
			Next(ButtonWidth(label));
			return SmallButtonColored(label, color);
		}
	};

	static const char* kTipComplete = u8"Doplní předměty, které závěrečný rozhovor chce, přenese tě k postavě, začne rozhovor a sám v něm vybere závěrečnou větu. Hra pak úkol dokončí sama: postava si vezme, co chtěla, dá odměnu a svět se změní, jako bys to odehrál. Když se v rozhovoru ještě vybírá z možností, vybereš si sám.";
	static const char* kTipTalk = u8"Přenese tě k postavě a začne rozhovor, jako když ji oslovíš sám. Větu vybíráš ty. Menu traineru se zavře.";
	static const char* kTipTalkPick = u8"Přenese tě k postavě, začne rozhovor a vybere v něm tuhle větu – jako bys ji vybral sám. Co dál, rozhoduješ ty.";

	static void NavigateButton(ButtonRow& b, int questIndex, const QuestRow& row) {
		QuestNavTrackFn track = QuestNavTrack();
		if (!track || row.status != 1)
			return;
		if (b.Button(u8"Navigovat")) {
			if (track(g_questDb.model.quests[questIndex].raw.c_str()))
				Toast(u8"Minimapa ukazuje cestu: " + *row.title);
			else
				Toast(u8"Minimapa teď tento úkol vést nemůže");
		}
		Tooltip(u8"Minimapa (M a F6) začne ukazovat cestu k dalšímu kroku tohoto úkolu.");
	}

	static void TeleportToGoal(const QuestGoal& g) {
		if (g.npc >= 0)
			QuestTeleportToNpc(g.npc, g.who);
		else
			QuestTeleportToSpot(g.dest.n[0], g.dest.n[1], g.dest.n[2], !g.who.empty() ? g.who : g.item >= 0 ? QuestName(g.item) : g.text);
	}

	// The buttons of the main next step: one main action first.
	static void GoalButtons(int questIndex, const QuestRow& row, const QuestGoal& g) {
		ButtonRow b;
		std::vector<QS::Amount> wants;
		if (g.item >= 0 && g.amount > 0) {
			QS::Amount a;
			a.item = g.item;
			a.amount = g.amount;
			wants.push_back(a);
		}
		const bool completes = g.kind == kGoalFinish || g.kind == kGoalFinisher;
		switch (g.kind) {
			case kGoalFinish:
			case kGoalFinisher:
				if (b.Colored(u8"Dokončit úkol", kGood))
					QuestCompleteByDialog(questIndex, g.step);
				Tooltip(kTipComplete);
				break;
			case kGoalTalk:
				if (b.Colored(u8"Promluvit a vybrat", kGold))
					QuestTalkStep(questIndex, g.step);
				Tooltip(kTipTalkPick);
				break;
			case kGoalTake:
				if (b.Colored(u8"Vzít si úkol", kGold))
					QuestTalkStep(questIndex, g.step);
				Tooltip(u8"Přenese tě k postavě, začne rozhovor a vybere větu, kterou úkol dostaneš.");
				break;
			case kGoalFind:
			case kGoalChest:
				if (b.Colored(g.kind == kGoalFind ? u8"Teleport k předmětu" : u8"Teleport k truhle", kGold))
					TeleportToGoal(g);
				break;
			case kGoalCarrier:
				if (b.Colored(u8"Teleport k postavě", kGold))
					TeleportToGoal(g);
				break;
			case kGoalKill:
				if (b.Colored(u8"Teleport k cíli", kGold))
					TeleportToGoal(g);
				Tooltip(u8"Pozor, cíl může zaútočit.");
				break;
			case kGoalSource:
			case kGoalFirst:
				if (b.Colored(u8"Promluvit", kGold))
					QuestTalkTo(g.npc, g.who);
				Tooltip(kTipTalk);
				break;
			default:
				if (b.Colored(u8"Teleport", kGold))
					TeleportToGoal(g);
				break;
		}
		if (g.npc >= 0 && (completes || g.kind == kGoalTalk || g.kind == kGoalTake || g.kind == kGoalGiver)) {
			if (b.Button(u8"Promluvit"))
				QuestTalkTo(g.npc, g.who);
			Tooltip(kTipTalk);
		}
		if (g.npc >= 0 && (completes || g.kind == kGoalTalk || g.kind == kGoalTake || g.kind == kGoalSource || g.kind == kGoalFirst)) {
			if (b.Button(u8"Teleport"))
				TeleportToGoal(g);
		}
		if (g.kind == kGoalKill) {
			if (b.Button(u8"Zabít"))
				QuestKillNpc(g.npc, g.who);
			Tooltip(u8"Zabije cíl na dálku, jako by padl tvou rukou. Jde to, když je postava blízko – jinak se k ní nejdřív teleportuj.");
		}
		if (!wants.empty()) {
			if (b.Button(u8"Dát předmět"))
				QuestGiveItems(wants);
			Tooltip(u8"Doplní ti do inventáře, kolik ho úkol chce.");
		}
		if (!completes && row.status <= 1 && HasRole(g_questDb.model.quests[questIndex], QS::kRoleSuccess)) {
			if (b.Button(u8"Dokončit úkol"))
				QuestCompleteByDialog(questIndex);
			Tooltip(kTipComplete);
		}
		NavigateButton(b, questIndex, row);
	}

	// One of the other possible steps: short, with a teleport.
	static void GoalLine(int questIndex, const QuestGoal& g, int id) {
		ImGui::PushID(id);
		ImGui::TextColored(TierColor(g.tier), u8"•");
		ImGui::SameLine();
		ImGui::TextUnformatted(g.text.c_str());
		const std::string where = GoalWhere(g);
		if (!where.empty()) {
			SameLineIfFits(ImGui::CalcTextSize(where.c_str()).x);
			ImGui::TextDisabled("%s", where.c_str());
		}
		SameLineIfFits(SmallButtonWidth(u8"Teleport"));
		if (ImGui::SmallButton(u8"Teleport"))
			TeleportToGoal(g);
		if (g.kind == kGoalFinish || g.kind == kGoalFinisher) {
			SameLineIfFits(SmallButtonWidth(u8"Dokončit"));
			if (ImGui::SmallButton(u8"Dokončit"))
				QuestCompleteByDialog(questIndex, g.step);
			Tooltip(kTipComplete);
		}
		else if (g.kind == kGoalTalk || g.kind == kGoalTake) {
			SameLineIfFits(SmallButtonWidth(u8"Promluvit a vybrat"));
			if (ImGui::SmallButton(u8"Promluvit a vybrat"))
				QuestTalkStep(questIndex, g.step);
			Tooltip(kTipTalkPick);
		}
		else if (g.kind == kGoalSource || g.kind == kGoalFirst) {
			SameLineIfFits(SmallButtonWidth(u8"Promluvit"));
			if (ImGui::SmallButton(u8"Promluvit"))
				QuestTalkTo(g.npc, g.who);
			Tooltip(kTipTalk);
		}
		ImGui::PopID();
	}

	// ------------------------------------------------------------------ what the dialog needs

	// state: 1 fulfilled, 0 missing, -1 not known
	static void CheckLine(int state, const std::string& text) {
		const char* glyph = state > 0 ? u8"✓" : state == 0 ? u8"✗" : u8"·";
		const ImVec4 glyphColor = state > 0 ? kGood : state == 0 ? kWarn : kMuted;
		ImGui::TextColored(glyphColor, "%s", glyph);
		ImGui::SameLine();
		WrappedText(text, state > 0 ? kGood : state == 0 ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : kMuted);
	}

	static int CarriedOf(const QuestLiveState* live, int item) {
		if (!live)
			return -1;
		std::unordered_map<int, int>::const_iterator it = live->carried.find(item);
		return it != live->carried.end() ? it->second : -1;
	}

	static int ToldOf(const QuestLiveState* live, int info) {
		if (!live)
			return -1;
		std::unordered_map<int, int>::const_iterator it = live->dialogTold.find(info);
		return it != live->dialogTold.end() ? it->second : -1;
	}

	static void ItemCheck(const QS::Amount& a, const QuestLiveState* live) {
		const int have = CarriedOf(live, a.item);
		std::string text = a.item == g_questDb.goldItem ? GoldText(a.amount) : QuestName(a.item) + (a.amount > 1 ? ::GT::Text::Format(" × %d", a.amount) : std::string());
		if (have >= 0)
			text += ::GT::Text::Format(u8"  (máš %d)", have);
		CheckLine(have < 0 ? -1 : (have >= a.amount ? 1 : 0), text);
	}

	static std::string DialogNeedText(int info) {
		std::unordered_map<int, int>::const_iterator it = g_questDb.dialogOfInfo.find(info);
		if (it == g_questDb.dialogOfInfo.end())
			return std::string();
		const DialogText& t = g_questDb.dialogs[it->second];
		return t.npcName + u8" – " + LineText(t, g_questDb.model.dialogs[it->second]);
	}

	// Everything the dialog checks before it is offered, ticked off with the live state.
	static void DialogChecklist(int dialog, const QuestLiveState* live) {
		const QS::DialogFacts& f = g_questDb.model.dialogs[dialog];
		int lines = 0;
		for (const QS::Amount& a : f.needs) {
			if (a.group > 0)
				continue;
			ItemCheck(a, live);
			lines++;
		}
		std::vector<int> groups;
		for (const QS::Amount& a : f.needs) {
			if (a.group <= 0 || std::find(groups.begin(), groups.end(), a.group) != groups.end())
				continue;
			groups.push_back(a.group);
			std::vector<std::string> names;
			int state = live ? 0 : -1;
			for (const QS::Amount& b : f.needs)
				if (b.group == a.group) {
					names.push_back(AmountText(b));
					if (CarriedOf(live, b.item) >= b.amount)
						state = 1;
				}
			CheckLine(state, u8"jedno z: " + JoinTexts(names, ", ", u8" nebo "));
			lines++;
		}
		for (const QS::Amount& a : f.handIn) {
			bool listed = false;
			for (const QS::Amount& n : f.needs)
				if (n.item == a.item)
					listed = true;
			if (!listed) {
				ItemCheck(a, live);
				lines++;
			}
		}
		for (int target : f.mustBeDead) {
			int state = -1;
			if (live) {
				std::unordered_map<int, int>::const_iterator it = live->npcState.find(target);
				if (it != live->npcState.end())
					state = it->second;
			}
			const std::string name = QuestName(target);
			if (state == 1)
				CheckLine(1, name + u8" je mrtvý");
			else if (state == 0)
				CheckLine(0, u8"zabít: " + name);
			else
				CheckLine(-1, u8"zabít: " + name + u8" (není v tomto světě)");
			if (state == 0) {
				ImGui::PushID(target);
				SameLineIfFits(SmallButtonWidth(u8"K cíli"));
				if (ImGui::SmallButton(u8"K cíli"))
					QuestTeleportToNpc(target, name);
				SameLineIfFits(SmallButtonWidth(u8"Zabít"));
				if (ImGui::SmallButton(u8"Zabít"))
					QuestKillNpc(target, name);
				Tooltip(u8"Zabije cíl na dálku, jako by padl tvou rukou. Jde to, když je postava blízko – jinak se k ní nejdřív teleportuj.");
				ImGui::PopID();
			}
			lines++;
		}
		for (int info : f.needsDialogs) {
			const std::string text = DialogNeedText(info);
			if (text.empty())
				continue;
			const int told = ToldOf(live, info);
			CheckLine(told, u8"rozhovor: " + text);
			lines++;
		}
		if (!f.anyDialogs.empty()) {
			std::vector<std::string> texts;
			int state = live ? 0 : -1;
			for (int info : f.anyDialogs) {
				AddUnique(texts, DialogNeedText(info));
				if (ToldOf(live, info) == 1)
					state = 1;
			}
			if (!texts.empty()) {
				CheckLine(state, u8"jeden z rozhovorů: " + JoinTexts(texts, "; ", u8"; nebo "));
				lines++;
			}
		}
		if (f.minChapter > 1) {
			const int state = live ? (live->chapter >= f.minChapter ? 1 : 0) : -1;
			CheckLine(state, live ? ::GT::Text::Format(u8"%d. kapitola (teď je %d.)", f.minChapter, live->chapter) : ::GT::Text::Format(u8"%d. kapitola", f.minChapter));
			lines++;
		}
		if (lines == 0)
			ImGui::TextDisabled(u8"Žádné podmínky, které by šly ze skriptu poznat – stačí promluvit.");
		if (f.choices)
			ImGui::TextDisabled(u8"V rozhovoru se ještě vybírá z možností.");
	}

	// ------------------------------------------------------------------ parts of the detail

	static void QuestAutoBanner() {
		if (!g_questAuto.active)
			return;
		ImGui::TextColored(kGold, g_questAuto.finishing ? u8"Dokončuji úkol v rozhovoru: %s" : u8"Mluvím s: %s", g_questAuto.npcName.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton(u8"Zrušit"))
			Enqueue([]() { StopQuestAuto(u8"Rozhovor traineru zrušen"); });
	}

	static void QuestWarnings(int questIndex, const QuestRow& row, const QuestLiveState* live, const QuestNext* next) {
		const QS::Quest& q = g_questDb.model.quests[questIndex];
		bool failShown = false;
		if (row.status == 1 && next) {
			if (next->failNow >= 0 && next->failNow < (int)q.steps.size()) {
				const int d = q.steps[next->failNow].dialog;
				WrappedText(u8"Pozor: " + g_questDb.dialogs[d].npcName + u8" teď nabízí rozhovor, který úkol pokazí: "
					+ LineText(g_questDb.dialogs[d], g_questDb.model.dialogs[d]), kWarn);
				failShown = true;
			}
			if (next->finisherDead)
				WrappedText(u8"Kdo úkol uzavírá, je mrtvý (" + next->deadName + u8"). Rozhovorem ho už nedokončíš – v Podrobnostech ho můžeš Jen označit jako splněný.", kWarn);
		}
		if (row.status == 1 && !failShown) {
			std::vector<std::string> who;
			for (const QS::Step& st : q.steps)
				if ((st.roles & QS::kRoleFail) && !(st.roles & QS::kRoleSuccess))
					AddUnique(who, g_questDb.dialogs[st.dialog].npcName);
			if (!who.empty())
				WrappedText(u8"Úkol se dá i pokazit – pozor na rozhovor s: " + JoinTexts(who, ", ", u8" a "), kMuted);
		}
		if (row.status == 0 && live) {
			int givers = 0, dead = 0;
			for (size_t i = 0; i < q.steps.size(); i++)
				if (q.steps[i].roles & QS::kRoleStart) {
					givers++;
					if (live->steps[i].here && live->steps[i].dead)
						dead++;
				}
			if (givers > 0 && dead == givers)
				WrappedText(u8"Kdo úkol zadává, je mrtvý – tenhle úkol už nedostaneš.", kWarn);
		}
		if (row.status == 1 && q.endChapter > 1 && live && live->chapter < q.endChapter)
			WrappedText(::GT::Text::Format(u8"S příchodem %d. kapitoly úkol zastará – dokonči ho dřív.", q.endChapter),
				live->chapter + 1 >= q.endChapter ? kWarn : kMuted);
	}

	static void GoalExplanation(const QuestGoal& g, const QuestLiveState* live, const QS::Quest& q) {
		const bool important = g.dialog >= 0 && g_questDb.model.dialogs[g.dialog].dialog.important;
		const std::string line = g.line.empty() ? std::string() : u8"„" + g.line + u8"“";
		switch (g.kind) {
			case kGoalFinish:
			case kGoalTalk:
			case kGoalTake:
				if (important)
					WrappedText(u8"Promluví na tebe sám, jakmile k němu přijdeš.", kMuted);
				else if (!line.empty())
					WrappedText(u8"V rozhovoru vyber: " + line, ImGui::GetStyleColorVec4(ImGuiCol_Text));
				break;
			case kGoalFinisher:
				WrappedText(u8"Až budeš mít všechno, co úkol chce (níže), odevzdej ho tady.", kMuted);
				break;
			case kGoalFind:
				WrappedText(u8"Leží někde na zemi – nejbližší kus.", kMuted);
				break;
			case kGoalChest:
				WrappedText(u8"Leží v truhle – nejbližší, ve které je.", kMuted);
				break;
			case kGoalCarrier:
				WrappedText(u8"Nosí ho u sebe – kup, vyměň, nebo ho z něj dostaň.", kMuted);
				break;
			case kGoalSource:
				WrappedText(line.empty() ? std::string(u8"Dá ti ho v rozhovoru.") : u8"Dá ti ho v rozhovoru: " + line, kMuted);
				break;
			case kGoalKill:
				WrappedText(u8"Úkol chce, aby byl mrtvý.", kMuted);
				break;
			case kGoalFirst:
				WrappedText(line.empty() ? std::string(u8"Tenhle rozhovor musí být dřív.") : u8"Tenhle rozhovor musí být dřív: " + line, kMuted);
				break;
			case kGoalGiver:
				WrappedText(u8"Úkol teď ještě nenabízí. Musí platit tohle:", kMuted);
				if (g.step >= 0 && g.step < (int)q.steps.size()) {
					ImGui::Indent();
					DialogChecklist(q.steps[g.step].dialog, live);
					ImGui::Unindent();
				}
				break;
		}
	}

	static void QuestNextCard(int questIndex, const QuestRow& row, const QuestLiveState* live, const QuestNext* next) {
		const QS::Quest& q = g_questDb.model.quests[questIndex];
		const bool running = row.status == 1;
		if (!running && row.status != 0) {
			ImGui::Spacing();
			WrappedText(row.status == 2 ? u8"✓ Úkol je splněný." : row.status == 3 ? u8"Úkol skončil neúspěchem." : u8"Úkol zastaral – už ho nejde splnit.",
				QuestStatusColor(row.status));
			return;
		}
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.22f, 0.16f, 0.08f, 0.75f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(kGold.x * 0.75f, kGold.y * 0.75f, kGold.z * 0.75f, 0.85f));
		if (ImGui::BeginChild("##nextstep", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY)) {
			ImGui::TextColored(kMuted, "%s", running ? u8"DALŠÍ KROK" : u8"JAK ÚKOL ZÍSKAT");
			if (!next) {
				ImGui::TextDisabled(u8"Zjišťuji…");
			}
			else if (!next->goals.empty()) {
				const QuestGoal& g = next->goals[0];
				ImGui::PushFont(::GT::Overlay::TitleFont());
				WrappedText(g.text, g.tier == 2 ? kGold : TierColor(g.tier));
				ImGui::PopFont();
				const std::string where = GoalWhere(g);
				if (!where.empty())
					ImGui::TextDisabled("%s", where.c_str());
				GoalExplanation(g, live, q);
				if (!next->note.empty())
					WrappedText(next->note, kWarn);
				ImGui::Spacing();
				GoalButtons(questIndex, row, g);
				if (next->goals.size() > 1) {
					ImGui::Spacing();
					ImGui::TextDisabled("%s", running ? u8"Další možnosti:" : u8"Nabízí ho i:");
					const size_t shown = next->goals.size() < 8 ? next->goals.size() : 8;
					for (size_t i = 1; i < shown; i++)
						GoalLine(questIndex, next->goals[i], (int)i);
					if (next->goals.size() > shown)
						ImGui::TextDisabled(u8"…a další %d", (int)(next->goals.size() - shown));
				}
			}
			else if (running) {
				if (!next->note.empty())
					WrappedText(next->note, kGold);
				WrappedText(u8"Další krok se teď nepodařilo určit – postava může být jinde, nebo úkol pokračuje skriptem. Podívej se do deníku a na podmínky níže.", kMuted);
				ButtonRow b;
				if (HasRole(q, QS::kRoleSuccess)) {
					if (b.Colored(u8"Dokončit úkol", kGood))
						QuestCompleteByDialog(questIndex);
					Tooltip(kTipComplete);
				}
				NavigateButton(b, questIndex, row);
			}
			else {
				if (!next->note.empty())
					WrappedText(next->note, kGold);
				else if (HasRole(q, QS::kRoleStart))
					WrappedText(u8"Kdo úkol zadává, teď v tomto světě není (nebo je mimo hru).", kMuted);
				else
					WrappedText(u8"Úkol nezačíná rozhovorem – spouští ho jiný úkol nebo skript.", kMuted);
				if (HasRole(q, QS::kRoleSuccess)) {
					ButtonRow b;
					if (b.Button(u8"Dokončit úkol"))
						QuestCompleteByDialog(questIndex);
					Tooltip(kTipComplete);
				}
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor(2);
	}

	static void InfoRow(const char* label, const std::string& value, const ImVec4* color = nullptr) {
		ImGui::TextColored(kMuted, "%s", label);
		ImGui::SameLine(Em(5.5f));
		ImGui::PushTextWrapPos(0.0f);
		if (color)
			ImGui::TextColored(*color, "%s", value.c_str());
		else
			ImGui::TextUnformatted(value.c_str());
		ImGui::PopTextWrapPos();
	}

	static void QuestOverview(int questIndex, const QuestRow& row, const QuestLiveState* live) {
		const QS::Quest& q = g_questDb.model.quests[questIndex];
		ImGui::SeparatorText(u8"Přehled");
		const std::string givers = NpcsWithRole(q, QS::kRoleStart);
		const std::string closers = NpcsWithRole(q, QS::kRoleSuccess);
		InfoRow(u8"Zadává", givers.empty() ? std::string(u8"nikdo v rozhovoru (spouští ho skript nebo jiný úkol)") : givers);
		InfoRow(u8"Uzavírá", closers.empty() ? std::string(u8"nikdo v rozhovoru") : closers);
		const int best = BestStepUi(q, live, QS::kRoleSuccess);
		if (best >= 0) {
			const std::string reward = RewardText(g_questDb.model.dialogs[q.steps[best].dialog]);
			if (reward.empty())
				InfoRow(u8"Odměna", u8"ve skriptu žádná (nebo ji neumím poznat)", &kMuted);
			else
				InfoRow(u8"Odměna", reward, &kGood);
		}
		(void)row;
	}

	static void QuestRequirements(int questIndex, const QuestRow& row, const QuestLiveState* live) {
		const QS::Quest& q = g_questDb.model.quests[questIndex];
		if (row.status >= 2)
			return;   // done, failed, obsolete
		ImGui::SeparatorText(row.status == 1 ? u8"Co úkol chce" : u8"Co bude úkol chtít");
		const int best = BestStepUi(q, live, QS::kRoleSuccess);
		if (best < 0) {
			WrappedText(u8"Tenhle úkol se nedokončuje rozhovorem (hlavní příběh nebo jiný skript). V Podrobnostech ho můžeš Jen označit jako splněný.", kMuted);
			return;
		}
		auto stepHeader = [&](int stepIndex, const char* label) {
			const int d = q.steps[stepIndex].dialog;
			const DialogText& t = g_questDb.dialogs[d];
			const QuestStepLive* sl = live ? &live->steps[stepIndex] : nullptr;
			ImGui::TextDisabled("%s", label);
			ImGui::SameLine();
			ImGui::TextColored(sl && sl->told ? kGood : kGold, "%s%s", sl && sl->told ? u8"✓ " : "", t.npcName.c_str());
			const std::string where = WhereText(t, sl);
			SameLineIfFits(ImGui::CalcTextSize(where.c_str()).x);
			ImGui::TextDisabled("%s", where.c_str());
			WrappedText(LineText(t, g_questDb.model.dialogs[d]), ImGui::GetStyleColorVec4(ImGuiCol_Text));
		};
		stepHeader(best, u8"Závěrečný rozhovor:");
		ImGui::Indent();
		DialogChecklist(q.steps[best].dialog, live);
		ImGui::Unindent();
		const std::vector<QS::Amount> items = ItemsFor(g_questDb.model.dialogs[q.steps[best].dialog]);
		if (!items.empty()) {
			if (ImGui::SmallButton(u8"Dát potřebné předměty"))
				QuestGiveItems(items);
			Tooltip(u8"Doplní ti do inventáře všechno, co závěrečný rozhovor chce. Do rozhovoru nejdeš.");
		}
		// other ways to complete it
		for (size_t i = 0; i < q.steps.size(); i++) {
			if ((int)i == best || !(q.steps[i].roles & QS::kRoleSuccess))
				continue;
			ImGui::PushID(1000 + (int)i);
			ImGui::Spacing();
			stepHeader((int)i, u8"Nebo:");
			const QuestStepLive* sl = live ? &live->steps[i] : nullptr;
			const bool reachable = sl && sl->here && !sl->dead && sl->place != kNpcParked;
			if (!reachable)
				ImGui::BeginDisabled();
			if (ImGui::SmallButton(u8"Dokončit tímto rozhovorem"))
				QuestCompleteByDialog(questIndex, (int)i);
			if (!reachable)
				ImGui::EndDisabled();
			const std::vector<QS::Amount> other = ItemsFor(g_questDb.model.dialogs[q.steps[i].dialog]);
			if (!other.empty()) {
				ImGui::SameLine();
				if (ImGui::SmallButton(u8"Dát předměty"))
					QuestGiveItems(other);
			}
			if (ImGui::TreeNode(u8"Co chce tenhle rozhovor")) {
				DialogChecklist(q.steps[i].dialog, live);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	// The diary entries as a timeline, the newest at the bottom.
	static void DiaryTimeline(const std::vector<std::string>& entries) {
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float indent = Em(1.3f);
		const float x = ImGui::GetCursorScreenPos().x + Em(0.45f);
		const float radius = Em(0.2f);
		std::vector<float> dots;
		for (size_t i = 0; i < entries.size(); i++) {
			const bool last = i + 1 == entries.size();
			dots.push_back(ImGui::GetCursorScreenPos().y + ImGui::GetTextLineHeight() * 0.5f);
			ImGui::Indent(indent);
			WrappedText(entries[i], last ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : kMuted);
			ImGui::Unindent(indent);
			if (!last)
				ImGui::Dummy(ImVec2(0.0f, Em(0.15f)));
		}
		if (dots.size() > 1)
			dl->AddLine(ImVec2(x, dots.front()), ImVec2(x, dots.back()), ImGui::GetColorU32(ImVec4(kMuted.x, kMuted.y, kMuted.z, 0.45f)), Em(0.1f));
		for (size_t i = 0; i < dots.size(); i++) {
			const bool last = i + 1 == dots.size();
			dl->AddCircleFilled(ImVec2(x, dots[i]), last ? radius * 1.4f : radius, ImGui::GetColorU32(last ? kGold : kMuted));
		}
	}

	static void StepButtons(int dialog, const QuestStepLive* live) {
		const QS::DialogFacts& f = g_questDb.model.dialogs[dialog];
		const DialogText& t = g_questDb.dialogs[dialog];
		const bool reachable = live && live->here && !live->dead && live->place != kNpcParked && live->distance >= 0.0f;
		if (!reachable)
			ImGui::BeginDisabled();
		if (ImGui::SmallButton(u8"Teleport"))
			QuestTeleportToNpc(f.dialog.npc, t.npcName);
		ImGui::SameLine();
		if (ImGui::SmallButton(u8"Promluvit"))
			QuestTalkTo(f.dialog.npc, t.npcName);
		if (!reachable)
			ImGui::EndDisabled();
		if (reachable)
			Tooltip(kTipTalk);
	}

	static void DiaryStatusCombo(const std::string& raw, int status, bool inDiary) {
		if (!inDiary)
			ImGui::BeginDisabled();
		ImGui::SetNextItemWidth(Em(9.0f));
		if (ImGui::BeginCombo("##queststatus", u8"Stav v deníku")) {
			for (int st = 1; st <= 4; st++)
				if (ImGui::Selectable(QuestStatusName(st), status == st))
					QuestSetDiaryStatus(raw, st);
			ImGui::EndCombo();
		}
		if (!inDiary)
			ImGui::EndDisabled();
		Tooltip(inDiary ? u8"Změní jen zápis v deníku, ne průběh úkolu." : u8"Úkol v deníku ještě není.");
	}

	// Technical details and manual changes, folded away.
	static void QuestDetails(int questIndex, const QuestRow& row, const QuestLiveState* live) {
		QuestsUiState& s = g_questsUi;
		const QS::Quest& q = g_questDb.model.quests[questIndex];
		const QuestText& text = g_questDb.quests[questIndex];
		ImGui::Spacing();
		if (!ImGui::CollapsingHeader(u8"Podrobnosti a ruční úpravy###questmore"))
			return;
		ImGui::Indent(Em(0.5f));

		// --- manual actions
		ButtonRow b;
		const bool hasSuccess = HasRole(q, QS::kRoleSuccess);
		if (!hasSuccess)
			ImGui::BeginDisabled();
		if (b.Button(u8"Připravit"))
			QuestPrepareCompletion(questIndex);
		Tooltip(u8"Jako Dokončit úkol, jen závěrečnou větu v rozhovoru vybereš sám.");
		if (!hasSuccess)
			ImGui::EndDisabled();
		const double now = ImGui::GetTime();
		if (s.confirmForce == questIndex && now > s.confirmUntil)
			s.confirmForce = -1;
		if (s.confirmForce == questIndex) {
			b.Next(ButtonWidth(u8"Opravdu jen označit?"));
			if (SmallButtonColored(u8"Opravdu jen označit?", kWarn)) {
				s.confirmForce = -1;
				QuestForceComplete(questIndex);
			}
		}
		else if (b.Button(u8"Jen označit jako splněný")) {
			s.confirmForce = questIndex;
			s.confirmUntil = now + 5.0;
		}
		Tooltip(u8"Bez rozhovoru: nastaví stav úkolu, zapíše ho v deníku jako splněný a dá odměnu. Postavy ve světě o tom ale nevědí – kdo úkol uzavírá, může dál čekat na předmět. Hodí se jen pro úkoly, které rozhovorem dokončit nejdou (hlavní příběh, postava je mrtvá nebo v jiném světě).");
		b.Next(Em(9.0f));
		DiaryStatusCombo(q.raw, row.status, row.diary >= 0);

		// --- every dialog of the quest
		const std::string stepsTitle = ::GT::Text::Format(u8"Všechny rozhovory úkolu (%d)###queststeps", (int)q.steps.size());
		if (ImGui::TreeNodeEx(stepsTitle.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth)) {
			const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
			if (ImGui::BeginTable("##steps", 4, flags)) {
				ImGui::TableSetupColumn(u8"Postava", ImGuiTableColumnFlags_WidthStretch, 1.3f);
				ImGui::TableSetupColumn(u8"Věta", ImGuiTableColumnFlags_WidthStretch, 2.6f);
				ImGui::TableSetupColumn(u8"Kde", ImGuiTableColumnFlags_WidthFixed, Em(6.5f));
				ImGui::TableSetupColumn(u8"Akce", ImGuiTableColumnFlags_WidthFixed, Em(9.5f));
				ImGui::TableHeadersRow();
				for (size_t i = 0; i < q.steps.size(); i++) {
					const int d = q.steps[i].dialog;
					const DialogText& t = g_questDb.dialogs[d];
					const QuestStepLive* sl = live ? &live->steps[i] : nullptr;
					const unsigned roles = q.steps[i].roles;
					ImGui::PushID(2000 + (int)i);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					const char* glyph = sl && sl->told ? u8"✓" : (sl && sl->available ? u8"•" : u8"·");
					const ImVec4 color = sl && sl->told ? kGood : (sl && sl->available ? kGold : kMuted);
					ImGui::TextColored(color, "%s %s", glyph, t.npcName.c_str());
					const char* role = (roles & QS::kRoleSuccess) ? u8"splnění" : (roles & QS::kRoleFail) ? u8"neúspěch" : (roles & QS::kRoleStart) ? u8"zadání" : u8"postup";
					ImGui::TextDisabled("%s", role);
					ImGui::TableNextColumn();
					ImGui::PushTextWrapPos(0.0f);
					ImGui::TextUnformatted(LineText(t, g_questDb.model.dialogs[d]).c_str());
					ImGui::PopTextWrapPos();
					ImGui::TableNextColumn();
					ImGui::TextDisabled("%s", WhereText(t, sl).c_str());
					ImGui::TableNextColumn();
					StepButtons(d, sl);
					ImGui::PopID();
				}
				ImGui::EndTable();
			}
			ImGui::TextDisabled(u8"✓ = už proběhl · • = dá se vybrat teď · · = zatím ne nebo je postava jinde");
			ImGui::TreePop();
		}

		// --- spoilers: everything the quest can write to the diary
		if (!text.texts.empty()) {
			const std::string hintTitle = ::GT::Text::Format(u8"Nápověda – co všechno může úkol zapsat do deníku (%d)###questhints", (int)text.texts.size());
			if (ImGui::TreeNodeEx(hintTitle.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth)) {
				ImGui::TextColored(kWarn, u8"Pozor, prozrazuje další průběh úkolu.");
				for (const std::string& e : text.texts)
					WrappedBullet(e);
				ImGui::TreePop();
			}
		}

		// --- variables
		if (!q.missionVars.empty() && ImGui::TreeNodeEx(u8"Proměnné úkolu###questvars", ImGuiTreeNodeFlags_SpanAvailWidth)) {
			static const char* states[] = { u8"0 – nezadán", u8"1 – probíhá", u8"2 – splněn", u8"3 – nesplněn", u8"4 – zastaralý" };
			for (size_t i = 0; i < q.missionVars.size(); i++) {
				const int var = q.missionVars[i];
				int value = live && i < live->values.size() ? live->values[i] : -1;
				ImGui::PushID(3000 + (int)i);
				zCPar_Symbol* sym = parser ? parser->GetSymbol(var) : nullptr;
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(sym ? sym->name.ToChar() : "?");
				ImGui::SameLine(Em(17.0f));
				ImGui::SetNextItemWidth(Em(10.0f));
				const char* current = value >= 0 && value <= 4 ? states[value] : (value < 0 ? u8"…" : "?");
				std::string other;
				if (value > 4) {
					other = ::GT::Text::Format("%d", value);
					current = other.c_str();
				}
				if (ImGui::BeginCombo("##var", current)) {
					for (int v = 0; v <= 4; v++)
						if (ImGui::Selectable(states[v], value == v))
							QuestSetVariable(var, v);
					ImGui::EndCombo();
				}
				ImGui::PopID();
			}
			if (ImGui::SmallButton(u8"Srovnat deník podle proměnných"))
				QuestSyncDiary();
			Tooltip(u8"Spustí herní B_CheckLog: úkoly, které mají proměnnou splněnou nebo nesplněnou, se v deníku přesunou do správné části.");
			ImGui::TreePop();
		}
		ImGui::Unindent(Em(0.5f));
	}

	// ------------------------------------------------------------------ detail

	static void QuestDetail(const QuestRow& row) {
		ImGui::PushFont(::GT::Overlay::TitleFont());
		WrappedText(*row.title, kGold);
		ImGui::PopFont();
		ImGui::TextColored(QuestStatusColor(row.status), "%s %s", QuestStatusGlyph(row.status), QuestStatusName(row.status));
		if (row.section == 1) {
			ImGui::SameLine();
			ImGui::TextDisabled(u8"· poznámka");
		}
		const DiaryTopic* diary = row.diary >= 0 ? &g_diary.topics[row.diary] : nullptr;

		if (row.quest < 0) {
			// a topic the analyzer does not know (written by a script outside of dialogs)
			DiaryStatusCombo(diary->raw, row.status, true);
			ImGui::SeparatorText(u8"Deník");
			if (!diary->entries.empty())
				DiaryTimeline(diary->entries);
			ImGui::TextDisabled(u8"Tenhle zápis přidává skript mimo rozhovory, víc o něm nevím.");
			return;
		}

		const int questIndex = row.quest;
		const QS::Quest& q = g_questDb.model.quests[questIndex];

		// live state and the next steps: refreshed on the game thread every 1.5 s
		const double now = ImGui::GetTime();
		if (g_questLive.pending && now > g_questLive.nextRefresh + 5.0)
			g_questLive.pending = false;
		if (!g_questLive.pending && (g_questLive.quest != questIndex || now >= g_questLive.nextRefresh)) {
			g_questLive.pending = true;
			g_questLive.nextRefresh = now + 1.5;
			Enqueue([questIndex]() { RefreshQuestDetail(questIndex); });
		}
		const QuestLiveState* live = g_questLive.quest == questIndex && g_questLive.steps.size() == q.steps.size() ? &g_questLive : nullptr;
		const QuestNext* next = live && g_questNext.quest == questIndex ? &g_questNext.next : nullptr;

		QuestAutoBanner();
		QuestWarnings(questIndex, row, live, next);
		QuestNextCard(questIndex, row, live, next);
		QuestOverview(questIndex, row, live);
		QuestRequirements(questIndex, row, live);
		ImGui::SeparatorText(u8"Deník");
		if (diary && !diary->entries.empty())
			DiaryTimeline(diary->entries);
		else
			ImGui::TextDisabled("%s", diary ? u8"Úkol je v deníku zatím bez zápisu." : u8"Úkol v deníku zatím není.");
		QuestDetails(questIndex, row, live);
	}

	// ------------------------------------------------------------------ overview without a selection

	static void QuestDashboard(const std::vector<QuestRow>& all) {
		ImGui::PushFont(::GT::Overlay::TitleFont());
		ImGui::TextColored(kGold, "%s", u8"Přehled úkolů");
		ImGui::PopFont();
		QuestAutoBanner();
		struct Ready { int quest; const QuestBrief* brief; };
		std::vector<Ready> ready;
		int running = 0;
		for (const QuestRow& r : all) {
			if (r.status != 1 || r.section != 0)
				continue;
			running++;
			if (r.quest < 0)
				continue;
			std::unordered_map<int, QuestBrief>::const_iterator it = g_questBoard.brief.find(r.quest);
			if (it != g_questBoard.brief.end() && it->second.tier == 0)
				ready.push_back({ r.quest, &it->second });
		}
		std::stable_sort(ready.begin(), ready.end(), [](const Ready& a, const Ready& b) { return a.brief->metres < b.brief->metres; });
		ImGui::TextDisabled(u8"Probíhá: %d · hned k dokončení: %d · nové k převzetí: %d", running, (int)ready.size(), (int)g_questBoard.offers.size());
		if (!g_questBoard.ready) {
			ImGui::TextDisabled(u8"Zjišťuji, co se dá dělat…");
			return;
		}

		ImGui::SeparatorText(u8"Jde dokončit hned");
		if (ready.empty())
			ImGui::TextDisabled(u8"Teď žádný. Co dělat dál u ostatních, uvidíš vlevo.");
		for (size_t i = 0; i < ready.size(); i++) {
			const Ready& r = ready[i];
			ImGui::PushID(5000 + (int)i);
			ImGui::TextColored(kGood, u8"★");
			ImGui::SameLine();
			ImGui::TextUnformatted(g_questDb.quests[r.quest].title.c_str());
			std::string sub = r.brief->text;
			const std::string distance = MetresText(r.brief->metres);
			if (!distance.empty())
				sub += u8" · " + distance;
			if (!r.brief->area.empty())
				sub += u8" · " + r.brief->area;
			ImGui::Indent(Em(1.2f));
			ImGui::TextDisabled("%s", sub.c_str());
			ImGui::SameLine();
			if (SmallButtonColored(u8"Dokončit", kGood))
				QuestCompleteByDialog(r.quest, r.brief->step);
			Tooltip(kTipComplete);
			SameLineIfFits(ButtonWidth(u8"Otevřít"));
			if (ImGui::Button(u8"Otevřít"))
				SelectQuest(r.quest, kShowRunning);
			ImGui::Unindent(Em(1.2f));
			ImGui::PopID();
		}

		ImGui::SeparatorText(u8"Nové úkoly, které si můžeš vzít hned");
		if (g_questBoard.offers.empty())
			ImGui::TextDisabled(u8"V tomhle světě ti teď nikdo nový úkol nenabízí.");
		const size_t shown = g_questBoard.offers.size() < 15 ? g_questBoard.offers.size() : 15;
		for (size_t i = 0; i < shown; i++) {
			const QuestOffer& o = g_questBoard.offers[i];
			ImGui::PushID(6000 + (int)i);
			ImGui::TextColored(kGold, u8"☆");
			ImGui::SameLine();
			ImGui::TextUnformatted(g_questDb.quests[o.quest].title.c_str());
			std::string sub = o.npcName;
			const std::string distance = MetresText(o.metres);
			if (!distance.empty())
				sub += u8" · " + distance;
			if (!o.area.empty())
				sub += u8" · " + o.area;
			ImGui::Indent(Em(1.2f));
			ImGui::TextDisabled("%s", sub.c_str());
			ImGui::SameLine();
			if (SmallButtonColored(u8"Vzít si", kGold))
				QuestTalkStep(o.quest, o.step);
			Tooltip(u8"Přenese tě k postavě, začne rozhovor a vybere větu, kterou úkol dostaneš.");
			SameLineIfFits(ButtonWidth(u8"Teleport"));
			if (ImGui::Button(u8"Teleport"))
				QuestTeleportToNpc(o.npc, o.npcName);
			SameLineIfFits(ButtonWidth(u8"Otevřít"));
			if (ImGui::Button(u8"Otevřít"))
				SelectQuest(o.quest, kShowNotStarted);
			ImGui::Unindent(Em(1.2f));
			ImGui::PopID();
		}
		if (g_questBoard.offers.size() > shown)
			ImGui::TextDisabled(u8"…a další %d ve filtru Nezačaté", (int)(g_questBoard.offers.size() - shown));

		ImGui::Spacing();
		WrappedText(u8"Vyber úkol vlevo. Uvidíš, co dělat dál a kde, kdo ho zadává a kdo uzavírá, co chce a jakou dá odměnu – a jedním kliknutím ho dokončíš v rozhovoru, jako bys ho odehrál.", kMuted);
	}

	// ------------------------------------------------------------------ the list

	static const QuestBrief* BriefOf(const QuestRow& r) {
		if (r.quest < 0 || r.status != 1 || !g_questBoard.ready)
			return nullptr;
		std::unordered_map<int, QuestBrief>::const_iterator it = g_questBoard.brief.find(r.quest);
		return it != g_questBoard.brief.end() ? &it->second : nullptr;
	}

	static const QuestOffer* OfferOf(const QuestRow& r) {
		if (r.quest < 0 || r.status != 0 || r.diary >= 0 || !g_questBoard.ready)
			return nullptr;
		std::unordered_map<int, int>::const_iterator it = g_questBoard.offerOf.find(r.quest);
		return it != g_questBoard.offerOf.end() ? &g_questBoard.offers[it->second] : nullptr;
	}

	static float RowMetres(const QuestRow& r) {
		if (const QuestBrief* b = BriefOf(r))
			return b->tier < 9 ? b->metres : -1.0f;
		if (const QuestOffer* o = OfferOf(r))
			return o->metres;
		return -1.0f;
	}

	struct QuestSection {
		std::string title;
		ImVec4 color;
		int order = 0;
		bool byDistance = false;
		float nearest = 1.0e30f;
		std::vector<int> rows;
	};

	static std::vector<QuestSection> GroupQuestRows(const std::vector<QuestRow>& rows, int filter, int sort) {
		std::vector<QuestSection> out;
		auto into = [&](const std::string& title, const ImVec4& color, int order, bool byDistance, int row) {
			size_t k = 0;
			while (k < out.size() && out[k].title != title)
				k++;
			if (k == out.size()) {
				QuestSection s;
				s.title = title;
				s.color = color;
				s.order = order;
				s.byDistance = byDistance;
				out.push_back(s);
			}
			out[k].rows.push_back(row);
			const float m = RowMetres(rows[row]);
			if (m >= 0.0f && m < out[k].nearest)
				out[k].nearest = m;
		};
		for (int i = 0; i < (int)rows.size(); i++) {
			const QuestRow& r = rows[i];
			if (sort == kSortName) {
				into(std::string(), kMuted, 0, false, i);
				continue;
			}
			if (filter == kShowAll) {
				static const char* titles[] = { u8"Nezačaté", u8"Probíhající", u8"Splněné", u8"Nesplněné", u8"Zastaralé" };
				const int st = r.status >= 0 && r.status <= 4 ? r.status : 0;
				const int order = st == 1 ? 0 : st == 0 ? 1 : st;
				into(titles[st], QuestStatusColor(st), order, false, i);
				continue;
			}
			if (r.section == 1) {
				into(u8"Poznámky", kMuted, 8, false, i);
				continue;
			}
			if (r.status == 1) {
				const QuestBrief* b = BriefOf(r);
				if (!b)
					into(g_questBoard.ready ? std::string(u8"Další krok neznámý") : std::string(u8"Zjišťuji…"), kMuted, 7, false, i);
				else if (sort == kSortArea) {
					if (b->tier < 9)
						into(b->area.empty() ? std::string(u8"Neznámé místo") : b->area, kGold, 1, true, i);
					else if (!b->elsewhere.empty())
						into(u8"Jiný svět: " + WorldDisplayName(b->elsewhere), kMuted, 5, false, i);
					else
						into(u8"Bez místa", kMuted, 6, false, i);
				}
				else if (b->tier == 0)
					into(u8"Jde dokončit hned", kGood, 0, true, i);
				else if (b->tier < 9)
					into(u8"V tomto světě – podle vzdálenosti", kGold, 1, true, i);
				else if (!b->elsewhere.empty())
					into(u8"V jiném světě: " + WorldDisplayName(b->elsewhere), kMuted, 5, false, i);
				else
					into(u8"Další krok neznámý", kMuted, 6, false, i);
				continue;
			}
			if (r.status == 0) {
				if (const QuestOffer* o = OfferOf(r)) {
					if (sort == kSortArea)
						into(o->area.empty() ? std::string(u8"Neznámé místo") : o->area, kGold, 1, true, i);
					else
						into(u8"Můžeš si vzít hned", kGold, 0, true, i);
				}
				else
					into(u8"Ostatní", kMuted, 6, false, i);
				continue;
			}
			into(std::string(), kMuted, 0, false, i);
		}
		for (QuestSection& s : out)
			if (s.byDistance)
				std::stable_sort(s.rows.begin(), s.rows.end(), [&](int a, int b) { return RowMetres(rows[a]) < RowMetres(rows[b]); });
		std::stable_sort(out.begin(), out.end(), [](const QuestSection& a, const QuestSection& b) {
			if (a.order != b.order)
				return a.order < b.order;
			return a.nearest < b.nearest;
		});
		return out;
	}

	struct RowLook {
		const char* glyph = u8"·";
		ImVec4 glyphColor;
		std::string sub;
		ImVec4 subColor;
		std::string subMore;    // after sub, muted
		std::string right;      // distance
	};

	static RowLook LookOf(const QuestRow& r) {
		RowLook k;
		k.glyph = QuestStatusGlyph(r.status);
		k.glyphColor = QuestStatusColor(r.status);
		k.subColor = kMuted;
		if (r.section == 1 || r.quest < 0)
			return k;
		if (r.status == 1) {
			const QuestBrief* b = BriefOf(r);
			if (!b) {
				k.sub = g_questBoard.ready ? std::string() : std::string(u8"zjišťuji…");
				return k;
			}
			if (b->finisherDead) {
				k.sub = u8"Kdo ho uzavírá, je mrtvý: " + b->deadName;
				k.subColor = kWarn;
			}
			else if (b->tier < 9) {
				k.sub = b->text;
				k.subColor = TierColor(b->tier);
				if (!b->area.empty())
					k.subMore = u8" · " + b->area;
				k.right = MetresText(b->metres);
				if (b->tier == 0) {
					k.glyph = u8"★";
					k.glyphColor = kGood;
				}
			}
			else {
				k.sub = b->text.empty() ? std::string(u8"další krok neznámý") : b->text;
			}
			return k;
		}
		if (r.status == 0) {
			if (const QuestOffer* o = OfferOf(r)) {
				k.glyph = u8"☆";
				k.glyphColor = kGold;
				k.sub = u8"Nabízí: " + o->npcName;
				k.subColor = kGold;
				if (!o->area.empty())
					k.subMore = u8" · " + o->area;
				k.right = MetresText(o->metres);
			}
			else {
				const std::string givers = NpcsWithRole(g_questDb.model.quests[r.quest], QS::kRoleStart);
				if (!givers.empty())
					k.sub = u8"Zadává: " + givers;
			}
		}
		return k;
	}

	// One quest of the list: the title, the next step under it, the distance on the right.
	static bool DrawQuestRow(const QuestRow& r, bool selected) {
		const RowLook k = LookOf(r);
		const ImVec2 start = ImGui::GetCursorScreenPos();
		const float width = ImGui::GetContentRegionAvail().x;
		const float line = ImGui::GetTextLineHeight();
		const float gap = Em(0.1f);
		const bool two = !k.sub.empty();
		const bool clicked = ImGui::Selectable("##quest", selected, ImGuiSelectableFlags_None, ImVec2(0.0f, two ? line * 2.0f + gap : line));
		if (!ImGui::IsItemVisible())
			return clicked;   // scrolled away, nothing to draw
		const bool hovered = ImGui::IsItemHovered();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddText(ImVec2(start.x + Em(0.15f), start.y), ImGui::GetColorU32(k.glyphColor), k.glyph);
		const float textX = start.x + Em(1.2f);
		const float right = start.x + width - Em(0.25f);
		float rightWidth = 0.0f;
		if (!k.right.empty()) {
			rightWidth = ImGui::CalcTextSize(k.right.c_str()).x;
			dl->AddText(ImVec2(right - rightWidth, start.y), ImGui::GetColorU32(kMuted), k.right.c_str());
		}
		const std::string title = FitText(*r.title, right - textX - (rightWidth > 0.0f ? rightWidth + Em(0.6f) : 0.0f));
		dl->AddText(ImVec2(textX, start.y), ImGui::GetColorU32(ImGuiCol_Text), title.c_str());
		bool cut = title.size() != r.title->size();
		if (two) {
			const float y = start.y + line + gap;
			const std::string sub = FitText(k.sub, right - textX);
			dl->AddText(ImVec2(textX, y), ImGui::GetColorU32(k.subColor), sub.c_str());
			if (sub.size() != k.sub.size())
				cut = true;
			else if (!k.subMore.empty()) {
				const float used = ImGui::CalcTextSize(sub.c_str()).x;
				const std::string more = FitText(k.subMore, right - textX - used);
				dl->AddText(ImVec2(textX + used, y), ImGui::GetColorU32(kMuted), more.c_str());
				if (more.size() != k.subMore.size())
					cut = true;
			}
		}
		if (hovered && cut && ImGui::BeginItemTooltip()) {
			ImGui::PushTextWrapPos(Em(24.0f));
			ImGui::TextUnformatted(r.title->c_str());
			if (two)
				ImGui::TextColored(k.subColor, "%s", (k.sub + k.subMore).c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
		return clicked;
	}

	// ------------------------------------------------------------------ tab

	static void TabQuests() {
		if (!InGame()) {
			NotInGame();
			return;
		}
		QuestsUiState& s = g_questsUi;
		if (g_questDb.ready && (g_questDb.parser != parser || g_questDb.symbols != parser->symtab.table.GetNumInList()))
			g_questDb.ready = false;
		if (!g_questDb.ready) {
			if (!g_questDb.building) {
				g_questDb.building = true;
				Enqueue([]() { BuildQuestDatabase(); });
			}
			ImGui::TextColored(kGold, u8"Čtu úkoly ze skriptů hry…");
			return;
		}
		const double now = ImGui::GetTime();
		if (g_diary.pending && now > g_diary.nextRefresh + 5.0)
			g_diary.pending = false;   // the request was dropped (loading screen)
		if (!g_diary.pending && now >= g_diary.nextRefresh) {
			g_diary.pending = true;
			g_diary.nextRefresh = now + 1.0;
			Enqueue([]() { RefreshDiary(); });
		}
		if (g_questBoard.pending && now > g_questBoard.nextRefresh + 5.0)
			g_questBoard.pending = false;
		if (!g_questBoard.pending && now >= g_questBoard.nextRefresh) {
			g_questBoard.pending = true;
			g_questBoard.nextRefresh = now + 2.0;
			Enqueue([]() { RefreshQuestBoard(); });
		}

		std::vector<QuestRow> all = AllQuestRows();
		const std::string needle = ::GT::Text::Fold(s.search);
		int counts[kShowCount] = {};
		for (const QuestRow& r : all)
			for (int f = 0; f < kShowCount; f++)
				if (AcceptQuestRow(r, f, s.notes, needle))
					counts[f]++;

		ImGui::SetNextItemWidth(Em(16.0f));
		ImGui::InputTextWithHint("##questsearch", u8"Hledat úkol (luk, Lobart, drak…)", s.search, sizeof(s.search));
		ImGui::SameLine();
		static const char* sortNames[kSortCount] = { u8"Podle postupu", u8"Podle oblasti", u8"Podle názvu" };
		ImGui::SetNextItemWidth(Em(9.5f));
		ImGui::Combo("##questsort", &s.sort, sortNames, kSortCount);
		Tooltip(u8"Podle postupu: nahoře úkoly, které jdou dokončit hned, pak ostatní podle vzdálenosti dalšího kroku a nakonec úkoly v jiném světě.\nPodle oblasti: seskupené podle místa, kde je další krok.\nPodle názvu: abecedně.");
		ImGui::SameLine();
		ImGui::Checkbox(u8"Poznámky", &s.notes);
		Tooltip(u8"Ukázat i poznámky z deníku (kdo co prodává, kdo co učí).");
		ImGui::SameLine();
		if (ImGui::Button(u8"Přehled")) {
			s.selectedRaw.clear();
			s.selectedIsDiaryOnly = false;
		}
		Tooltip(u8"Co jde dokončit hned a které nové úkoly si můžeš vzít.");
		static const char* filterNames[kShowCount] = { u8"Probíhající", u8"Splněné", u8"Nesplněné", u8"Nezačaté", u8"Všechny" };
		for (int f = 0; f < kShowCount; f++) {
			if (f)
				ImGui::SameLine();
			std::string label = ::GT::Text::Format("%s (%d)##qf%d", filterNames[f], counts[f], f);
			ImGui::RadioButton(label.c_str(), &s.filter, f);
		}

		std::vector<QuestRow> rows;
		for (const QuestRow& r : all)
			if (AcceptQuestRow(r, s.filter, s.notes, needle))
				rows.push_back(r);

		// the selection stays visible in the detail even when the filter hides it
		const QuestRow* selected = nullptr;
		if (!s.selectedRaw.empty())
			for (const QuestRow& r : all)
				if (RawOf(r) == s.selectedRaw && (r.quest < 0) == s.selectedIsDiaryOnly) {
					selected = &r;
					break;
				}

		if (ImGui::BeginTable("##questsLayout", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable, ImGui::GetContentRegionAvail())) {
			ImGui::TableSetupColumn("list", ImGuiTableColumnFlags_WidthStretch, 1.05f);
			ImGui::TableSetupColumn("detail", ImGuiTableColumnFlags_WidthStretch, 1.85f);
			ImGui::TableNextColumn();
			if (ImGui::BeginChild("##questlist", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
				const std::vector<QuestSection> sections = GroupQuestRows(rows, s.filter, s.sort);
				const bool headers = sections.size() > 1 || (!sections.empty() && !sections[0].title.empty());
				for (const QuestSection& sec : sections) {
					if (headers && !sec.title.empty()) {
						ImGui::PushStyleColor(ImGuiCol_Text, sec.color);
						ImGui::SeparatorText(::GT::Text::Format("%s (%d)", sec.title.c_str(), (int)sec.rows.size()).c_str());
						ImGui::PopStyleColor();
					}
					for (int idx : sec.rows) {
						const QuestRow& r = rows[idx];
						ImGui::PushID(idx);
						if (DrawQuestRow(r, selected && SameQuestRow(*selected, r))) {
							s.selectedRaw = RawOf(r);
							s.selectedIsDiaryOnly = r.quest < 0;
						}
						ImGui::PopID();
					}
				}
				if (rows.empty())
					ImGui::TextDisabled(u8"Nic nenalezeno.");
			}
			ImGui::EndChild();

			ImGui::TableNextColumn();
			if (ImGui::BeginChild("##questdetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
				if (selected)
					QuestDetail(*selected);
				else
					QuestDashboard(all);
			}
			ImGui::EndChild();
			ImGui::EndTable();
		}
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
