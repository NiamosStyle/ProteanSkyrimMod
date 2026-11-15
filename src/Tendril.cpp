#include "Tendril.h"

#include "Blade.h"
#include "Evolve.h"
#include "Registry.h"
#include "Shift.h"

namespace protean::Tendril
{
	namespace
	{
		using Clock = std::chrono::steady_clock;
		constexpr auto kPerk = "tendril";

		constexpr float kKnock = 15.0f;          // the yank: Pull Mastery's force (a negative knock pulls)
		constexpr float kHaulSpeed = 2200.0f;    // the player hauled to a large target, game units / s
		constexpr float kHaulGravity = 1000.0f;  // the haul's own gravity (as a leap's): a short, low arc
		// every throw: the vampire lord's swipe (Dawnguard), chosen by ear in game over a dragon's tail whip, a werewolf's,
		// a spriggan's and a chaurus's attack and an Apocrypha tentacle (user, 2026-10-23). Played here at the throw: as
		// the projectile's sound it came only after the tendril had come back.
		std::string g_throwSound{ "NPCVampireLordAttackLeft" };
		// on a hit, blood on the target: an art object or an effect shader (dev 'tendrilblood' compares them live)
		RE::FormID g_blood{ 0x000A6EF5 };  // BloodSprayDecap01: a gush of blood (Pull Mastery's spears use it too)
		constexpr float kHaulGap = 150.0f;       // stop this far short of the target's edge (at 60 the player bumped a mammoth)

		float g_cooldown{ 0.0f };     // seconds until the next throw
		Clock::time_point g_armed{};  // a paid throw is in flight until then: its hit may pull; no hit by then = a miss
		float             g_paid{ 0.0f };  // what the throw in flight cost: a miss gives it back (the user)
		Clock::time_point g_toldAt{};
		Clock::time_point g_thrownAt{};  // the last paid throw: the gate leaves its own cast alone a moment

		// The sleeve (2026-11-14): flesh, sinew and a bone harpoon on each arm whose hand holds the Tendril, in two
		// versions (user: "look like 2 when relaxed and 1 when you draw your weapon"): with weapons away the harpoon lies
		// along the fingers; from partway through the draw it stands out of the palm, where the spell leaves from. One
		// hidden piece per version and hand, never kept in the inventory; 1st person shows the drawn one only (with
		// weapons away the arms are off screen). The 1st-person spike runs through the 1st-person spell's starting
		// point (2026-11-14); the 3rd-person pieces draw nothing in 1st person (slot 61 is drawn there, and showed the
		// resting harpoon beside the 1st-person spike).
		struct Side
		{
			bool                                    left;
			RE::BGSBipedObjectForm::BipedObjectSlot slot;
			const char*                             firstName;
			const char*                             firstModel;
			const char*                             tag;
		};
		constexpr Side kSides[] = {
			{ false, RE::BGSBipedObjectForm::BipedObjectSlot::kModMisc1, "ProteanTendril1st_R", "Protean\\Tendril\\Sleeve1st_R_{}.nif", "tendril sleeve right" },  // 48
			{ true, RE::BGSBipedObjectForm::BipedObjectSlot::kFX01, "ProteanTendril1st_L", "Protean\\Tendril\\Sleeve1st_L_{}.nif", "tendril sleeve left" },       // 61
		};

		// dev 'tendrilaim <seconds>': where the 1st-person hands point while the Tendril is ready and as it is thrown, in
		// each hand's own space, so the 1st-person spike can be laid along the throw (tools/measure_magic_node.py turns
		// the logged numbers into the body's space). 2026-11-14: the right spike looked angled too low.
		float g_aimFor{ 0.0f };

		void LogAim(RE::PlayerCharacter* a_p, const char* a_when)
		{
			auto* root = a_p->Get3D(true);
			if (!root) {
				return;
			}
			const float  pitch = a_p->data.angle.x, yaw = a_p->data.angle.z;
			const auto   ahead = RE::NiPoint3(std::sin(yaw) * std::cos(pitch), std::cos(yaw) * std::cos(pitch), -std::sin(pitch));
			for (const auto* s : { "R", "L" }) {
				auto* hand = root->GetObjectByName(fmt::format("NPC {} Hand [{}Hnd]", s, s));
				auto* magic = root->GetObjectByName(fmt::format("NPC {} MagicNode [{}Mag]", s, s));
				if (!hand || !magic) {
					continue;
				}
				const auto back = hand->world.rotate.Transpose();
				const auto dir = back * ahead;
				const auto at = back * (magic->world.translate - hand->world.translate) / hand->world.scale;
				logger::info("tendril aim {} {}: ahead in hand space ({:.3f}, {:.3f}, {:.3f}), magic node ({:.2f}, {:.2f}, {:.2f})", s, a_when,
					dir.x, dir.y, dir.z, at.x, at.y, at.z);
			}
		}

		std::int32_t Count(RE::PlayerCharacter* a_p, RE::TESBoundObject* a_item)
		{
			const auto counts = a_p->GetInventoryCounts([a_item](RE::TESBoundObject& a_obj) { return &a_obj == a_item; });
			const auto it = counts.find(a_item);
			return it != counts.end() ? it->second : 0;
		}

		void Sleeves(RE::PlayerCharacter* a_p, RE::SpellItem* a_spell)
		{
			using WS = RE::WEAPON_STATE;
			const auto* st = a_p->AsActorState();
			const auto  ws = st ? st->GetWeaponState() : WS::kSheathed;
			// only while fully drawn, as the Blade, Shield and Hammerfist: with weapons away the sleeve is gone (user,
			// 2026-11-14: a sleeve left on while sheathed was "too incongruent" beside the others)
			const bool  drawn = ws == WS::kDrawn;
			const bool  human = !Shift::ActiveCreature();
			const auto& reg = Registry::Get();
			auto*       mgr = RE::ActorEquipManager::GetSingleton();
			for (const auto& side : kSides) {
				auto* rest = reg.TendrilSleeve(false, side.left);
				auto* out = reg.TendrilSleeve(true, side.left);
				if (!rest || !out) {
					continue;
				}
				const bool holds = human && a_p->GetEquippedObject(side.left) == a_spell;
				// weapons out: the palm harpoon. Tried keeping the resting one on instead (2026-11-14): it stuck up past the
				// fingers on every throw (user: "that won't do").
				// The resting piece is no longer worn (2026-11-14: nothing while sheathed); kept here so a save that still
				// wears it loses it.
				auto*      want = holds && drawn ? out : nullptr;
				auto*      worn = a_p->GetWornArmor(side.slot);
				Blade::HangFirstPerson(a_p, side.firstName, side.firstModel, holds && drawn, side.tag);
				for (auto* piece : { rest, out }) {
					if (piece == want) {
						continue;
					}
					if (worn == piece) {
						mgr->UnequipObject(a_p, piece, nullptr, 1, nullptr, false, true, false, true);
						worn = nullptr;
						logger::info("{}: goes back", side.tag);
						if (piece == out) {
							Blade::SleeveSound(a_p, false, side.tag);
						}
					}
					if (const auto n = Count(a_p, piece); n > 0) {
						a_p->RemoveItem(piece, n, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
					}
				}
				if (want && worn != want) {
					if (Count(a_p, want) == 0) {
						a_p->AddObjectToContainer(want, nullptr, 1, nullptr);
					}
					mgr->EquipObject(a_p, want, nullptr, 1, nullptr, false, false, false, true);
					logger::info("{}: grows (weapon state {})", side.tag, static_cast<int>(ws));
					Blade::SleeveSound(a_p, true, side.tag);
				}
			}
		}

		// the player hauled toward a large target (set every frame after the player's update, as a leap is)
		struct Haul
		{
			bool                on{ false };
			Clock::time_point   since{};
			RE::ObjectRefHandle target{};
			RE::NiPoint3        dir{};       // horizontal, unit
			float               speed{ 0.0f };
			float               vz0{ 0.0f };
			float               time{ 0.0f };  // seconds the arc takes
			float               stop{ 0.0f };  // horizontal distance to stop at
			float               gravity{ 1.0f };
			RE::NiPoint3        lastPos{};
		};
		Haul g_haul;

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		bool Ready(RE::PlayerCharacter* a_p)
		{
			return g_cooldown <= 0.0f && a_p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) >= Dial("Protean_TendrilCost", 25.0f);
		}

		// damage: a share of what Rending Claws deal on a full bar; drain: a base plus a little per level
		void SetMagnitudes(RE::PlayerCharacter* a_p, RE::SpellItem* a_spell)
		{
			const float level = static_cast<float>(a_p->GetLevel());
			const float claws = Dial("Protean_ClawDamageMax", 10.0f) + Dial("Protean_ClawDamagePerLevel", 0.7f) * level +
			                    Dial("Protean_ClawDamagePerLevelSq", 0.02f) * level * level;
			a_spell->effects[0]->effectItem.magnitude = std::round(claws * Dial("Protean_TendrilDamageShare", 0.5f));
			a_spell->effects[1]->effectItem.magnitude = std::round(Dial("Protean_TendrilDrain", 20.0f) + Dial("Protean_TendrilDrainPerLevel", 0.5f) * level);
		}

		// a throw begun while it isn't ready is broken off before it leaves the hand
		void Gate(RE::PlayerCharacter* a_p, RE::SpellItem* a_spell)
		{
			if (Ready(a_p) || Clock::now() - g_thrownAt < std::chrono::milliseconds(500)) {
				return;
			}
			using State = RE::MagicCaster::State;
			for (const auto source : { RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand }) {
				auto* caster = a_p->GetMagicCaster(source);
				if (!caster || caster->currentSpell != a_spell) {
					continue;
				}
				const auto state = caster->state.get();
				if (state == State::kNone || state == State::kUnk07 || state == State::kUnk08 || state == State::kUnk09) {
					continue;
				}
				caster->InterruptCast(false);
				if (Clock::now() - g_toldAt > std::chrono::seconds(1)) {
					g_toldAt = Clock::now();
					Shift::Notify(g_cooldown > 0.0f ? "Your tendril is not ready yet." : "You are too tired to lash out.");
				}
				logger::info("tendril: throw broken off ({}, cooldown {:.1f} s)", g_cooldown > 0.0f ? "cooling down" : "stamina", g_cooldown);
			}
		}

		void EndHaul(RE::PlayerCharacter* a_p, const char* a_why)
		{
			if (auto* cc = a_p->GetCharController()) {
				cc->gravity = g_haul.gravity;
				cc->fallStartHeight = a_p->GetPositionZ();  // a drop after the haul hurts from here, not from the arc
				const float s = RE::bhkWorld::GetWorldScale();
				cc->SetLinearVelocityImpl(RE::hkVector4(0.0f, 0.0f, std::min(0.0f, g_haul.vz0 - kHaulGravity * g_haul.time) * s, 0.0f));
			}
			logger::info("tendril: haul {} after {:.2f} s", a_why, std::chrono::duration<float>(Clock::now() - g_haul.since).count());
			g_haul = {};
		}

		void UpdateHaul(RE::PlayerCharacter* a_p, float a_delta)
		{
			if (!g_haul.on) {
				return;
			}
			auto* cc = a_p->GetCharController();
			auto  target = g_haul.target.get();
			auto* actor = target ? target->As<RE::Actor>() : nullptr;
			if (!cc || a_p->IsDead()) {
				g_haul = {};
				return;
			}
			if (!actor || actor->IsDisabled() || !actor->Is3DLoaded()) {
				EndHaul(a_p, "lost its target");
				return;
			}
			const float t = std::chrono::duration<float>(Clock::now() - g_haul.since).count();
			const auto  pos = a_p->GetPosition();
			const auto  to = actor->GetPosition() - pos;
			if (std::sqrt(to.x * to.x + to.y * to.y) <= g_haul.stop) {
				EndHaul(a_p, "arrived");
				return;
			}
			if (t > g_haul.time + 0.3f) {
				EndHaul(a_p, "ran out of time");
				return;
			}
			// a wall or a slope in the way: stop where we are
			if (t > 0.15f && a_delta > 0.0001f) {
				const auto  step = (pos - g_haul.lastPos) / a_delta;
				const float ahead = step.x * g_haul.dir.x + step.y * g_haul.dir.y;
				if (ahead < 0.3f * g_haul.speed) {
					EndHaul(a_p, "blocked");
					return;
				}
			}
			g_haul.lastPos = pos;
			cc->gravity = 0.0f;
			cc->context.currentState = RE::hkpCharacterStateType::kInAir;
			cc->fallStartHeight = pos.z;  // no fall damage from the arc itself
			const float vz = g_haul.vz0 - kHaulGravity * t;
			const float s = RE::bhkWorld::GetWorldScale();
			cc->SetLinearVelocityImpl(RE::hkVector4(g_haul.dir.x * g_haul.speed * s, g_haul.dir.y * g_haul.speed * s, vz * s, 0.0f));
		}

		// a large target: the player is hauled to it along a low arc that ends at its edge
		void StartHaul(RE::PlayerCharacter* a_p, RE::Actor* a_target)
		{
			auto* cc = a_p->GetCharController();
			if (!cc) {
				return;
			}
			const auto  from = a_p->GetPosition();
			const auto  to = a_target->GetPosition();
			const auto  ext = (a_target->GetBoundMax() - a_target->GetBoundMin()) * a_target->GetScale();
			const float edge = 0.5f * std::max(ext.x, ext.y);
			auto        flat = RE::NiPoint3(to.x - from.x, to.y - from.y, 0.0f);
			const float dist = flat.Length();
			const float run = dist - edge - kHaulGap;
			if (dist < 1.0f || run < 50.0f) {
				logger::info("tendril: '{}' is already within reach", a_target->GetName());
				return;
			}
			flat /= dist;
			const float speed = kHaulSpeed * Dial("Protean_TendrilPullMult", 1.0f);
			const float time = run / speed;
			const float dz = std::min(to.z - from.z, 300.0f);  // up a ledge, not onto a dragon's back
			g_haul = { true, Clock::now(), a_target->CreateRefHandle(), flat, speed, dz / time + 0.5f * kHaulGravity * time, time,
				edge + kHaulGap, cc->gravity, from };
			logger::info("tendril: hauling the player {:.0f} units to '{}' ({:.2f} s, {:.0f} up)", run, a_target->GetName(), time, dz);
		}

		// a light target: knocked off its feet toward the player
		// the last one yanked: a landing that kills it is the player's kill (the game credits a fall to no one)
		RE::FormID        g_yankedId = 0;
		Clock::time_point g_yankedAt{};

		void Yank(RE::PlayerCharacter* a_p, RE::Actor* a_target)
		{
			g_yankedId = a_target->GetFormID();
			g_yankedAt = Clock::now();
			auto* proc = a_target->GetActorRuntimeData().currentProcess;
			if (!proc) {
				return;
			}
			const float force = kKnock * Dial("Protean_TendrilPullMult", 1.0f);
			proc->KnockExplosion(a_target, a_p->GetPosition(), -force);
			logger::info("tendril: yanked '{}' (force {:.0f}) from {:.0f} units", a_target->GetName(), force, a_p->GetPosition().GetDistance(a_target->GetPosition()));
		}
	}

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		auto* spell = Registry::Get().Tendril();
		if (!spell || !a_p) {
			return;
		}
		if (g_cooldown > 0.0f) {
			g_cooldown -= a_delta;
		}
		if (g_paid > 0.0f && Clock::now() > g_armed) {
			a_p->AsActorValueOwner()->RestoreActorValue(RE::ActorValue::kStamina, g_paid);
			g_cooldown = 0.0f;  // and no wait after a miss (the user)
			logger::info("tendril: missed, {:.0f} stamina back, no cooldown", g_paid);
			g_paid = 0.0f;
		}
		UpdateHaul(a_p, a_delta);
		Gate(a_p, spell);
		Sleeves(a_p, spell);
		if (g_aimFor > 0.0f) {
			static float sinceAim = 0.0f;
			g_aimFor -= a_delta;
			if ((sinceAim += a_delta) >= 0.25f) {
				sinceAim = 0.0f;
				LogAim(a_p, "ready");
			}
		}
		static float since = 0.0f;
		if ((since += a_delta) < 0.25f) {
			return;
		}
		since = 0.0f;
		SetMagnitudes(a_p, spell);
		static float sinceGive = 0.0f;
		if ((sinceGive += 0.25f) >= 2.0f) {
			sinceGive = 0.0f;
			if (Evolve::Has(kPerk)) {
				Give();
			}
		}
	}

	void OnCast(RE::SpellItem* a_spell)
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (!p || !a_spell || a_spell != Registry::Get().Tendril()) {
			return;
		}
		if (g_aimFor > 0.0f) {
			LogAim(p, "thrown");
		}
		if (!Ready(p)) {
			logger::info("tendril: a throw slipped past the gate (cooldown {:.1f} s): no pull", g_cooldown);
			return;
		}
		RE::BSSoundHandle sound;
		if (auto* audio = RE::BSAudioManager::GetSingleton()) {
			audio->GetSoundHandleByName(sound, g_throwSound.c_str(), 0x1A);
		}
		if (sound.IsValid()) {
			sound.SetPosition(p->GetPosition());
			sound.Play();
		}
		const float cost = Dial("Protean_TendrilCost", 25.0f);
		p->AsActorValueOwner()->DamageActorValue(RE::ActorValue::kStamina, cost);
		g_cooldown = Dial("Protean_TendrilCooldown", 3.0f);
		g_thrownAt = Clock::now();
		// in flight for range / speed, plus a little for the hit to be reported
		const auto* proj = a_spell->effects[0]->baseEffect->data.projectileBase;
		const float flight = proj && proj->data.speed > 0.0f ? proj->data.range / proj->data.speed : 0.5f;
		g_armed = g_thrownAt + std::chrono::milliseconds(static_cast<int>(1000.0f * (flight + 0.3f)));
		g_paid = cost;
		logger::info("tendril: thrown, stamina -{:.0f}, damage {:.0f}, drain {:.0f}", cost, a_spell->effects[0]->effectItem.magnitude,
			a_spell->effects[1]->effectItem.magnitude);
	}

	bool PulledRecently(RE::FormID a_id)
	{
		return a_id != 0 && a_id == g_yankedId && Clock::now() - g_yankedAt < std::chrono::seconds(6);
	}

	void OnHit(RE::Actor* a_target)
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (!p || !a_target || a_target == p || Clock::now() > g_armed) {
			return;
		}
		if (auto* form = RE::TESForm::LookupByID(g_blood)) {
			if (auto* art = form->As<RE::BGSArtObject>()) {
				a_target->ApplyArtObject(art, 1.0f);
			} else if (auto* shader = form->As<RE::TESEffectShader>()) {
				a_target->ApplyEffectShader(shader, 1.5f);
			}
		}
		g_armed = {};  // one pull per throw
		g_paid = 0.0f;  // it hit: no refund
		const char* skip = a_target->IsDead()             ? "dead" :
		                   a_target->IsPlayerTeammate()   ? "a follower" :
		                   a_target->IsEssential()        ? "essential" :
		                   a_target->AsActorState()->IsFlying() ? "flying" :
		                   a_target->AsActorState()->IsSwimming() || p->AsActorState()->IsSwimming() ? "in the water" :
		                   g_haul.on ? "already hauling" : nullptr;
		if (skip) {
			logger::info("tendril: hit '{}', no pull ({})", a_target->GetName(), skip);
			return;
		}
		const auto* race = a_target->GetRace();
		const bool  large = race && race->data.raceSize.get() >= RE::RACE_SIZE::kLarge;
		if (large) {
			StartHaul(p, a_target);
		} else {
			Yank(p, a_target);
		}
	}

	void SetLooks(std::string_view a_what, const std::string& a_value)
	{
		if (a_what == "sound") {
			g_throwSound = a_value;
		} else if (a_what == "blood") {
			g_blood = static_cast<RE::FormID>(std::strtoul(a_value.c_str(), nullptr, 16));
		} else if (a_what == "aim") {
			g_aimFor = a_value.empty() ? 10.0f : std::strtof(a_value.c_str(), nullptr);
			logger::info("tendril: logging the 1st-person aim for {:.0f} s", g_aimFor);
			return;
		}
		logger::info("tendril: throw sound '{}', blood {:08X}", g_throwSound, g_blood);
	}

	void Give()
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (auto* spell = Registry::Get().Tendril(); p && spell && !p->HasSpell(spell)) {
			p->AddSpell(spell);
			logger::info("tendril: spell taught");
		}
	}
}
