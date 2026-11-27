// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.
#include "Papyrus.h"

#include "Shift.h"

#include <map>
#include <mutex>
#include <string>

namespace protean::Papyrus
{
	namespace
	{
		using Type = RE::BSScript::ObjectTypeInfo;

		bool Find(Type* a_type, const RE::BSFixedString& a_fn, bool a_static)
		{
			// the function lists are only laid out once the script is linked; GetScriptObjectType loads and links it
			for (RE::BSTSmartPointer<Type> t{ a_type }; t; t = t->parentTypeInfo) {
				if (!t->IsLinkedValid()) {
					return false;
				}
				if (a_static) {
					auto* f = t->GetGlobalFuncIter();
					for (std::uint32_t i = 0; i < t->GetNumGlobalFuncs(); ++i) {
						if (f[i].func && f[i].func->GetName() == a_fn) {
							return true;
						}
					}
					return false;  // a static function belongs to its own script only
				}
				auto* f = t->GetMemberFuncIter();
				for (std::uint32_t i = 0; i < t->GetNumMemberFuncs(); ++i) {
					if (f[i].func && f[i].func->GetName() == a_fn) {
						return true;
					}
				}
			}
			return false;
		}
	}

	bool Has(const char* a_class, const char* a_fn, bool a_static)
	{
		static std::mutex                            lock;
		static std::map<std::string, bool>           known;  // "Class.Function" -> found; the scripts don't change in a session
		const std::string                            key = std::string(a_class) + "." + a_fn;
		{
			std::scoped_lock l{ lock };
			if (auto it = known.find(key); it != known.end()) {
				return it->second;
			}
		}
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) {
			return false;
		}
		RE::BSTSmartPointer<Type> type;
		const bool found = vm->GetScriptObjectType(a_class, type) && type && Find(type.get(), a_fn, a_static);
		{
			std::scoped_lock l{ lock };
			known[key] = found;
		}
		if (found) {
			logger::info("papyrus: {} found", key);
		} else {
			logger::error("papyrus: {} NOT FOUND in the game's scripts; Protean skips it. If it is an SKSE function, SKSE's "
						  "script files are missing or another mod overwrites them (Scripts/{}.pex).",
				key, a_class);
			Shift::Notify("Protean: script function " + key + " is missing. Reinstall SKSE's Scripts, or check what overwrites " + a_class + ".pex.");
		}
		return found;
	}
}
