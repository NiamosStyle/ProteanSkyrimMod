#include "Evolve.h"

#include "Shift.h"
#include "State.h"

namespace protean::Evolve
{
	namespace
	{
		constexpr auto kUnbound = "unbound_form";

		// Protean perks the player has (each record is one rank)
		std::uint32_t Bought()
		{
			auto*         p = RE::PlayerCharacter::GetSingleton();
			std::uint32_t n = 0;
			for (const auto& [id, perk] : Registry::Get().Perks()) {
				if (p && p->HasPerk(perk)) {
					++n;
				}
			}
			return n;
		}

		// Disease immunity is a temporary modifier on the resistance; the state remembers how much is on.
		void SyncDisease(RE::PlayerCharacter* a_p)
		{
			auto&       s = State::Get();
			const float want = Has(kUnbound) ? 100.0f : 0.0f;
			if (std::abs(want - s.diseaseApplied) > 0.001f) {
				a_p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kResistDisease,
					want - s.diseaseApplied);
				s.diseaseApplied = want;
			}
		}

		// The stacks' regeneration is a temporary modifier on StaminaRateMult (the multiplier the game applies to
		// stamina regeneration, 100 = normal); the state remembers how much is on, so a load can take it off or keep it.
		// What Active Effects shows: an ability whose only effect states the total; it does nothing itself. Its
		// magnitude is set on the record and the ability given again whenever the total changes (an active effect
		// keeps the magnitude it started with), and after a load (the save keeps the record's own).
		bool g_boostShownStale = true;

		void ShowBoost(RE::PlayerCharacter* a_p, float a_total)
		{
			auto* spell = Registry::Get().KillBoostDisplay();
			if (!spell || spell->effects.empty()) {
				return;
			}
			if (a_p->HasSpell(spell)) {
				a_p->RemoveSpell(spell);
			}
			if (a_total > 0.0f) {
				spell->effects[0]->effectItem.magnitude = a_total;
				a_p->AddSpell(spell);
			}
		}

		void SyncBoost(RE::PlayerCharacter* a_p)
		{
			auto&       s = State::Get();
			const float want = Has(kUnbound) ? Registry::Get().Dial("Protean_KillBoostPct", 15.0f) * static_cast<float>(s.boosts.size()) : 0.0f;
			const bool  changed = std::abs(want - s.boostApplied) > 0.001f;
			if (changed) {
				a_p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStaminaRateMult,
					want - s.boostApplied);
				s.boostApplied = want;
			}
			if (changed || g_boostShownStale) {
				g_boostShownStale = false;
				ShowBoost(a_p, want);
			}
		}

		// Everything that follows from the points and the perks, however they changed (feeding, the menu, a load):
		// the points left in the menu's global, Unbound Form's gifts, and the Evolution power once there is
		// anything to spend or show.
		void Sync()
		{
			auto* p = RE::PlayerCharacter::GetSingleton();
			if (!p) {
				return;
			}
			auto& reg = Registry::Get();
			// each perk gained or lost, once (test aid: when a menu purchase took)
			static std::set<std::string> owned;
			for (const auto& [id, perk] : reg.Perks()) {
				const bool has = p->HasPerk(perk);
				if (has != owned.contains(id)) {
					has ? (void)owned.insert(id) : (void)owned.erase(id);
					logger::info("evolution: perk '{}' {}", id, has ? "owned" : "gone");
				}
			}
			// a placeholder perk (not built yet) is taken back: the point returns by itself, since points left are
			// worked out from the perks owned
			for (const auto& [id, perk] : reg.Perks()) {
				if (!reg.PerkBuilt(id) && p->HasPerk(perk)) {
					p->RemovePerk(perk);
					Shift::Notify(std::format("{} is not ready yet. Your perk point is returned.", perk->GetName()));
					logger::info("evolution: placeholder perk '{}' taken back", id);
				}
			}
			if (const auto left = static_cast<float>(PointsAvailable()); reg.Dial("Protean_PerkPoints", -1.0f) != left) {
				reg.SetGlobal("Protean_PerkPoints", left);
			}
			if (Has(kUnbound) && !p->HasSpell(reg.Starter())) {
				p->AddSpell(reg.Starter());  // the Instinct power: shapeshifting, and hearts in corpses
			}
			if (auto* fp = reg.FacePower(); fp && Has("borrowed_visage") && !p->HasSpell(fp)) {
				p->AddSpell(fp);  // Borrowed Visage: the Stolen Face power (face stealing itself asks for the perk)
				logger::info("evolution: the Stolen Face power given");
			}
			SyncDisease(p);
			const auto& s = State::Get();
			if (auto* evo = reg.Evolution(); evo && !p->HasSpell(evo) && (s.evoTotal > 0 || s.facePoints > 0 || Bought() > 0)) {
				p->AddSpell(evo);
				logger::info("evolution: the Evolution power given");
			}
		}
	}

	std::uint32_t PointsEarned(std::uint32_t a_total)
	{
		// the largest n with n (n + 1) / 2 <= total
		std::uint32_t n = 0;
		while ((n + 1) * (n + 2) / 2 <= a_total) {
			++n;
		}
		return n;
	}

	std::uint32_t PointsAvailable()
	{
		const auto&         s = State::Get();
		const std::uint32_t have = PointsEarned(s.evoTotal) + s.facePoints;
		const std::uint32_t spent = Bought();
		return have > spent ? have - spent : 0;
	}

	void Add(std::uint32_t a_points, std::string_view a_why)
	{
		auto&               s = State::Get();
		const std::uint32_t before = PointsEarned(s.evoTotal);
		s.evoTotal += a_points;
		const std::uint32_t after = PointsEarned(s.evoTotal);
		logger::info("evolution: +{} ({}), total {}, perk points earned {}, available {}", a_points, a_why, s.evoTotal, after,
			PointsAvailable());
		Sync();
		if (after > before) {
			Shift::Notify(std::format("You have a new Protean perk point ({} available).", PointsAvailable()));
		}
	}

	void AddFacePoint(std::string_view a_why)
	{
		auto& s = State::Get();
		++s.facePoints;
		logger::info("evolution: +1 perk point from a face ({}), available {}", a_why, PointsAvailable());
		Sync();
		Shift::Notify(std::format("You have a new Protean perk point ({} available).", PointsAvailable()));
	}

	void Feed(const Creature& a_c)
	{
		auto&               s = State::Get();
		const std::uint32_t n = ++s.feeds[a_c.id];
		if (n == 1) {
			Add(1, std::format("first heart of the {}", a_c.name));
		} else if (n == 5 || n == 10 || n == 20) {
			Add(1, std::format("{} hearts of the {}", n, a_c.name));
		} else {
			logger::info("evolution: heart {} of the {}", n, a_c.name);
		}
	}

	bool Has(std::string_view a_perk)
	{
		auto*       p = RE::PlayerCharacter::GetSingleton();
		auto* perk = Registry::Get().Perk(a_perk);
		return p && perk && p->HasPerk(perk);
	}

	bool Buy(std::string_view a_perk)
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		auto* perk = Registry::Get().Perk(a_perk);
		if (!p || !perk) {
			Shift::Notify("There is no such perk.");
			return false;
		}
		if (p->HasPerk(perk)) {
			Shift::Notify(std::format("You already have {}.", perk->GetName()));
			return false;
		}
		if (PointsAvailable() == 0) {
			Shift::Notify("You have no perk points.");
			return false;
		}
		p->AddPerk(perk);
		logger::info("perk bought (dev): '{}', points left {}", a_perk, PointsAvailable());
		Sync();
		return true;
	}

	void OpenMenu()
	{
		Sync();  // the menu shows Protean_PerkPoints: make it current first
		Registry::Get().SetGlobal("Protean_ShowPerkMenu", 1.0f);  // Custom Skills Framework opens the menu and clears it
		logger::info("evolution: perk menu requested, {} point(s) to spend", PointsAvailable());
	}

	std::uint32_t BoostStacks()
	{
		return static_cast<std::uint32_t>(State::Get().boosts.size());
	}

	void OnKill()
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		auto& s = State::Get();
		if (!p || !Has(kUnbound)) {
			return;
		}
		auto&       reg = Registry::Get();
		const float seconds = reg.Dial("Protean_KillBoostSeconds", 90.0f);
		const auto  cap = static_cast<std::size_t>(std::max(reg.Dial("Protean_KillBoostMax", 0.0f), 0.0f));
		if (cap > 0 && s.boosts.size() >= cap) {
			s.boosts.erase(s.boosts.begin());  // the oldest makes room: a kill at the cap renews the boost
		}
		s.boosts.push_back(seconds);
		SyncBoost(p);
		logger::info("kill boost: {} stack(s), +{:.0f} stamina regeneration", s.boosts.size(), s.boostApplied);
	}

	void OnFrame(float a_delta)
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		auto& s = State::Get();
		if (!p) {
			return;
		}
		// a perk bought in the menu shows up here: a few times a second is plenty
		static float sinceSync = 0.0f;
		if ((sinceSync += a_delta) >= 0.25f) {
			sinceSync = 0.0f;
			Sync();
		}
		if (s.boosts.empty() && s.boostApplied == 0.0f) {
			return;
		}
		for (auto& t : s.boosts) {
			t -= a_delta;
		}
		const auto n = s.boosts.size();
		std::erase_if(s.boosts, [](float t) { return t <= 0.0f; });
		SyncBoost(p);
		if (s.boosts.size() != n) {
			logger::info("kill boost: {} stack(s) left", s.boosts.size());
		}
	}

	void OnGameLoaded()
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (!p) {
			return;
		}
		// A save made with a boost on keeps the temporary modifier in the save proper and the stacks in the co-save,
		// so the two agree. Just make the modifiers match the stacks.
		g_boostShownStale = true;
		Sync();
		SyncBoost(p);
	}

	void Dump()
	{
		const auto& s = State::Get();
		std::string perks;
		for (const auto& [id, perk] : Registry::Get().Perks()) {
			perks += std::format(" {}={}", id, Has(id) ? 1 : 0);
		}
		std::string feeds;
		for (const auto& [id, n] : s.feeds) {
			feeds += std::format(" {}={}", id, n);
		}
		logger::info("dev: evolution total {} earned {} face {} bought {} available {} (global {}); perks{}; feeds{}; boost stacks {} "
		             "applied {:.0f}; disease {:.0f}",
			s.evoTotal, PointsEarned(s.evoTotal), s.facePoints, Bought(), PointsAvailable(), Registry::Get().Dial("Protean_PerkPoints", -1.0f),
			perks, feeds, s.boosts.size(), s.boostApplied, s.diseaseApplied);
	}
}
