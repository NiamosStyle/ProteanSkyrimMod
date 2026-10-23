#include "Evolve.h"

#include "Shift.h"
#include "State.h"

namespace protean::Evolve
{
	namespace
	{
		constexpr auto kUnbound = "unbound_form";

		// the perks that exist so far: id -> (name, ranks)
		struct Perk
		{
			const char*   id;
			const char*   name;
			std::uint32_t ranks;
		};
		constexpr std::array kPerks{ Perk{ kUnbound, "Unbound Form", 1 } };

		const Perk* Find(std::string_view a_id)
		{
			for (const auto& p : kPerks) {
				if (a_id == p.id) {
					return &p;
				}
			}
			return nullptr;
		}

		std::uint32_t Bought()
		{
			std::uint32_t n = 0;
			for (const auto& [id, rank] : State::Get().perks) {
				n += rank;
			}
			return n;
		}

		// Disease immunity is a temporary modifier on the resistance, taken once; the state remembers it was given.
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
		void SyncBoost(RE::PlayerCharacter* a_p)
		{
			auto&       s = State::Get();
			const float want = Has(kUnbound) ? Registry::Get().Dial("Protean_KillBoostPct", 15.0f) * static_cast<float>(s.boosts.size()) : 0.0f;
			if (std::abs(want - s.boostApplied) > 0.001f) {
				a_p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStaminaRateMult,
					want - s.boostApplied);
				s.boostApplied = want;
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
		if (after > before) {
			Shift::Notify(std::format("You have a new Protean perk point ({} available).", PointsAvailable()));
		}
	}

	void AddFacePoint(std::string_view a_why)
	{
		auto& s = State::Get();
		++s.facePoints;
		logger::info("evolution: +1 perk point from a face ({}), available {}", a_why, PointsAvailable());
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
		const auto& perks = State::Get().perks;
		const auto  it = perks.find(std::string(a_perk));
		return it != perks.end() && it->second > 0;
	}

	bool Buy(std::string_view a_perk)
	{
		auto*       p = RE::PlayerCharacter::GetSingleton();
		auto&       s = State::Get();
		const auto* perk = Find(a_perk);
		if (!perk) {
			Shift::Notify("There is no such perk.");
			return false;
		}
		const auto current = s.perks.find(perk->id);
		if (current != s.perks.end() && current->second >= perk->ranks) {
			Shift::Notify(std::format("You already have all of {}.", perk->name));
			return false;
		}
		if (PointsAvailable() == 0) {
			Shift::Notify("You have no perk points.");
			return false;
		}
		const std::uint32_t rank = ++s.perks[perk->id];
		logger::info("perk bought: '{}' rank {}, points left {}", perk->id, rank, PointsAvailable());
		Shift::Notify(std::format("You learned {}.", perk->name));
		if (a_perk == kUnbound) {
			// it opens shapeshifting: the Instinct power, which also makes hearts drop from corpses
			auto& reg = Registry::Get();
			if (!p->HasSpell(reg.Starter())) {
				p->AddSpell(reg.Starter());
			}
			SyncDisease(p);
		}
		return true;
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
		if (!p || (s.boosts.empty() && s.boostApplied == 0.0f)) {
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
		SyncDisease(p);
		SyncBoost(p);
	}

	void Dump()
	{
		const auto& s = State::Get();
		std::string perks;
		for (const auto& [id, rank] : s.perks) {
			perks += std::format(" {}={}", id, rank);
		}
		std::string feeds;
		for (const auto& [id, n] : s.feeds) {
			feeds += std::format(" {}={}", id, n);
		}
		logger::info("dev: evolution total {} earned {} face {} bought {} available {}; perks{}; feeds{}; boost stacks {} applied {:.0f}; "
		             "disease {:.0f}",
			s.evoTotal, PointsEarned(s.evoTotal), s.facePoints, Bought(), PointsAvailable(), perks, feeds, s.boosts.size(), s.boostApplied,
			s.diseaseApplied);
	}
}
