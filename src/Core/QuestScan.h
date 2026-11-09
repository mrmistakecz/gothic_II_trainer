// Quest analysis core: rebuilds the quests of Gothic II from Daedalus bytecode.
//
// The scripts never describe a quest as one object. A quest is a diary topic
// (a TOPIC_x string constant), usually with a mission variable (MIS_x set to
// LOG_RUNNING, LOG_SUCCESS, ...) and the dialogs that create the topic, add
// diary entries and finally set the variable to LOG_SUCCESS. This code walks
// the bytecode of every dialog (C_INFO) and rebuilds that picture: who gives
// the quest, who completes it, which items the completing dialog checks for,
// which NPCs must be dead and what the hero gets as a reward.
//
// Engine independent: the plugin feeds it the parser's symbols, the unit test
// feeds it a compiled GOTHIC.DAT. Include the standard headers before this file
// (the plugin includes it after the Union headers, so no std includes here).
#pragma once

namespace GT {
namespace QuestScan {

	// Daedalus symbol types and flags (zParser_Const.h / zParser_Symbol.h)
	enum : int {
		kTypeVoid = 0, kTypeFloat = 1, kTypeInt = 2, kTypeString = 3,
		kTypeClass = 4, kTypeFunc = 5, kTypePrototype = 6, kTypeInstance = 7
	};
	enum : unsigned { kFlagConst = 1, kFlagReturn = 2, kFlagClassVar = 4, kFlagExternal = 8 };

	// What the analyzer needs to know about the loaded scripts.
	class Scripts {
	public:
		virtual ~Scripts() {}
		virtual int Count() const = 0;
		virtual int Type(int sym) const = 0;
		virtual unsigned Flags(int sym) const = 0;
		virtual int Elements(int sym) const = 0;                          // parameters of a function
		virtual int Content(int sym) const = 0;                           // code address of a function
		virtual bool IntValue(int sym, int& out) const = 0;               // value of an int constant
		virtual bool StringValue(int sym, std::string& out) const = 0;    // raw text, game encoding
		virtual std::string Name(int sym) const = 0;                      // upper case
		virtual int Find(const char* upperName) const = 0;                // -1 if missing
		virtual int BaseClass(int sym) const = 0;                         // class of an instance, -1
		virtual const unsigned char* Code() const = 0;
		virtual int CodeSize() const = 0;
	};

	// Input: one dialog (C_INFO instance) as the game created it.
	struct Dialog {
		int info = -1;          // instance symbol
		int npc = -1;           // NPC instance symbol
		int conditions = -1;    // function symbols
		int information = -1;
		bool important = false;
		bool permanent = false;
		std::string line;       // description, raw game encoding
	};

	struct Amount {
		int item = -1;
		int amount = 0;
		int group = 0;          // > 0: one item of this group is enough
	};

	// A lesson: a call of one of the game's B_Teach* functions
	// (B_TeachAttributePoints(self, other, ATR_STRENGTH, 1, 100), ...).
	struct Teach {
		int func = -1;          // the B_Teach* function
		int label = -1;         // string constant the menu shows for it ("Síla + 1"), -1 none
		int what = 0;           // its third argument: attribute, talent, potion, spell, circle ...
		int whatSym = -1;       // the constant that argument was written with, -1 a plain number
		int amount = 0;         // points or percent (4th argument), 0 when it has none
		int max = 0;            // the teacher's maximum (5th argument), 0 when it has none
	};

	struct DialogFacts {
		Dialog dialog;
		bool choices = false;            // continues with Info_AddChoice options
		std::vector<Amount> needs;       // condition: Npc_HasItems(hero, item) >= amount
		std::vector<int> mustBeDead;     // condition: Npc_IsDead(npc)
		std::vector<int> needsDialogs;   // condition: Npc_KnowsInfo(hero, info)
		std::vector<int> anyDialogs;     // condition: one of Npc_KnowsInfo(hero, a) || Npc_KnowsInfo(hero, b) ...
		int minChapter = 0;              // condition: Kapitel >= n
		int xp = 0;                      // B_GivePlayerXP in the dialog itself
		int choiceXp = 0;                // biggest B_GivePlayerXP in one of its choices
		std::vector<Amount> rewards;     // items the hero gets (not counting choices)
		std::vector<Amount> choiceRewards;   // items the hero gets in one of its choices
		std::vector<Amount> handIn;      // items the hero hands over
		std::vector<Teach> teaches;      // what the dialog teaches, its choices included
	};

	enum : unsigned { kRoleStart = 1, kRoleEntry = 2, kRoleSuccess = 4, kRoleFail = 8 };

	struct Step {
		int dialog = -1;        // index into Model::dialogs
		unsigned roles = 0;
	};

	struct Quest {
		int topic = -1;                     // string constant symbol
		std::string raw;                    // title, raw game encoding
		int section = 0;                    // 0 = mission, 1 = note
		std::vector<int> missionVars;       // int variables with the quest state
		int endChapter = 0;                 // from B_CloseTopic: obsolete from this chapter on
		std::vector<Step> steps;
		std::vector<std::string> texts;     // diary texts the scripts can write, raw
	};

	struct Model {
		std::vector<Quest> quests;
		std::vector<DialogFacts> dialogs;
		std::vector<std::pair<int, std::string> > npcWorlds;   // NPC instance -> world key
		int logRunning = 1, logSuccess = 2, logFailed = 3, logObsolete = 4, logNote = 1;
		int chapterVar = -1;
		int analyzedFunctions = 0;
	};

	// ------------------------------------------------------------------ bytecode walker

	enum : unsigned char {
		kOpIs = 9, kOpLogOr = 11, kOpLogAnd = 12,
		kOpLower = 7, kOpHigher = 8, kOpLowerEq = 15, kOpEqual = 16, kOpNotEqual = 17, kOpHigherEq = 18,
		kOpIsPlus = 19, kOpIsDiv = 22, kOpUnaryNot = 32,
		kOpReturn = 60, kOpCall = 61, kOpCallExtern = 62, kOpPushInt = 64, kOpPushVar = 65, kOpPushInst = 67,
		kOpAssignStr = 70, kOpAssignInst = 74, kOpJump = 75, kOpJumpF = 76, kOpSetInst = 80, kOpPushArrayVar = 245,
	};

	enum : unsigned char { kValNone, kValInt, kValVar, kValInst, kValHas, kValNeed, kValGroup, kValOther, kValKnows, kValDead, kValLabel };
	enum : unsigned char { kWhoUnknown, kWhoHero, kWhoSelf, kWhoNpc };

	struct Value {
		unsigned char kind = kValNone;
		int a = -1;             // int, symbol, need index or group
		unsigned char who = kWhoUnknown;   // kValHas: whose inventory
		int item = -1;          // kValHas: item instance
	};

	struct Known {   // symbols resolved once
		int self = -1, other = -1, hero = -1, chapter = -1, npcClass = -1;
		int logCreate = -1, logStatus = -1, logAdd = -1, logEntry = -1, closeTopic = -1;
		int giveXp = -1, giveItems = -1, createItems = -1, createItem = -1;
		int hasItems = -1, knowsInfo = -1, isDead = -1, addChoice = -1, insertNpc = -1;
		int buildLearn = -1;                   // B_BuildLearnString: the text of a lesson in the menu
		std::unordered_set<int> teachFuncs;    // B_Teach* functions
		std::unordered_map<int, int> funcAt;   // code address -> function symbol
	};

	struct Events {
		struct Topic { unsigned char kind; int topic; int arg; };   // 0 create, 1 status, 2 entry (arg: text symbol)
		struct Close { int topic, mission, common, chapter; };
		std::vector<Topic> topics;
		std::vector<std::pair<int, int> > assigns;                 // variable, value (INT_MIN: unknown)
		std::vector<Amount> needs, rewards, choiceRewards, handIn;
		std::vector<int> dead, knows, anyKnows, spawns;
		std::vector<Close> closes;
		std::vector<Teach> teaches;
		int xp = 0, choiceXp = 0, minChapter = 0, groups = 0;
		bool choices = false;
	};

	class Walker {
	public:
		Walker(const Scripts& s, const Known& k) : s_(s), k_(k), code_(s.Code()), size_(s.CodeSize()), count_(s.Count()) {}

		// Walks a function body, following calls into script functions up to maxDepth.
		// pastReturn: keep reading after an early "return" when a jump leads
		// further (dialog effects: "if (dead) { ...; return; }; ... LOG_SUCCESS").
		void Walk(int funcSym, Events& ev, int maxDepth, bool pastReturn = false) {
			visited_.clear();
			inChoice_ = 0;
			choiceLabel_ = -1;
			pastReturn_ = pastReturn;
			if (funcSym >= 0 && funcSym < count_ && s_.Type(funcSym) == kTypeFunc)
				Run(s_.Content(funcSym), ev, 0, maxDepth);
		}

		int walked = 0;

	private:
		const Scripts& s_;
		const Known& k_;
		const unsigned char* code_;
		int size_;
		int count_;
		std::unordered_set<int> visited_;
		int inChoice_ = 0;
		int choiceLabel_ = -1;   // the menu text of the choice being walked
		bool pastReturn_ = false;

		bool ReadInt(int pos, int& out) const {
			if (pos < 0 || pos + 4 > size_)
				return false;
			std::memcpy(&out, code_ + pos, 4);
			return true;
		}

		bool ConstInt(const Value& v, int& out) const {
			if (v.kind == kValInt) {
				out = v.a;
				return true;
			}
			if (v.kind == kValVar && v.a >= 0 && v.a < count_ && s_.Type(v.a) == kTypeInt && (s_.Flags(v.a) & kFlagConst))
				return s_.IntValue(v.a, out);
			return false;
		}

		unsigned char Who(const Value& v) const {
			if (v.kind != kValInst && v.kind != kValInt)
				return kWhoUnknown;
			if (v.a == k_.self)
				return kWhoSelf;
			if (v.a == k_.other || v.a == k_.hero)
				return kWhoHero;
			return kWhoNpc;
		}

		bool IsNpc(int sym) const {
			return sym >= 0 && sym < count_ && s_.Type(sym) == kTypeInstance && k_.npcClass >= 0 && s_.BaseClass(sym) == k_.npcClass;
		}

		bool IsStringConst(const Value& v) const {
			return v.kind == kValVar && v.a >= 0 && v.a < count_ && s_.Type(v.a) == kTypeString;
		}

		int AddNeed(Events& ev, const Value& has, int amount, int group) {
			if (has.who != kWhoHero || has.item < 0 || amount <= 0)
				return -1;
			Amount a;
			a.item = has.item;
			a.amount = amount;
			a.group = group;
			ev.needs.push_back(a);
			return (int)ev.needs.size() - 1;
		}

		// "if (Npc_HasItems(hero, x))", "&&" chains: the item is required
		void Require(Events& ev, const Value& v) {
			if (v.kind == kValHas)
				AddNeed(ev, v, 1, 0);
		}

		// "a || b": any one of the items is enough
		Value Alternatives(Events& ev, const Value& left, const Value& right) {
			int group = 0;
			if (left.kind == kValGroup)
				group = left.a;
			else if (right.kind == kValGroup)
				group = right.a;
			if (!group)
				group = ++ev.groups;
			const Value* both[2] = { &left, &right };
			for (const Value* v : both) {
				if (v->kind == kValHas)
					AddNeed(ev, *v, 1, group);
				else if (v->kind == kValNeed && v->a >= 0 && v->a < (int)ev.needs.size())
					ev.needs[v->a].group = group;
				else if (v->kind == kValGroup && v->a != group)
					for (Amount& n : ev.needs)
						if (n.group == v->a)
							n.group = group;
			}
			Value r;
			r.kind = kValGroup;
			r.a = group;
			return r;
		}

		// "!Npc_KnowsInfo(hero, x)", "Npc_IsDead(npc) == FALSE": the opposite of a requirement
		static void Negate(Events& ev, const Value& v) {
			if (v.kind == kValKnows && v.a >= 0 && v.a < (int)ev.knows.size())
				ev.knows[v.a] = -1;
			else if (v.kind == kValDead && v.a >= 0 && v.a < (int)ev.dead.size())
				ev.dead[v.a] = -1;
		}

		// "Npc_KnowsInfo(hero, a) || Npc_IsDead(b)": either is enough, so neither
		// is a must. A known dialog stays as one of the alternatives; a death is
		// dropped (it is often the way to fail the quest).
		static void Alternative(Events& ev, const Value& v) {
			if (v.kind == kValKnows && v.a >= 0 && v.a < (int)ev.knows.size()) {
				if (ev.knows[v.a] >= 0)
					ev.anyKnows.push_back(ev.knows[v.a]);
				ev.knows[v.a] = -1;
			}
			else if (v.kind == kValDead && v.a >= 0 && v.a < (int)ev.dead.size()) {
				ev.dead[v.a] = -1;
			}
		}

		Value Compare(Events& ev, unsigned char op, const Value& left, const Value& right) {
			Value r;
			r.kind = kValOther;
			if (op != kOpLogOr && op != kOpLogAnd) {
				const Value* flag = nullptr;
				const Value* other = nullptr;
				if (left.kind == kValKnows || left.kind == kValDead) {
					flag = &left;
					other = &right;
				}
				else if (right.kind == kValKnows || right.kind == kValDead) {
					flag = &right;
					other = &left;
				}
				if (flag) {
					int n = 0;
					if (ConstInt(*other, n) && ((op == kOpEqual && n == 0) || (op == kOpNotEqual && n != 0)))
						Negate(ev, *flag);
					return r;
				}
			}
			if (op == kOpLogOr) {
				Alternative(ev, left);
				Alternative(ev, right);
				bool interesting = left.kind == kValHas || left.kind == kValNeed || left.kind == kValGroup
					|| right.kind == kValHas || right.kind == kValNeed || right.kind == kValGroup;
				return interesting ? Alternatives(ev, left, right) : r;
			}
			if (op == kOpLogAnd) {
				Require(ev, left);
				Require(ev, right);
				return r;
			}
			// Npc_HasItems(hero, x) >= n   (left is the top of the stack)
			const Value* has = nullptr;
			const Value* other = nullptr;
			unsigned char o = op;
			if (left.kind == kValHas) {
				has = &left;
				other = &right;
			}
			else if (right.kind == kValHas) {
				has = &right;
				other = &left;
				if (op == kOpLower)
					o = kOpHigher;
				else if (op == kOpHigher)
					o = kOpLower;
				else if (op == kOpLowerEq)
					o = kOpHigherEq;
				else if (op == kOpHigherEq)
					o = kOpLowerEq;
			}
			if (has) {
				int n = 0;
				if (!ConstInt(*other, n))
					return r;
				int need = 0;
				if (o == kOpHigherEq || o == kOpEqual)
					need = n;
				else if (o == kOpHigher)
					need = n + 1;
				else if (o == kOpNotEqual && n == 0)
					need = 1;
				int index = need > 0 ? AddNeed(ev, *has, need, 0) : -1;
				if (index >= 0) {
					r.kind = kValNeed;
					r.a = index;
				}
				return r;
			}
			// Kapitel >= n
			if (k_.chapter >= 0) {
				int n = 0;
				if (left.kind == kValVar && left.a == k_.chapter && ConstInt(right, n)) {
					if (op == kOpHigherEq || op == kOpEqual)
						ev.minChapter = n > ev.minChapter ? n : ev.minChapter;
					else if (op == kOpHigher)
						ev.minChapter = n + 1 > ev.minChapter ? n + 1 : ev.minChapter;
				}
			}
			return r;
		}

		void Call(int fsym, const Value* args, int n, Events& ev, int depth, int maxDepth) {
			int v = 0;
			if (n >= 3 && k_.teachFuncs.count(fsym)) {
				// a lesson; the function itself is still walked below like any other
				Teach t;
				t.func = fsym;
				t.label = choiceLabel_;
				if (ConstInt(args[2], v))
					t.what = v;
				if (args[2].kind == kValVar && args[2].a >= 0 && args[2].a < count_ && (s_.Flags(args[2].a) & kFlagConst))
					t.whatSym = args[2].a;
				if (n >= 4 && ConstInt(args[3], v))
					t.amount = v;
				if (n >= 5 && ConstInt(args[4], v))
					t.max = v;
				ev.teaches.push_back(t);
			}
			if (fsym == k_.logCreate && n >= 2) {
				if (IsStringConst(args[0])) {
					Events::Topic t = { 0, args[0].a, ConstInt(args[1], v) ? v : 0 };
					ev.topics.push_back(t);
				}
			}
			else if (fsym == k_.logStatus && n >= 2) {
				if (IsStringConst(args[0])) {
					Events::Topic t = { 1, args[0].a, ConstInt(args[1], v) ? v : -1 };
					ev.topics.push_back(t);
				}
			}
			else if ((fsym == k_.logAdd || fsym == k_.logEntry) && n >= 2) {
				if (IsStringConst(args[0])) {
					Events::Topic t = { 2, args[0].a, IsStringConst(args[1]) ? args[1].a : -1 };
					ev.topics.push_back(t);
				}
			}
			else if (fsym == k_.closeTopic && n >= 4) {
				Events::Close c;
				c.topic = IsStringConst(args[0]) ? args[0].a : -1;
				c.mission = args[1].kind == kValVar ? args[1].a : -1;
				c.common = args[2].kind == kValVar ? args[2].a : -1;
				c.chapter = ConstInt(args[3], v) ? v : 0;
				ev.closes.push_back(c);
			}
			else if (fsym == k_.giveXp && n >= 1) {
				if (ConstInt(args[0], v) && v > 0) {
					if (inChoice_)
						ev.choiceXp = v > ev.choiceXp ? v : ev.choiceXp;
					else
						ev.xp = v > ev.xp ? v : ev.xp;
				}
			}
			else if (fsym == k_.giveItems && n >= 4) {
				int item = 0, amount = 1;
				if (ConstInt(args[2], item) && item >= 0) {
					if (!ConstInt(args[3], amount) || amount <= 0)
						amount = 1;
					Amount a;
					a.item = item;
					a.amount = amount;
					unsigned char from = Who(args[0]), to = Who(args[1]);
					if (from == kWhoSelf && to == kWhoHero)
						(inChoice_ ? ev.choiceRewards : ev.rewards).push_back(a);
					else if (from == kWhoHero && to == kWhoSelf)
						ev.handIn.push_back(a);
				}
			}
			else if ((fsym == k_.createItems && n >= 3) || (fsym == k_.createItem && n >= 2)) {
				int item = 0, amount = 1;
				if (Who(args[0]) == kWhoHero && ConstInt(args[1], item) && item >= 0) {
					if (n >= 3 && (!ConstInt(args[2], amount) || amount <= 0))
						amount = 1;
					Amount a;
					a.item = item;
					a.amount = amount;
					(inChoice_ ? ev.choiceRewards : ev.rewards).push_back(a);
				}
			}
			else if (fsym == k_.knowsInfo && n >= 2) {
				if (Who(args[0]) == kWhoHero && ConstInt(args[1], v))
					ev.knows.push_back(v);
			}
			else if (fsym == k_.isDead && n >= 1) {
				if ((args[0].kind == kValInst || args[0].kind == kValInt) && Who(args[0]) == kWhoNpc && IsNpc(args[0].a))
					ev.dead.push_back(args[0].a);
			}
			else if (fsym == k_.addChoice && n >= 3) {
				ev.choices = true;
				if (ConstInt(args[2], v) && v >= 0 && v < count_ && s_.Type(v) == kTypeFunc && depth < kMaxDepth) {
					const int savedLabel = choiceLabel_;
					if (args[1].kind == kValLabel)
						choiceLabel_ = args[1].a;
					else if (IsStringConst(args[1]) && (s_.Flags(args[1].a) & kFlagConst))
						choiceLabel_ = args[1].a;
					inChoice_++;
					Run(s_.Content(v), ev, depth + 1, maxDepth + 1 < kMaxDepth ? maxDepth + 1 : kMaxDepth);
					inChoice_--;
					choiceLabel_ = savedLabel;
				}
			}
			else if (fsym == k_.insertNpc && n >= 2) {
				if (ConstInt(args[0], v) && IsNpc(v))
					ev.spawns.push_back(v);
			}
			else if (depth < maxDepth && s_.Type(fsym) == kTypeFunc && !(s_.Flags(fsym) & kFlagExternal)) {
				Run(s_.Content(fsym), ev, depth + 1, maxDepth);
			}
		}

		static Value Pop(std::vector<Value>& stack) {
			Value v;
			if (!stack.empty()) {
				v = stack.back();
				stack.pop_back();
			}
			return v;
		}

		enum { kMaxDepth = 8 };

		void Run(int address, Events& ev, int depth, int maxDepth) {
			if (depth > kMaxDepth || address <= 0 || address >= size_ || !visited_.insert(address).second)
				return;
			walked++;
			std::vector<Value> stack;
			stack.reserve(16);
			int pc = address;
			int farthest = address;   // furthest jump target: code up to there belongs to the function
			for (int steps = 0; steps < 200000 && pc >= 0 && pc < size_; steps++) {
				const unsigned char op = code_[pc];
				int arg = 0;
				if (op == kOpPushInt || op == kOpPushVar || op == kOpPushInst) {
					if (!ReadInt(pc + 1, arg))
						return;
					Value v;
					v.kind = op == kOpPushInt ? kValInt : (op == kOpPushVar ? kValVar : kValInst);
					v.a = arg;
					stack.push_back(v);
					pc += 5;
				}
				else if (op == kOpPushArrayVar) {
					if (pc + 6 > size_)
						return;
					Value v;
					v.kind = kValOther;
					stack.push_back(v);
					pc += 6;
				}
				else if (op == kOpJumpF) {
					if (!ReadInt(pc + 1, arg))
						return;
					Require(ev, Pop(stack));
					if (arg > farthest)
						farthest = arg;
					pc += 5;
				}
				else if (op == kOpJump || op == kOpSetInst) {
					if (!ReadInt(pc + 1, arg))
						return;
					if (op == kOpJump && arg > farthest)
						farthest = arg;
					pc += 5;
				}
				else if (op == kOpCall || op == kOpCallExtern) {
					if (!ReadInt(pc + 1, arg))
						return;
					pc += 5;
					int fsym = -1;
					if (op == kOpCallExtern) {
						fsym = arg;
					}
					else {
						std::unordered_map<int, int>::const_iterator it = k_.funcAt.find(arg);
						if (it != k_.funcAt.end())
							fsym = it->second;
					}
					if (fsym < 0 || fsym >= count_) {
						stack.clear();
						continue;
					}
					int n = s_.Elements(fsym);
					Value args[8];
					const size_t knowsBefore = ev.knows.size();
					const size_t deadBefore = ev.dead.size();
					if (n < 0 || n > 8 || (int)stack.size() < n) {
						stack.clear();
					}
					else {
						for (int i = 0; i < n; i++)
							args[i] = stack[stack.size() - n + i];
						stack.resize(stack.size() - n);
						Call(fsym, args, n, ev, depth, maxDepth);
					}
					if (s_.Flags(fsym) & kFlagReturn) {
						Value r;
						r.kind = kValOther;
						if (fsym == k_.hasItems && n == 2) {
							int item = 0;
							if (ConstInt(args[1], item)) {
								r.kind = kValHas;
								r.who = Who(args[0]);
								r.item = item;
							}
						}
						else if (fsym == k_.knowsInfo && ev.knows.size() == knowsBefore + 1) {
							r.kind = kValKnows;
							r.a = (int)knowsBefore;
						}
						else if (fsym == k_.isDead && ev.dead.size() == deadBefore + 1) {
							r.kind = kValDead;
							r.a = (int)deadBefore;
						}
						else if (fsym == k_.buildLearn && n >= 1 && IsStringConst(args[0]) && (s_.Flags(args[0].a) & kFlagConst)) {
							r.kind = kValLabel;
							r.a = args[0].a;
						}
						stack.push_back(r);
					}
				}
				else if (op == kOpIs || (op >= kOpIsPlus && op <= kOpIsDiv)) {
					Value target = Pop(stack);
					Value value = Pop(stack);
					if (op == kOpIs && target.kind == kValVar) {
						int v = 0;
						ev.assigns.push_back(std::make_pair(target.a, ConstInt(value, v) ? v : INT_MIN));
					}
					pc += 1;
				}
				else if (op >= kOpAssignStr && op <= kOpAssignInst) {
					Pop(stack);
					Pop(stack);
					pc += 1;
				}
				else if (op <= kOpHigherEq && op != kOpIs && op != 10) {
					Value left = Pop(stack);
					Value right = Pop(stack);
					stack.push_back(Compare(ev, op, left, right));
					pc += 1;
				}
				else if (op >= 30 && op <= 33) {
					if (op == kOpUnaryNot && !stack.empty() && (stack.back().kind == kValHas || stack.back().kind == kValNeed || stack.back().kind == kValGroup)) {
						// "hero does NOT have it": not a requirement
						if (stack.back().kind == kValNeed && stack.back().a >= 0 && stack.back().a < (int)ev.needs.size())
							ev.needs[stack.back().a].amount = 0;
						stack.back().kind = kValOther;
					}
					else if (op == kOpUnaryNot && !stack.empty() && (stack.back().kind == kValKnows || stack.back().kind == kValDead)) {
						Negate(ev, stack.back());
						stack.back().kind = kValOther;
					}
					pc += 1;
				}
				else if (op == kOpReturn) {
					if (!pastReturn_ || pc >= farthest)
						return;
					stack.clear();   // an early return inside an "if": the rest still runs on other paths
					pc += 1;
				}
				else {
					pc += 1;
				}
				if (stack.size() > 64)
					stack.clear();
			}
		}
	};

	// ------------------------------------------------------------------ helpers

	// Length of the longest piece two names have in common.
	inline int CommonPiece(const std::string& a, const std::string& b) {
		int best = 0;
		for (size_t i = 0; i < a.size(); i++) {
			for (size_t len = (size_t)best + 1; i + len <= a.size(); len++) {
				if (b.find(a.substr(i, len)) == std::string::npos)
					break;
				best = (int)len;
			}
		}
		return best;
	}

	inline std::string Stem(const std::string& name) {
		std::string n = name;
		const char* prefixes[] = { "TOPIC_", "MIS_", "ADDON_" };
		for (const char* p : prefixes) {
			size_t at;
			while ((at = n.find(p)) != std::string::npos)
				n.erase(at, std::strlen(p));
		}
		std::string out;
		for (char c : n)
			if (c != '_')
				out += c;
		return out;
	}

	inline int Similarity(const std::string& a, const std::string& b) {
		return CommonPiece(Stem(a), Stem(b));
	}

	// ------------------------------------------------------------------ model

	inline Model Build(const Scripts& s, const std::vector<Dialog>& dialogs, const std::vector<std::string>& worlds) {
		Model m;
		Known k;
		k.self = s.Find("SELF");
		k.other = s.Find("OTHER");
		k.hero = s.Find("HERO");
		k.chapter = s.Find("KAPITEL");
		k.npcClass = s.Find("C_NPC");
		k.logCreate = s.Find("LOG_CREATETOPIC");
		k.logStatus = s.Find("LOG_SETTOPICSTATUS");
		k.logAdd = s.Find("LOG_ADDENTRY");
		k.logEntry = s.Find("B_LOGENTRY");
		k.closeTopic = s.Find("B_CLOSETOPIC");
		k.giveXp = s.Find("B_GIVEPLAYERXP");
		k.giveItems = s.Find("B_GIVEINVITEMS");
		k.createItems = s.Find("CREATEINVITEMS");
		k.createItem = s.Find("CREATEINVITEM");
		k.hasItems = s.Find("NPC_HASITEMS");
		k.knowsInfo = s.Find("NPC_KNOWSINFO");
		k.isDead = s.Find("NPC_ISDEAD");
		k.addChoice = s.Find("INFO_ADDCHOICE");
		k.insertNpc = s.Find("WLD_INSERTNPC");
		k.buildLearn = s.Find("B_BUILDLEARNSTRING");
		m.chapterVar = k.chapter;

		const char* logNames[] = { "LOG_RUNNING", "LOG_SUCCESS", "LOG_FAILED", "LOG_OBSOLETE", "LOG_NOTE" };
		int* logValues[] = { &m.logRunning, &m.logSuccess, &m.logFailed, &m.logObsolete, &m.logNote };
		for (int i = 0; i < 5; i++) {
			int sym = s.Find(logNames[i]);
			int v = 0;
			if (sym >= 0 && s.IntValue(sym, v))
				*logValues[i] = v;
		}

		const int count = s.Count();
		for (int i = 0; i < count; i++)
			if (s.Type(i) == kTypeFunc && !(s.Flags(i) & kFlagExternal) && s.Content(i) > 0) {
				k.funcAt.insert(std::make_pair(s.Content(i), i));
				if (s.Elements(i) >= 3 && s.Name(i).compare(0, 7, "B_TEACH") == 0)
					k.teachFuncs.insert(i);
			}

		Walker walker(s, k);

		// --- every dialog: what its condition checks and what it does
		std::vector<Events> doing;
		doing.reserve(dialogs.size());
		m.dialogs.reserve(dialogs.size());
		for (const Dialog& d : dialogs) {
			Events info, cond;
			walker.Walk(d.information, info, 3, true);
			walker.Walk(d.conditions, cond, 2);
			DialogFacts f;
			f.dialog = d;
			f.choices = info.choices;
			for (Amount a : cond.needs) {
				if (a.amount <= 0)
					continue;
				// the dialog itself may ask for more ("Npc_HasItems(other, ItPl_Beet) >= 20")
				for (const Amount& more : info.needs)
					if (more.item == a.item && more.amount > a.amount)
						a.amount = more.amount;
				f.needs.push_back(a);
			}
			for (int npc : cond.dead)
				if (npc >= 0 && std::find(f.mustBeDead.begin(), f.mustBeDead.end(), npc) == f.mustBeDead.end())
					f.mustBeDead.push_back(npc);
			for (int info : cond.knows)
				if (info >= 0 && std::find(f.needsDialogs.begin(), f.needsDialogs.end(), info) == f.needsDialogs.end())
					f.needsDialogs.push_back(info);
			for (int info : cond.anyKnows)
				if (info >= 0 && std::find(f.needsDialogs.begin(), f.needsDialogs.end(), info) == f.needsDialogs.end()
					&& std::find(f.anyDialogs.begin(), f.anyDialogs.end(), info) == f.anyDialogs.end())
					f.anyDialogs.push_back(info);
			f.minChapter = cond.minChapter;
			f.xp = info.xp;
			f.choiceXp = info.choiceXp;
			f.rewards = info.rewards;
			f.choiceRewards = info.choiceRewards;
			f.handIn = info.handIn;
			f.teaches = info.teaches;
			m.dialogs.push_back(f);
			doing.push_back(info);
		}

		// --- topics
		std::unordered_map<int, int> questOf;   // topic symbol -> index
		std::vector<std::unordered_map<int, unsigned> > roles;
		auto questFor = [&](int topic) -> int {
			std::unordered_map<int, int>::iterator it = questOf.find(topic);
			if (it != questOf.end())
				return it->second;
			Quest q;
			q.topic = topic;
			s.StringValue(topic, q.raw);
			m.quests.push_back(q);
			roles.push_back(std::unordered_map<int, unsigned>());
			int index = (int)m.quests.size() - 1;
			questOf.insert(std::make_pair(topic, index));
			return index;
		};
		for (size_t d = 0; d < doing.size(); d++) {
			for (const Events::Topic& t : doing[d].topics) {
				int q = questFor(t.topic);
				unsigned role = 0;
				if (t.kind == 0) {
					role = kRoleStart;
					m.quests[q].section = t.arg;
				}
				else if (t.kind == 1) {
					if (t.arg == m.logRunning)
						role = kRoleStart;
					else if (t.arg == m.logSuccess)
						role = kRoleSuccess;
					else if (t.arg == m.logFailed || t.arg == m.logObsolete)
						role = kRoleFail;
				}
				else {
					role = kRoleEntry;
					std::string text;
					if (t.arg >= 0 && s.StringValue(t.arg, text) && !text.empty()) {
						std::vector<std::string>& texts = m.quests[q].texts;
						if (std::find(texts.begin(), texts.end(), text) == texts.end())
							texts.push_back(text);
					}
				}
				if (role)
					roles[q][(int)d] |= role;
			}
		}

		// --- mission variables: the game's own B_CloseTopic tables first ...
		Events closes;
		int checkLog = s.Find("B_CHECKLOG");
		if (checkLog >= 0)
			walker.Walk(checkLog, closes, 3);
		std::unordered_map<int, std::vector<int> > missionOf;
		for (const Events::Close& c : closes.closes) {
			if (c.topic < 0)
				continue;
			int q = questFor(c.topic);
			if (c.chapter > 0)
				m.quests[q].endChapter = c.chapter;
			if (c.mission >= 0 && s.Type(c.mission) == kTypeInt && !(s.Flags(c.mission) & kFlagConst)) {
				std::vector<int>& vars = missionOf[c.topic];
				if (std::find(vars.begin(), vars.end(), c.mission) == vars.end())
					vars.push_back(c.mission);
			}
		}
		// ... then MIS_ variables set in the same dialog that opens or closes the topic
		const bool hasTables = !missionOf.empty();
		for (size_t d = 0; d < doing.size(); d++) {
			std::vector<int> topics, vars;
			for (const Events::Topic& t : doing[d].topics)
				if (t.kind != 2 && std::find(topics.begin(), topics.end(), t.topic) == topics.end())
					topics.push_back(t.topic);
			for (const std::pair<int, int>& a : doing[d].assigns) {
				bool state = a.second == m.logRunning || a.second == m.logSuccess || a.second == m.logFailed || a.second == m.logObsolete;
				if (state && s.Name(a.first).compare(0, 4, "MIS_") == 0 && std::find(vars.begin(), vars.end(), a.first) == vars.end())
					vars.push_back(a.first);
			}
			if (topics.empty() || vars.empty())
				continue;
			for (int topic : topics) {
				if (hasTables && missionOf.count(topic))
					continue;
				int best = -1, bestScore = 0;
				if (topics.size() == 1 && vars.size() == 1) {
					best = vars[0];
				}
				else {
					for (int var : vars) {
						int score = Similarity(s.Name(topic), s.Name(var));
						if (score > bestScore) {
							bestScore = score;
							best = var;
						}
					}
					if (bestScore < 4)
						best = -1;
				}
				if (best >= 0) {
					std::vector<int>& list = missionOf[topic];
					if (std::find(list.begin(), list.end(), best) == list.end())
						list.push_back(best);
				}
			}
		}
		for (Quest& q : m.quests) {
			std::unordered_map<int, std::vector<int> >::iterator it = missionOf.find(q.topic);
			if (it != missionOf.end())
				q.missionVars = it->second;
		}

		// --- dialogs that move a mission variable
		std::unordered_map<int, std::vector<int> > questsOfVar;
		for (size_t q = 0; q < m.quests.size(); q++)
			for (int var : m.quests[q].missionVars)
				questsOfVar[var].push_back((int)q);
		for (size_t d = 0; d < doing.size(); d++) {
			for (const std::pair<int, int>& a : doing[d].assigns) {
				std::unordered_map<int, std::vector<int> >::iterator it = questsOfVar.find(a.first);
				if (it == questsOfVar.end())
					continue;
				unsigned role = 0;
				if (a.second == m.logRunning)
					role = kRoleStart;
				else if (a.second == m.logSuccess)
					role = kRoleSuccess;
				else if (a.second == m.logFailed || a.second == m.logObsolete)
					role = kRoleFail;
				if (role)
					for (int q : it->second)
						roles[q][(int)d] |= role;
			}
		}

		// --- steps in a readable order: start, diary entries, success, failure
		for (size_t q = 0; q < m.quests.size(); q++) {
			std::vector<Step>& steps = m.quests[q].steps;
			const bool note = m.quests[q].section == m.logNote;
			for (const std::pair<const int, unsigned>& r : roles[q]) {
				Step st;
				st.dialog = r.first;
				st.roles = r.second;
				if (note)
					st.roles &= ~(unsigned)(kRoleSuccess | kRoleFail);   // a note is never completed
				if (st.roles)
					steps.push_back(st);
			}
			std::sort(steps.begin(), steps.end(), [](const Step& a, const Step& b) {
				auto rank = [](unsigned r) {
					return (r & kRoleStart) ? 0 : (r & kRoleSuccess) ? 2 : (r & kRoleFail) ? 3 : 1;
				};
				int ra = rank(a.roles), rb = rank(b.roles);
				return ra != rb ? ra < rb : a.dialog < b.dialog;
			});
		}

		// --- where NPCs live: the startup scripts of each world insert them
		std::unordered_set<int> placed;
		for (const std::string& world : worlds) {
			int startup = s.Find(("STARTUP_" + world).c_str());
			if (startup < 0)
				continue;
			Events spawns;
			walker.Walk(startup, spawns, 8);
			for (int npc : spawns.spawns)
				if (placed.insert(npc).second)
					m.npcWorlds.push_back(std::make_pair(npc, world));
		}

		m.analyzedFunctions = walker.walked;
		return m;
	}

} // namespace QuestScan
} // namespace GT
