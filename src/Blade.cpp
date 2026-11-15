#include "Blade.h"

#include "Evolve.h"
#include "Registry.h"

#undef GetObject  // windows.h renames it to GetObjectA, which hides BGSDefaultObjectManager::GetObject

namespace protean::Blade
{
	// draw: a kill move's blade going in; sheathe: pulled back out (first picks, 2026-11-14, to be chosen by ear)
	std::string g_growSound{ "NPCKillStabIn" };
	std::string g_backSound{ "NPCKillStabOut" };

	void SleeveSound(RE::PlayerCharacter* a_p, bool a_grow, const char* a_tag)
	{
		const auto& name = a_grow ? g_growSound : g_backSound;
		static std::chrono::steady_clock::time_point last[2]{};
		const auto now = std::chrono::steady_clock::now();
		if (name.empty() || now - last[a_grow] < std::chrono::milliseconds(150)) {  // the other hand, the same moment
			return;
		}
		last[a_grow] = now;
		RE::BSSoundHandle sound;
		if (auto* audio = RE::BSAudioManager::GetSingleton()) {
			audio->GetSoundHandleByName(sound, name.c_str(), 0x1A);
		}
		if (!sound.IsValid()) {
			logger::warn("{}: no sound '{}'", a_tag, name);
			return;
		}
		sound.SetPosition(a_p->GetPosition());
		sound.Play();
	}

	namespace
	{
		constexpr float kGrowFrom = 0.15f;  // the starting size, as a share of full size about each bone
		struct Growing
		{
			RE::NiPointer<RE::NiSkinData> data;
			std::vector<RE::NiTransform>  full;  // each bone's skin-to-bone transform at full size
			float                         t{ 0.0f };
			float                         seconds{ 0.3f };
		};
		std::vector<Growing> g_growing;

		// each bone's piece drawn at a_s of its size about the bone: v_bone = s * (R v + t)
		void SetSize(Growing& a_g, float a_s)
		{
			for (std::uint32_t i = 0; i < a_g.full.size(); ++i) {
				auto&       x = a_g.data->GetBoneDataSkinToBone(i);
				const auto& f = a_g.full[i];
				x.rotate = f.rotate;
				x.scale = f.scale * a_s;
				x.translate = f.translate * a_s;
			}
		}
	}

	void GrowAll(RE::PlayerCharacter* a_p, float a_seconds)
	{
		int pieces = 0;
		for (const bool first : { false, true }) {
			auto* root = a_p->Get3D(first);
			if (!root) {
				continue;
			}
			RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* a_geo) {
				if (!a_geo->name.c_str() || std::string_view(a_geo->name.c_str()).rfind("Protean", 0) != 0) {
					return RE::BSVisit::BSVisitControl::kContinue;
				}
				auto* skin = a_geo->GetGeometryRuntimeData().skinInstance.get();
				auto* data = skin ? skin->skinData.get() : nullptr;
				if (!data || std::any_of(g_growing.begin(), g_growing.end(), [&](const Growing& g) { return g.data.get() == data; })) {
					return RE::BSVisit::BSVisitControl::kContinue;
				}
				Growing g{ RE::NiPointer<RE::NiSkinData>(data), {}, 0.0f, std::max(0.05f, a_seconds) };
				for (std::uint32_t i = 0; i < data->GetBoneCount(); ++i) {
					g.full.push_back(data->GetBoneDataSkinToBone(i));
				}
				SetSize(g, kGrowFrom);
				g_growing.push_back(std::move(g));
				++pieces;
				return RE::BSVisit::BSVisitControl::kContinue;
			});
		}
		logger::info("grow: {} piece(s) over {:.2f} s", pieces, a_seconds);
	}

	void UpdateGrow(float a_delta)
	{
		for (auto it = g_growing.begin(); it != g_growing.end();) {
			it->t += a_delta / it->seconds;
			if (it->t >= 1.0f) {
				SetSize(*it, 1.0f);  // exactly the model's own transforms again
				it = g_growing.erase(it);
				continue;
			}
			const float e = 1.0f - (1.0f - it->t) * (1.0f - it->t) * (1.0f - it->t);  // fast, then settling
			SetSize(*it, kGrowFrom + (1.0f - kGrowFrom) * e);
			++it;
		}
	}

	void SetSleeveSound(bool a_grow, const std::string& a_sound)
	{
		(a_grow ? g_growSound : g_backSound) = a_sound == "none" ? std::string{} : a_sound;
		logger::info("sleeves: draw sound '{}', sheathe sound '{}'", g_growSound, g_backSound);
	}

	// The game draws no body slot 57 in 1st person (seen 2026-11-04). There the plugin hangs a model (Sleeve1st_M/F.nif)
	// in the 1st-person body itself and binds its skinned pieces to the 1st-person skeleton's bones of the same names,
	// as the game does for worn armor, so the flesh bends with the wrist. Shared with the Shield (2026-11-05).
	void HangFirstPerson(RE::PlayerCharacter* a_p, const char* kFirstName, std::string_view kFirstModel, bool a_on, const char* a_tag)
	{
		auto* root3d = a_p->Get3D(true);
		auto* root = root3d ? root3d->AsNode() : nullptr;
		if (!root) {
			return;
		}
		auto* have = root->GetObjectByName(kFirstName);
		if (!a_on) {
			if (have && have->parent) {
				have->parent->DetachChild2(have);
				logger::info("{}: 1st person goes back", a_tag);
			}
			return;
		}
		if (have) {
			return;
		}
		const auto* base = a_p->GetActorBase();
		const auto  path = fmt::format(fmt::runtime(kFirstModel), base && base->GetSex() == RE::SEX::kFemale ? 'F' : 'M');
		RE::NiPointer<RE::NiNode> model;
		if ((RE::BSModelDB::Demand(path.c_str(), model, {}) != RE::BSResource::ErrorCode::kNone || !model) &&
			(RE::BSModelDB::Demand(("Meshes\\" + path).c_str(), model, {}) != RE::BSResource::ErrorCode::kNone || !model)) {
			static std::set<std::string> warned;  // once per model
			if (warned.insert(path).second) {
				logger::warn("{}: could not load {}", a_tag, path);
			}
			return;
		}
		// the loaded model is shared: hang a copy
		RE::NiPointer<RE::NiObject> clone(model->Clone());
		auto*                       copy = clone ? clone->AsNode() : nullptr;
		if (!copy) {
			return;
		}
		copy->name = kFirstName;
		int bound = 0, missing = 0;
		RE::BSVisit::TraverseScenegraphGeometries(copy, [&](RE::BSGeometry* a_geo) {
			auto* skin = a_geo->GetGeometryRuntimeData().skinInstance.get();
			const auto n = skin && skin->skinData ? skin->skinData->bones : 0u;
			for (std::uint32_t i = 0; i < n; ++i) {
				auto* own = skin->bones[i];
				auto* real = own ? root->GetObjectByName(own->name) : nullptr;
				if (!real) {
					logger::info("{}: 1st person has no bone '{}': it follows the body instead", a_tag, own ? own->name.c_str() : "?");
					real = root;
					++missing;
				}
				skin->bones[i] = real;
				skin->boneWorldTransforms[i] = &real->world;
				++bound;
			}
			if (skin) {
				skin->rootParent = root;
			}
			// its bounds were measured where the 3rd-person arm is: never cull it for being "off screen"
			a_geo->GetFlags().set(RE::NiAVObject::Flag::kAlwaysDraw);
			return RE::BSVisit::BSVisitControl::kContinue;
		});
		root->AttachChild(copy, true);
		RE::NiUpdateData update;
		copy->Update(update);
		logger::info("{}: 1st person grows ({} bones bound, {} missing)", a_tag, bound, missing);
	}

	namespace
	{
		constexpr auto kPerk = "blade";

		bool  g_had{ false };   // the Blade was in the right hand at the last look
		float g_wait{ 0.0f };   // after a change, give the game time to carry it out before looking again

		std::int32_t Count(RE::PlayerCharacter* a_p, RE::TESBoundObject* a_item)
		{
			const auto counts = a_p->GetInventoryCounts([a_item](RE::TESBoundObject& a_obj) { return &a_obj == a_item; });
			const auto it = counts.find(a_item);
			return it != counts.end() ? it->second : 0;
		}

		// the game's Actor.UnequipSpell (source 0: the left hand); CommonLib has no direct call for it
		void UnequipLeftSpell(RE::PlayerCharacter* a_p, RE::SpellItem* a_spell)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm) {
				return;
			}
			const auto handle = vm->GetObjectHandlePolicy()->GetHandleForObject(a_p->GetFormType(), a_p);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
			std::int32_t source = 0;
			vm->DispatchMethodCall2(handle, "Actor", "UnequipSpell", RE::MakeFunctionArguments(std::move(a_spell), std::move(source)), cb);
		}

		void ClearLeft(RE::PlayerCharacter* a_p, RE::TESForm* a_left, RE::TESObjectARMO* a_shield)
		{
			auto* mgr = RE::ActorEquipManager::GetSingleton();
			if (a_shield) {
				mgr->UnequipObject(a_p, a_shield);
			}
			if (a_left) {
				if (auto* spell = a_left->As<RE::SpellItem>()) {
					UnequipLeftSpell(a_p, spell);
				} else if (auto* obj = a_left->As<RE::TESBoundObject>()) {
					auto* dom = RE::BGSDefaultObjectManager::GetSingleton();
					auto* slot = dom ? dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECTS::kLeftHandEquip) : nullptr;
					mgr->UnequipObject(a_p, obj, nullptr, 1, slot);
				}
			}
		}

		// like a greatsword, the Blade keeps the left hand empty (checked a few times a second)
		void KeepLeftEmpty(RE::PlayerCharacter* a_p, RE::TESObjectWEAP* a_blade, float a_step)
		{
			if (g_wait > 0.0f) {
				g_wait -= a_step;
				return;
			}
			const bool has = a_p->GetEquippedObject(false) == a_blade;
			auto*      left = a_p->GetEquippedObject(true);
			auto*      shield = a_p->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kShield);
			if (has && (left || shield)) {
				const char* what = shield ? shield->GetName() : left->GetName();
				if (!g_had) {
					// the Blade was just taken up: it needs both hands
					logger::info("blade: taken up, the left hand lets go of '{}'", what);
					ClearLeft(a_p, left, shield);
				} else {
					// something went into the left hand: the Blade goes away, as a greatsword would
					logger::info("blade: '{}' into the left hand, the Blade goes away", what);
					RE::ActorEquipManager::GetSingleton()->UnequipObject(a_p, a_blade);
					g_had = false;
					g_wait = 0.5f;
					return;
				}
				g_wait = 0.5f;
			}
			g_had = has;
		}

		constexpr auto kFirstName = "ProteanBlade1st";
		constexpr auto kFirstModel = "Protean\\Blade\\Sleeve1st_{}.nif";  // M or F (the female arm, 2026-11-04)

		void FirstPerson(RE::PlayerCharacter* a_p, bool a_on) { HangFirstPerson(a_p, kFirstName, kFirstModel, a_on, "blade"); }

		// the flesh and the bone blade are on the arm only while the Blade is drawn; never kept in the inventory
		void Sleeve(RE::PlayerCharacter* a_p, RE::TESObjectWEAP* a_blade, RE::TESObjectARMO* a_sleeve)
		{
			using WS = RE::WEAPON_STATE;
			const auto* st = a_p->AsActorState();
			const auto  ws = st ? st->GetWeaponState() : WS::kSheathed;
			// each step of a draw or sheathe in the log (timing a slow draw, 2026-11-04)
			static auto lastWs = ws;
			if (ws != lastWs && a_p->GetEquippedObject(false) == a_blade) {
				logger::info("blade: weapon state {} -> {}", static_cast<int>(lastWs), static_cast<int>(ws));
			}
			lastWs = ws;
			// only while fully drawn: both the draw and the sheathe reach across the body to the hip as if to a
			// scabbard, which looked odd with the blade on the arm (user, 2026-11-04, 1st person); it grows once the
			// draw is done and goes back the moment the sheathe starts
			const bool want = a_p->GetEquippedObject(false) == a_blade && ws == WS::kDrawn;
			FirstPerson(a_p, want);
			const bool worn = a_p->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kModShoulder) == a_sleeve;
			auto*      mgr = RE::ActorEquipManager::GetSingleton();
			if (want && !worn) {
				if (Count(a_p, a_sleeve) == 0) {
					a_p->AddObjectToContainer(a_sleeve, nullptr, 1, nullptr);
				}
				mgr->EquipObject(a_p, a_sleeve, nullptr, 1, nullptr, false, false, false, true);
				logger::info("blade: grows (weapon state {})", static_cast<int>(ws));
				SleeveSound(a_p, true, "blade");
			} else if (!want) {
				if (worn) {
					mgr->UnequipObject(a_p, a_sleeve, nullptr, 1, nullptr, false, true, false, true);
					logger::info("blade: goes back (weapon state {})", static_cast<int>(ws));
					SleeveSound(a_p, false, "blade");
				}
				if (const auto n = Count(a_p, a_sleeve); n > 0) {
					a_p->RemoveItem(a_sleeve, n, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
				}
			}
		}

		// a greatsword's lead over a sword on top of the claws' full-bar damage at the player's level (Claws.cpp); always
		// full strength, never weaker on a low stamina bar (user, 2026-11-04); Two-Handed skill and perks multiply it
		void SetDamage(RE::PlayerCharacter* a_p, RE::TESObjectWEAP* a_blade)
		{
			const auto& reg = Registry::Get();
			const float level = static_cast<float>(a_p->GetLevel());
			const float claws = reg.Dial("Protean_ClawDamageMax", 10.0f) + reg.Dial("Protean_ClawDamagePerLevel", 0.7f) * level +
			                    reg.Dial("Protean_ClawDamagePerLevelSq", 0.02f) * level * level;
			a_blade->attackDamage = static_cast<std::uint16_t>(
				std::clamp(std::lround(claws * reg.Dial("Protean_BladeDamageMult", 1.7f)), 0L, 65535L));
		}

		// every swing costs Stamina
		class AnimSink : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
			{
				auto* p = RE::PlayerCharacter::GetSingleton();
				auto* blade = Registry::Get().BladeWeapon();
				if (!a_event || !p || !blade || a_event->holder != p || p->GetEquippedObject(false) != blade) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const std::string_view tag(a_event->tag.c_str());
				if (tag == "weaponSwing") {
					const float cost = Registry::Get().Dial("Protean_BladeSwingCost", 8.0f);
					if (cost > 0.0f) {
						p->AsActorValueOwner()->DamageActorValue(RE::ActorValue::kStamina, cost);
					}
					logger::info("blade: swing, stamina -{:.0f}, damage {}", cost, blade->attackDamage);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		AnimSink g_anim;
	}

	void OnFrame(float a_delta)
	{
		auto*       p = RE::PlayerCharacter::GetSingleton();
		const auto& reg = Registry::Get();
		auto*       blade = reg.BladeWeapon();
		auto*       sleeve = reg.BladeSleeve();
		if (!p || !blade || !sleeve) {
			return;
		}
		// the player's animation graphs are rebuilt on a race change or a load: listen again whenever the Blade is
		// taken up (adding twice is harmless: it is taken off first)
		static bool held = false;
		const bool  has = p->GetEquippedObject(false) == blade;
		if (has && !held) {
			p->RemoveAnimationGraphEventSink(&g_anim);
			p->AddAnimationGraphEventSink(&g_anim);
		}
		held = has;
		Sleeve(p, blade, sleeve);

		static float since = 0.0f;
		if ((since += a_delta) < 0.1f) {
			return;
		}
		const float step = since;
		since = 0.0f;
		KeepLeftEmpty(p, blade, step);
		SetDamage(p, blade);
		// the perk keeps the Blade in the inventory
		static float sinceGive = 0.0f;
		if ((sinceGive += step) >= 2.0f) {
			sinceGive = 0.0f;
			if (Evolve::Has(kPerk) && Count(p, blade) == 0) {
				p->AddObjectToContainer(blade, nullptr, 1, nullptr);
				logger::info("blade: the perk gives the Blade");
			}
		}
	}
}
