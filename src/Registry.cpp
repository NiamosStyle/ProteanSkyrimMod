#include "Registry.h"

namespace protean
{
	namespace
	{
		constexpr auto kTable = "Data/SKSE/Plugins/Protean/creatures.json";
	}

	float Creature::HealthAtLevel(int a_level) const
	{
		if (ladder.empty()) {
			return 0.0f;
		}
		float best = static_cast<float>(ladder.front().second);
		for (const auto& [level, health] : ladder) {
			if (level <= a_level) {
				best = static_cast<float>(health);
			}
		}
		return best;
	}

	float Registry::Dial(std::string_view a_name, float a_fallback) const
	{
		const auto it = _globals.find(std::string(a_name));
		return it != _globals.end() ? it->second->value : a_fallback;
	}

	void Registry::SetGlobal(std::string_view a_name, float a_value) const
	{
		if (const auto it = _globals.find(std::string(a_name)); it != _globals.end()) {
			it->second->value = a_value;
		}
	}

	Registry& Registry::Get()
	{
		static Registry r;
		return r;
	}

	RE::BGSPerk* Registry::Perk(std::string_view a_id) const
	{
		const auto it = _perks.find(a_id);
		return it != _perks.end() ? it->second : nullptr;
	}

	bool Registry::Load()
	{
		_creatures.clear();
		std::ifstream in(kTable);
		if (!in) {
			logger::critical("table {} not found", kTable);
			return false;
		}
		nlohmann::json j;
		try {
			in >> j;
		} catch (const std::exception& e) {
			logger::critical("table {} is not valid JSON: {}", kTable, e.what());
			return false;
		}

		auto* dh = RE::TESDataHandler::GetSingleton();
		const std::string plugin = j.value("plugin", "Protean.esp");
		if (!dh->LookupModByName(plugin)) {
			logger::critical("{} is not loaded; Protean does nothing without it", plugin);
			return false;
		}
		_starter = dh->LookupForm<RE::SpellItem>(j.at("starter").get<RE::FormID>(), plugin);
		_revert = dh->LookupForm<RE::SpellItem>(j.at("revert").get<RE::FormID>(), plugin);
		_beast = dh->LookupForm<RE::TESFaction>(j.value("beastFaction", 0u), plugin);
		_witness = dh->LookupForm<RE::TESFaction>(j.value("witnessFaction", 0u), plugin);
		if (!_beast || !_witness) {
			logger::warn("beast/witness factions missing: witnesses will not keep their hostility");
		}
		_facePower = dh->LookupForm<RE::SpellItem>(j.value("facePower", 0u), plugin);
		_disguise = dh->LookupForm<RE::TESFaction>(j.value("disguiseFaction", 0u), plugin);
		_exposed = dh->LookupForm<RE::TESFaction>(j.value("exposedFaction", 0u), plugin);
		if (!_facePower || !_disguise || !_exposed) {
			logger::warn("face stealing records missing from {}: regenerate the plugin", plugin);
		}
		_evolution = dh->LookupForm<RE::SpellItem>(j.value("evolution", 0u), plugin);
		if (const auto& c = j.value("claws", nlohmann::json::object()); !c.empty()) {
			_clawWeapon = dh->LookupForm<RE::TESObjectWEAP>(c.value("weapon", 0u), plugin);
			_clawRight = dh->LookupForm<RE::TESObjectARMO>(c.value("right", 0u), plugin);
			_clawLeft = dh->LookupForm<RE::TESObjectARMO>(c.value("left", 0u), plugin);
		}
		if (!_clawWeapon || !_clawRight || !_clawLeft) {
			logger::warn("Rending Claws records missing from {}", plugin);
		}
		if (const auto& t = j.value("tendril", nlohmann::json::object()); !t.empty()) {
			_tendril = dh->LookupForm<RE::SpellItem>(t.value("spell", 0u), plugin);
		}
		if (!_tendril || _tendril->effects.size() < 2) {
			logger::warn("Tendril records missing from {}", plugin);
			_tendril = nullptr;
		}
		_killBoost = dh->LookupForm<RE::SpellItem>(j.value("killBoost", 0u), plugin);
		_glideMarkers.clear();
		for (const auto id : j.value("glideMarkers", std::vector<RE::FormID>{})) {
			if (auto* sp = dh->LookupForm<RE::SpellItem>(id, plugin)) {
				_glideMarkers.push_back(sp);
			}
		}
		_predatorPoison = dh->LookupForm<RE::SpellItem>(j.value("predatorPoison", 0u), plugin);
		if (!_predatorPoison || _predatorPoison->effects.empty()) {
			logger::warn("Predator poison missing from {}", plugin);
			_predatorPoison = nullptr;
		}
		_stalkerFade = dh->LookupForm<RE::SpellItem>(j.value("stalker", nlohmann::json::object()).value("fade", 0u), plugin);
		_stalkerAmbush = dh->LookupForm<RE::BGSPerk>(j.value("stalker", nlohmann::json::object()).value("ambush", 0u), plugin);
		_stalkerPower = dh->LookupForm<RE::SpellItem>(j.value("stalker", nlohmann::json::object()).value("power", 0u), plugin);
		_animalArms = dh->LookupForm<RE::BGSPerk>(j.value("skills", nlohmann::json::object()).value("arms", 0u), plugin);
		_deft = dh->LookupForm<RE::BGSPerk>(j.value("skills", nlohmann::json::object()).value("deft", 0u), plugin);
		if (!_animalArms || !_deft || _animalArms->perkEntries.size() < 2 || _deft->perkEntries.empty()) {
			logger::warn("the hidden skill perks are missing from {}", plugin);
			_animalArms = _deft = nullptr;
		}
		{
			const auto d = j.value("deceiver", nlohmann::json::object());
			_assassinMark = dh->LookupForm<RE::TESFaction>(d.value("assassinMark", 0u), plugin);
			_assassinPerk = dh->LookupForm<RE::BGSPerk>(d.value("assassinPerk", 0u), plugin);
			_infection = dh->LookupForm<RE::TESBoundObject>(d.value("infection", 0u), plugin);
			_infectPower = dh->LookupForm<RE::SpellItem>(d.value("infectPower", 0u), plugin);
			_infectedFaction = dh->LookupForm<RE::TESFaction>(d.value("infectedFaction", 0u), plugin);
			_metamorphosisPower = dh->LookupForm<RE::SpellItem>(d.value("metamorphosisPower", 0u), plugin);
			if (!_assassinMark || !_assassinPerk || !_infection || !_infectPower || !_infectedFaction || !_metamorphosisPower) {
				logger::warn("a Deceiver record (Assassin, Infected) is missing from {}", plugin);
			}
		}
		{
			const auto d = j.value("devastator", nlohmann::json::object());
			_devastatorPower = dh->LookupForm<RE::SpellItem>(d.value("power", 0u), plugin);
			_devastatorTendril = dh->LookupForm<RE::SpellItem>(d.value("tendril", 0u), plugin);
			_regenPower = dh->LookupForm<RE::SpellItem>(d.value("regen", 0u), plugin);
			if (!_devastatorPower || !_devastatorTendril || !_regenPower) {
				logger::warn("Devastator or Regeneration records missing from {}", plugin);
			}
		}
		if (!_stalkerFade || !_stalkerAmbush || !_stalkerPower) {
			logger::warn("Stalker's fade, ambush perk or power missing from {}", plugin);
		}
		if (!_killBoost) {
			logger::warn("Kill Boost display missing from {}", plugin);
		}
		_keenPowers = {};
		_keenSenses = {};
		if (const auto& k = j.value("keen", nlohmann::json::object()); !k.empty()) {
			const char* names[] = { "night", "life", "dead" };
			for (std::size_t i = 0; i < 3; ++i) {
				_keenPowers[i] = dh->LookupForm<RE::SpellItem>(k["powers"].value(names[i], 0u), plugin);
				_keenSenses[i] = dh->LookupForm<RE::SpellItem>(k["senses"].value(names[i], 0u), plugin);
				if (!_keenPowers[i] || !_keenSenses[i]) {
					logger::warn("Keen records missing from {}", plugin);
				}
			}
		}
		_perks.clear();
		for (const auto& [id, local] : j.value("perks", nlohmann::json::object()).items()) {
			if (auto* perk = dh->LookupForm<RE::BGSPerk>(local.get<RE::FormID>(), plugin)) {
				_perks[id] = perk;
			}
		}
		_perksBuilt.clear();
		for (const auto& id : j.value("perksBuilt", nlohmann::json::array())) {
			_perksBuilt.insert(id.get<std::string>());
		}
		logger::info("registry: {} perk(s) ({} working), evolution power {}", _perks.size(), _perksBuilt.size(),
			_evolution ? "found" : "MISSING");
		_globals.clear();
		for (const auto& [name, local] : j.value("globals", nlohmann::json::object()).items()) {
			if (auto* g = dh->LookupForm<RE::TESGlobal>(local.get<RE::FormID>(), plugin)) {
				_globals[name] = g;
			}
		}
		logger::info("registry: {} global setting(s)", _globals.size());
		_openKeywords.clear();
		for (const auto& k : j.value("openInteriorKeywords", nlohmann::json::array())) {
			if (auto* kw = dh->LookupForm<RE::BGSKeyword>(k.at("id").get<RE::FormID>(), k.at("file").get<std::string>())) {
				_openKeywords.push_back(kw);
			}
		}
		logger::info("registry: {} cave-like location keyword(s)", _openKeywords.size());
		if (!_starter || !_revert) {
			logger::critical("starter/revert spell missing from {}", plugin);
			return false;
		}

		for (const auto& c : j.at("creatures")) {
			Creature cr;
			cr.id = c.at("id");
			cr.name = c.at("name");
			cr.size = c.value("size", "medium");
			cr.cameraHeight = c.value("camera_height", 60.0f);
			if (c.contains("skin")) {
				const auto& sk = c["skin"];
				cr.skin = dh->LookupForm<RE::TESObjectARMO>(sk.at("id").get<RE::FormID>(), sk.at("file").get<std::string>());
			}
			cr.height = c.value("height", 1.0f);
			cr.reach = c.value("reach", 0.0f);
			cr.strikeAngle = c.value("strike_angle", 0.0f);
			cr.stance = c.value("stance", std::string{});
			cr.swims = c.value("swims", false);
			cr.diet = c.value("diet", std::string{});
			cr.feedEvent = c.value("feed_event", std::string{});
			cr.stanceState = c.value("stance_state", 0);
			cr.flies = c.value("flies", false);
			cr.ownSprint = c.value("own_sprint", false);
			cr.sprint = c.value("sprint", 33.0f);
			cr.leap = c.value("leap", 0.0f);
			cr.leapHeight = c.value("leap_height", 0.0f);
			if (c.contains("rmb_spell")) {
				const auto& sp = c["rmb_spell"];
				cr.rmbSpell = dh->LookupForm<RE::SpellItem>(sp.at("id").get<RE::FormID>(), sp.at("file").get<std::string>());
				if (!cr.rmbSpell) {
					logger::warn("creature '{}': its RMB spell was not found", cr.id);
				}
			}
			cr.spellEvents = c.value("spell_events", std::vector<std::string>{});
			cr.taps = c.value("taps", std::vector<std::string>{});
			cr.spellStanceDelay = c.value("spell_stance_delay", 0.5f);
			cr.spellDelay = c.value("spell_delay", 0.3f);
			const auto& r = c.at("race");
			cr.race = dh->LookupForm<RE::TESRace>(r.at("id").get<RE::FormID>(), r.at("file").get<std::string>());
			cr.heart = dh->LookupForm<RE::AlchemyItem>(c.at("heart").get<RE::FormID>(), plugin);
			cr.power = dh->LookupForm<RE::SpellItem>(c.at("power").get<RE::FormID>(), plugin);
			cr.index = c.value("index", -1);
			cr.ability = dh->LookupForm<RE::SpellItem>(c.value("ability", 0u), plugin);
			if (!cr.race || !cr.heart || !cr.power) {
				logger::error("creature '{}' skipped: race {} heart {} power {}", cr.id, static_cast<bool>(cr.race),
					static_cast<bool>(cr.heart), static_cast<bool>(cr.power));
				continue;
			}
			for (const auto& f : c.value("factions", nlohmann::json::array())) {
				if (auto* fac = dh->LookupForm<RE::TESFaction>(f.at("id").get<RE::FormID>(), f.at("file").get<std::string>())) {
					cr.factions.push_back(fac);
				}
			}
			for (const auto& a : c.value("attacks", nlohmann::json::array())) {
				Attack at;
				at.event = a.value("event", "");
				for (const auto& fl : a.value("flags", nlohmann::json::array())) {
					const auto name = fl.get<std::string>();
					at.power = at.power || name == "Power Attack";
					at.bash = at.bash || name == "Bash Attack";
					at.rotating = at.rotating || name == "Rotating Attack";
				}
				at.hasSpell = a.contains("spell") && !a["spell"].is_null();
				if (!at.event.empty()) {
					cr.attacks.push_back(std::move(at));
				}
			}
			for (const auto& rung : c.value("ladder", nlohmann::json::array())) {
				cr.ladder.emplace_back(rung.at(0).get<int>(), rung.at(1).get<int>());
			}
			_creatures.push_back(std::move(cr));
		}
		logger::info("registry: {} creature(s) loaded", _creatures.size());
		return !_creatures.empty();
	}

	const Creature* Registry::ByPower(RE::FormID a_id) const
	{
		for (const auto& c : _creatures) {
			if (c.power->GetFormID() == a_id) {
				return &c;
			}
		}
		return nullptr;
	}

	const Creature* Registry::ByHeart(RE::FormID a_id) const
	{
		for (const auto& c : _creatures) {
			if (c.heart->GetFormID() == a_id) {
				return &c;
			}
		}
		return nullptr;
	}

	const Creature* Registry::ByRace(const RE::TESRace* a_race) const
	{
		for (const auto& c : _creatures) {
			if (c.race == a_race) {
				return &c;
			}
		}
		return nullptr;
	}

	const Creature* Registry::ByActor(const RE::Actor* a_actor) const
	{
		if (!a_actor) {
			return nullptr;
		}
		const auto*       race = a_actor->GetRace();
		const char* const raw = a_actor->GetName();
		std::string       name = raw ? raw : "";
		std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
		const Creature* fallback = nullptr;
		for (const auto& c : _creatures) {
			if (c.race != race) {
				continue;
			}
			if (!fallback) {
				fallback = &c;
			}
			std::string cn = c.name;
			std::transform(cn.begin(), cn.end(), cn.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
			if (cn == name) {
				return &c;
			}
		}
		return fallback;
	}

	const Creature* Registry::ById(std::string_view a_id) const
	{
		for (const auto& c : _creatures) {
			if (c.id == a_id) {
				return &c;
			}
		}
		return nullptr;
	}
}
