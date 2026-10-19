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

	Registry& Registry::Get()
	{
		static Registry r;
		return r;
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
		if (!_starter || !_revert) {
			logger::critical("starter/revert spell missing from {}", plugin);
			return false;
		}

		for (const auto& c : j.at("creatures")) {
			Creature cr;
			cr.id = c.at("id");
			cr.name = c.at("name");
			cr.size = c.value("size", "medium");
			const auto& r = c.at("race");
			cr.race = dh->LookupForm<RE::TESRace>(r.at("id").get<RE::FormID>(), r.at("file").get<std::string>());
			cr.heart = dh->LookupForm<RE::AlchemyItem>(c.at("heart").get<RE::FormID>(), plugin);
			cr.power = dh->LookupForm<RE::SpellItem>(c.at("power").get<RE::FormID>(), plugin);
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
