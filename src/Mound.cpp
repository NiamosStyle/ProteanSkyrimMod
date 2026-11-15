#include "Mound.h"

#include "Evolve.h"
#include "Registry.h"
#include "Shift.h"
#include "State.h"

namespace protean::Mound
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		// Its spot in Helgen's ruins, over rubble the user picks in game (dev 'moundhere' logs it). Unset = not placed
		// on its own yet.
		struct Spot
		{
			RE::NiPoint3 pos;
			float        angle;
		};
		std::optional<Spot> g_spot{};

		constexpr RE::FormID kTamriel = 0x0000003C;      // the Skyrim worldspace
		constexpr RE::FormID kHelgenIntro = 0x0003372B;  // MQ101, Unbound: the mound stays away while it runs
		constexpr float      kPlaceWithin = 8000.0f;     // placed once the player is this near its spot
		constexpr RE::FormID kShadow = 0x0EBECD;         // WerewolfTrans02FXS: the shapeshift's shadow over the body
		constexpr float      kShadowSeconds = 3.0f;
		constexpr RE::FormID kBlood = 0x000A6EF5;        // BloodSprayDecap01: a gush of blood (the Tendril's hits)
		constexpr auto       kSound = "QSTMiraakDeathTentacleStab";  // Dragonborn: tentacles spearing Miraak
		constexpr float      kShrivelAfter = 1.5f;       // seconds after the search before it shrinks away
		constexpr float      kShrivelSeconds = 1.2f;

		float             g_check{ 0.0f };
		float             g_time{ 0.0f };
		float             g_shrivel{ -1.0f };  // seconds since the search while it shrivels, < 0 = not
		RE::ObjectRefHandle g_searched{};
		bool              g_kept{ false };  // placed by dev 'moundhere': stays even for a player who has Protean (lining it up)

		RE::TESObjectREFR* Placed()
		{
			auto& s = State::Get();
			auto* ref = s.moundRef ? RE::TESForm::LookupByID<RE::TESObjectREFR>(s.moundRef) : nullptr;
			return ref && !ref->IsDeleted() && !ref->IsDisabled() ? ref : nullptr;
		}

		void Remove(RE::TESObjectREFR* a_ref)
		{
			if (a_ref) {
				a_ref->Disable();
				a_ref->SetDelete(true);
			}
		}

		RE::TESObjectREFR* Place(RE::PlayerCharacter* a_p, const Spot& a_spot)
		{
			auto* base = Registry::Get().MoundActivator();
			if (!base) {
				return nullptr;
			}
			auto ref = a_p->PlaceObjectAtMe(base, true);  // persistent: it stays where it is put, loaded or not
			if (!ref) {
				return nullptr;
			}
			ref->SetPosition(a_spot.pos);
			ref->SetAngle(RE::NiPoint3(0.0f, 0.0f, a_spot.angle));
			State::Get().moundRef = ref->GetFormID();
			logger::info("mound: placed {:08X} at ({:.0f}, {:.0f}, {:.0f}), angle {:.2f}", ref->GetFormID(), a_spot.pos.x, a_spot.pos.y,
				a_spot.pos.z, a_spot.angle);
			return ref.get();
		}

		bool InIntro()
		{
			auto* q = RE::TESForm::LookupByID<RE::TESQuest>(kHelgenIntro);
			return q && q->IsRunning();
		}

		void PlaySound(RE::TESObjectREFR* a_at, const char* a_name)
		{
			RE::BSSoundHandle h;
			if (auto* audio = RE::BSAudioManager::GetSingleton()) {
				audio->GetSoundHandleByName(h, a_name, 0x1A);
			}
			if (h.IsValid()) {
				h.SetPosition(a_at->GetPosition());
				h.Play();
			} else {
				logger::warn("mound: no sound '{}'", a_name);
			}
		}

		void Searched(RE::TESObjectREFR* a_mound, RE::PlayerCharacter* a_p)
		{
			auto& s = State::Get();
			if (s.moundUsed) {
				return;
			}
			s.moundUsed = true;
			PlaySound(a_mound, kSound);
			if (auto* art = RE::TESForm::LookupByID<RE::BGSArtObject>(kBlood)) {
				a_mound->ApplyArtObject(art, 2.0f);
			}
			// the poison, cast by the mound itself (as a poison dart trap's): a real hit on the player
			if (auto* poison = Registry::Get().MoundPoison()) {
				if (auto* caster = a_mound->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
					caster->CastSpellImmediate(poison, false, a_p, 1.0f, false, 0.0f, nullptr);
				}
			}
			if (auto* fx = RE::TESForm::LookupByID<RE::TESEffectShader>(kShadow)) {
				a_p->ApplyEffectShader(fx, kShadowSeconds);
			}
			Shift::Notify("The mound splits open, and something burrows into your flesh.");
			Evolve::Infect("the Strange Mound");
			g_searched = a_mound->CreateRefHandle();
			g_shrivel = 0.0f;
			logger::info("mound: searched");
		}

		struct ActivateSink : RE::BSTEventSink<RE::TESActivateEvent>
		{
			RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* a_event, RE::BSTEventSource<RE::TESActivateEvent>*) override
			{
				auto* p = RE::PlayerCharacter::GetSingleton();
				auto* base = Registry::Get().MoundActivator();
				if (a_event && p && base && a_event->actionRef.get() == p && a_event->objectActivated &&
					a_event->objectActivated->GetBaseObject() == base) {
					Searched(a_event->objectActivated.get(), p);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		void Pulse(RE::TESObjectREFR* a_ref, float a_scale)
		{
			auto* node = a_ref ? a_ref->Get3D() : nullptr;
			if (!node) {
				return;
			}
			node->local.scale = a_scale;
			RE::NiUpdateData ud;
			node->Update(ud);
		}
	}

	void Install()
	{
		static ActivateSink sink;
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESActivateEvent>(&sink);
	}

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		g_time += a_delta;
		auto& s = State::Get();
		// after a search: it shrinks into the ground, then is gone
		if (g_shrivel >= 0.0f) {
			g_shrivel += a_delta;
			auto ref = g_searched.get();
			const float t = (g_shrivel - kShrivelAfter) / kShrivelSeconds;
			if (ref && t > 0.0f) {
				Pulse(ref.get(), std::max(0.05f, 1.0f - t));
			}
			if (t >= 1.0f || !ref) {
				Remove(ref.get());
				s.moundRef = 0;
				g_shrivel = -1.0f;
				g_searched = {};
			}
			return;
		}
		auto* placed = Placed();
		// a slow, faint breathing while it waits
		if (placed) {
			Pulse(placed, 1.0f + 0.025f * std::sin(g_time * 1.6f));
		}
		if ((g_check += a_delta) < 1.0f) {
			return;
		}
		g_check = 0.0f;
		if (s.moundUsed || Evolve::Has("unbound_form")) {
			if (placed && !s.moundUsed && !g_kept) {  // Protean came another way (dev, an older save): no use for it
				Remove(placed);
				s.moundRef = 0;
				logger::info("mound: the player already has Protean: removed");
			}
			return;
		}
		if (placed || !g_spot) {
			return;
		}
		auto* ws = a_p->GetWorldspace();
		if (!ws || ws->GetFormID() != kTamriel || a_p->GetPosition().GetDistance(g_spot->pos) > kPlaceWithin || InIntro()) {
			return;
		}
		Place(a_p, *g_spot);
	}

	void OnGameLoaded()
	{
		g_shrivel = -1.0f;
		g_searched = {};
		g_check = 0.0f;
	}

	void Here(RE::PlayerCharacter* a_p)
	{
		Remove(Placed());
		State::Get().moundRef = 0;
		const auto pos = a_p->GetPosition();
		g_spot = Spot{ pos, a_p->GetAngleZ() };
		g_kept = true;
		logger::info("mound: SPOT ({:.1f}, {:.1f}, {:.1f}) angle {:.3f} in {} (copy into Mound.cpp kSpot)", pos.x, pos.y, pos.z,
			a_p->GetAngleZ(), a_p->GetWorldspace() ? a_p->GetWorldspace()->GetFormEditorID() : "an interior");
		Place(a_p, *g_spot);
	}

	void Reset()
	{
		auto& s = State::Get();
		Remove(Placed());
		s.moundRef = 0;
		s.moundUsed = false;
		g_shrivel = -1.0f;
		logger::info("mound: reset (unused, none placed)");
	}
}
