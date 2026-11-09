// Engine side: readable names read straight from the compiled scripts.
//
// "instance VLK_439_Vatras (Npc_Default) { name = "Vatras"; ... }" compiles to
//   PUSHVAR <string constant>   PUSHVAR C_NPC.NAME   ASSIGNSTR
// Scanning the bytecode of an instance (and of its prototypes) finds the name
// the game itself will give the NPC, in the language of the installed scripts,
// without creating the NPC. Works with every mod that uses the C_NPC class.

namespace GOTHIC_NAMESPACE {
namespace Trainer {

	// Daedalus opcodes (see zParser_Const.h). Only these carry operands.
	enum : unsigned char {
		kOpRet          = 60,
		kOpCall         = 61,
		kOpCallExtern   = 62,
		kOpPushInt      = 64,
		kOpPushVar      = 65,
		kOpPushInst     = 67,
		kOpAssignStr    = 70,
		kOpJump         = 75,
		kOpJumpF        = 76,
		kOpSetInstance  = 80,
		kOpPushArrayVar = 245,   // PUSHVAR + FLAGARRAY: symbol (4 bytes) + index (1 byte)
	};

	static bool ReadCodeInt(const unsigned char* code, int size, int pos, int& out) {
		if (pos < 0 || pos + 4 > size)
			return false;
		std::memcpy(&out, code + pos, 4);
		return true;
	}

	// Walks one function body (until RET) and returns the first non-empty string
	// constant assigned to the class member with symbol index memberIndex.
	static bool FindAssignedString(int address, int memberIndex, std::string& out) {
		if (!parser || !parser->stack.stack || memberIndex < 0)
			return false;
		const unsigned char* code = parser->stack.stack;
		const int size = parser->stack.stacksize;
		if (address <= 0 || address >= size)
			return false;
		const int symbols = parser->symtab.table.GetNumInList();

		int pc = address;
		int last = -1;   // symbol pushed by the previous instruction
		int prev = -1;   // symbol pushed before that
		for (int steps = 0; steps < 8192 && pc >= 0 && pc < size; steps++) {
			const unsigned char op = code[pc];
			int arg = 0;
			switch (op) {
				case kOpCall:
				case kOpCallExtern:
				case kOpPushInt:
				case kOpPushVar:
				case kOpPushInst:
				case kOpJump:
				case kOpJumpF:
				case kOpSetInstance:
					if (!ReadCodeInt(code, size, pc + 1, arg))
						return false;
					prev = last;
					last = op == kOpPushVar ? arg : -1;
					pc += 5;
					break;

				case kOpPushArrayVar:
					if (!ReadCodeInt(code, size, pc + 1, arg) || pc + 5 >= size)
						return false;
					prev = last;
					last = code[pc + 5] == 0 ? arg : -1;
					pc += 6;
					break;

				case kOpAssignStr:
					if (last == memberIndex && prev >= 0 && prev < symbols) {
						zCPar_Symbol* value = parser->GetSymbol(prev);
						if (value && value->type == zPAR_TYPE_STRING && !(value->flags & zPAR_FLAG_CLASSVAR)
							&& value->ele >= 1 && value->stringdata) {
							zSTRING raw;
							value->GetValue(raw, 0);
							std::string text = FromZ(raw);
							if (!text.empty()) {
								out = text;
								return true;
							}
						}
					}
					prev = last = -1;
					pc += 1;
					break;

				case kOpRet:
					return false;

				default:
					prev = last = -1;
					pc += 1;
					break;
			}
		}
		return false;
	}

	// Name an instance gets from its own code or, failing that, from its prototypes.
	static std::string ScriptInstanceString(int instanceIndex, int memberIndex) {
		std::string name;
		if (!parser || memberIndex < 0)
			return name;
		zCPar_Symbol* sym = parser->GetSymbol(instanceIndex);
		if (!sym || sym->type != zPAR_TYPE_INSTANCE)
			return name;
		if (FindAssignedString(sym->single_intdata, memberIndex, name))
			return name;
		zCPar_Symbol* proto = sym->parent;
		for (int depth = 0; proto && depth < 8; depth++) {
			if (proto->type != zPAR_TYPE_PROTOTYPE)
				break;
			if (FindAssignedString(proto->single_intdata, memberIndex, name))
				return name;
			proto = proto->parent;
		}
		return std::string();
	}

} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
