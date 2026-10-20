#include "Face.h"

#include "Registry.h"
#include "Shift.h"
#include "State.h"

#include <atomic>
#include <mutex>

namespace protean::Face
{
	namespace
	{
		constexpr auto kIni = "Data/SKSE/Plugins/Protean.ini";
		constexpr int  kOfferFrames = 120;  // how long to wait for the loot window to open before giving up

		bool  g_copyHeadData{ true };    // [Face] CopyHeadData: also share the corpse's head-part / face-data / tint pointers
		bool  g_joinFactions{ true };    // [Face] JoinFactions: take on the corpse's factions (test switch)
		float g_range{ 3000.0f };        // [Face] WitnessRange, game units
		bool  g_enabled{ true };         // [Face] Enabled

		// The choice currently on offer.
		std::mutex             g_lock;
		std::string            g_offerName;
		std::atomic<RE::FormID> g_offerId{ 0 };
		int                    g_wait{ 0 };
		bool                   g_shown{ false };
		std::atomic<bool>      g_queued{ false };
		RE::FormID             g_bypass{ 0 };  // corpse we are opening ourselves for looting: do not offer again

		// What was in the player's base record before we borrowed the corpse's look. These are runtime
		// pointers, so they are not saved; they are handed back before any game load (BeforeLoad), because
		// loading a save can write into those structures.
		struct Borrowed
		{
			bool                         on{ false };
			RE::TESNPC*                  faceNpc{ nullptr };
			RE::BGSHeadPart**            headParts{ nullptr };
			std::int8_t                  numHeadParts{ 0 };
			RE::TESNPC::HeadRelatedData* headRelated{ nullptr };
			RE::TESNPC::FaceData*        faceData{ nullptr };
			decltype(RE::TESNPC::tintLayers) tintLayers{ nullptr };
			RE::TintMask*                skinMask{ nullptr };  // the player's own skin-tone tint, whose colour is swapped
			RE::Color                    skinColor{};
			RE::BGSColorForm*            hairColor{ nullptr };  // the player's own hair colour, swapped in place
			bool                         hairSwapped{ false };
		};
		Borrowed g_orig;

		RE::PlayerCharacter* Player() { return RE::PlayerCharacter::GetSingleton(); }

		void LoadIni()
		{
			std::ifstream in(kIni);
			std::string   line;
			bool          inSection = false;
			while (std::getline(in, line)) {
				if (!line.empty() && line[0] == '[') {
					inSection = line.starts_with("[Face]");
				} else if (inSection && line.starts_with("Enabled=")) {
					g_enabled = line.size() > 8 && line[8] == '1';
				} else if (inSection && line.starts_with("CopyHeadData=")) {
					g_copyHeadData = line.size() > 13 && line[13] == '1';
				} else if (inSection && line.starts_with("JoinFactions=")) {
					g_joinFactions = line.size() > 13 && line[13] == '1';
				} else if (inSection && line.starts_with("WitnessRange=")) {
					try {
						g_range = std::stof(line.substr(13));
					} catch (...) {
					}
				}
			}
		}

		// ---- the look ---------------------------------------------------------------------------------

		std::vector<RE::TintMask*> tints(RE::PlayerCharacter* a_p)
		{
			auto*                      list = a_p->GetTintList();
			std::vector<RE::TintMask*> out;
			if (list) {
				out.assign(list->begin(), list->end());
			}
			return out;
		}

		// The skin-tone entry of an NPC's tint layers (the one that colours arms, legs and body).
		RE::TESNPC::Layer* SkinLayer(RE::TESNPC* a_npc, RE::TESRace* a_race, bool a_female)
		{
			if (!a_npc || !a_race || !a_npc->tintLayers) {
				return nullptr;
			}
			auto* related = a_race->faceRelatedData[a_female ? 1 : 0];
			if (!related || !related->tintMasks) {
				return nullptr;
			}
			for (auto* asset : *related->tintMasks) {
				if (asset && asset->texture.skinTone.get() == RE::TESRace::FaceRelatedData::TintAsset::TintLayer::SkinTone::kSkinTone) {
					for (auto* layer : *a_npc->tintLayers) {
						if (layer && layer->tintIndex == asset->texture.index) {
							return layer;
						}
					}
				}
			}
			return nullptr;
		}

		void Capture(RE::TESNPC* a_base)
		{
			if (g_orig.on) {
				return;
			}
			g_orig.on = true;
			g_orig.faceNpc = a_base->faceNPC;
			g_orig.headParts = a_base->headParts;
			g_orig.numHeadParts = a_base->numHeadParts;
			g_orig.headRelated = a_base->headRelatedData;
			g_orig.faceData = a_base->faceData;
			g_orig.tintLayers = a_base->tintLayers;
		}

		// Put the corpse's look on the player's base record. Only memory is changed: nothing is marked as
		// changed, so a save keeps the player's own face. The stolen one is re-applied after a load.
		void ApplyLook(RE::TESNPC* a_src)
		{
			auto* base = Player()->GetActorBase();
			auto* root = a_src->GetRootFaceNPC();
			if (!base || !root) {
				return;
			}
			Capture(base);
			{
				// the body and hair take the colours of the new face, not the player's own
				auto* p = Player();
				RE::TintMask* mine = nullptr;
				for (auto* t : tints(p)) {
					if (t && t->type.get() == RE::TintMask::Type::kSkinTone) {
						mine = t;
						break;
					}
				}
				auto* theirs = SkinLayer(root, root->GetRace(), root->actorData.actorBaseFlags.all(RE::ACTOR_BASE_DATA::Flag::kFemale));
				const RE::Color skin = theirs ? theirs->tintColor : root->bodyTintColor;
				if (mine) {
					if (!g_orig.skinMask) {
						g_orig.skinMask = mine;
						g_orig.skinColor = mine->color;
					}
					logger::info("skin tone {},{},{} -> {},{},{} ({})", mine->color.red, mine->color.green, mine->color.blue, skin.red, skin.green,
						skin.blue, theirs ? "tint layer" : "body colour");
					mine->color = skin;
				} else {
					logger::info("skin tone: player has no skin-tone tint");
				}
				if (g_orig.headRelated && root->headRelatedData && root->headRelatedData->hairColor) {
					if (!g_orig.hairSwapped) {
						g_orig.hairColor = g_orig.headRelated->hairColor;
						g_orig.hairSwapped = true;
					}
					g_orig.headRelated->hairColor = root->headRelatedData->hairColor;
					logger::info("hair colour -> {}", root->headRelatedData->hairColor->GetFormEditorID());
				}
			}
			base->faceNPC = root == base ? nullptr : root;
			if (a_src->actorData.actorBaseFlags.all(RE::ACTOR_BASE_DATA::Flag::kFemale)) {
				base->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kFemale);
			} else {
				base->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kFemale);
			}
			base->height = root->height;
			base->weight = root->weight;
			base->bodyTintColor = root->bodyTintColor;
			if (g_copyHeadData) {
				base->headParts = root->headParts;
				base->numHeadParts = root->numHeadParts;
				base->headRelatedData = root->headRelatedData;
				base->faceData = root->faceData;
				base->tintLayers = root->tintLayers;
			}
		}

		void RestoreLook()
		{
			auto* base = Player()->GetActorBase();
			auto& s = State::Get();
			if (!base) {
				return;
			}
			if (g_orig.on) {
				base->faceNPC = g_orig.faceNpc;
				base->headParts = g_orig.headParts;
				base->numHeadParts = g_orig.numHeadParts;
				base->headRelatedData = g_orig.headRelated;
				base->faceData = g_orig.faceData;
				base->tintLayers = g_orig.tintLayers;
				if (g_orig.skinMask) {
					for (auto* t : tints(Player())) {
						if (t == g_orig.skinMask) {  // only if the game has not rebuilt the list since
							t->color = g_orig.skinColor;
						}
					}
					g_orig.skinMask = nullptr;
				}
				if (g_orig.hairSwapped && g_orig.headRelated) {
					g_orig.headRelated->hairColor = g_orig.hairColor;
					g_orig.hairSwapped = false;
				}
				g_orig.on = false;
			}
			if (s.ownFemale) {
				base->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kFemale);
			} else {
				base->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kFemale);
			}
			base->height = s.ownHeight;
			base->weight = s.ownWeight;
			base->bodyTintColor.red = s.ownTint[0];
			base->bodyTintColor.green = s.ownTint[1];
			base->bodyTintColor.blue = s.ownTint[2];
		}

		void RefreshModel()
		{
			SKSE::GetTaskInterface()->AddTask([]() {
				if (auto* p = Player(); p && p->Is3DLoaded()) {
					p->DoReset3D(true);
					p->UpdateSkinColor();
					p->UpdateHairColor();
				}
			});
		}

		void SnapshotOwn(RE::TESNPC* a_base)
		{
			auto* p = Player();
			auto& s = State::Get();
			s.ownRace = p->GetRace() ? p->GetRace()->GetFormID() : 0;
			s.ownFemale = a_base->actorData.actorBaseFlags.all(RE::ACTOR_BASE_DATA::Flag::kFemale);
			s.ownHeight = a_base->height;
			s.ownWeight = a_base->weight;
			s.ownTint[0] = a_base->bodyTintColor.red;
			s.ownTint[1] = a_base->bodyTintColor.green;
			s.ownTint[2] = a_base->bodyTintColor.blue;
			s.ownFaceNpc = a_base->faceNPC ? a_base->faceNPC->GetFormID() : 0;
		}

		// ---- factions and witnesses ------------------------------------------------------------------

		void JoinFactions(RE::TESNPC* a_src)
		{
			auto* p = Player();
			auto& s = State::Get();
			auto& reg = Registry::Get();
			auto  join = [&](RE::TESFaction* f) {
				if (f && !p->IsInFaction(f)) {
					p->AddToFaction(f, 0);
					s.disguiseFactions.push_back(f->GetFormID());
				}
			};
			for (const auto& fr : a_src->factions) {
				if (fr.faction && fr.rank >= 0) {
					join(fr.faction);
				}
			}
			join(reg.DisguiseFaction());
		}

		void LeaveFactions()
		{
			auto* p = Player();
			auto& s = State::Get();
			for (auto id : s.disguiseFactions) {
				if (auto* f = RE::TESForm::LookupByID<RE::TESFaction>(id)) {
					p->RemoveFromFaction(f);
				}
			}
			s.disguiseFactions.clear();
			if (auto* ex = Registry::Get().ExposedFaction()) {
				for (auto id : s.exposed) {
					if (auto* a = RE::TESForm::LookupByID<RE::Actor>(id)) {
						a->RemoveFromFaction(ex);
					}
				}
			}
			s.exposed.clear();
		}

		// Seeing someone take a dead person's face is treated like seeing a vampire feed: whoever could see
		// it (alive, not on the player's side) turns on the player, and stays hostile while the face is worn.
		void Expose(RE::Actor* a_corpse)
		{
			auto* p = Player();
			auto& s = State::Get();
			auto* ex = Registry::Get().ExposedFaction();
			auto* pl = RE::ProcessLists::GetSingleton();
			if (!pl) {
				return;
			}
			const auto here = p->GetPosition();
			int        seen = 0;
			for (auto& handle : pl->highActorHandles) {
				auto actor = handle.get();
				if (!actor || actor.get() == p || actor.get() == a_corpse || actor->IsDead() || actor->IsPlayerTeammate()) {
					continue;
				}
				// only people count: a horse or a dog watching is not a witness
				if (auto* r = actor->GetRace(); !r || !r->data.flags.all(RE::RACE_DATA::Flag::kFaceGenHead)) {
					continue;
				}
				if (here.GetDistance(actor->GetPosition()) > g_range || actor->RequestDetectionLevel(p) <= 0) {
					continue;
				}
				++seen;
				if (ex) {
					actor->AddToFaction(ex, 0);
					s.exposed.push_back(actor->GetFormID());
				}
				actor->StartCombat(p);
				logger::info("  {:08X} '{}' saw the theft", actor->GetFormID(), actor->GetName());
			}
			logger::info("face theft witnessed by {} actor(s)", seen);
			if (seen > 0) {
				Shift::Notify("You were seen!");
			}
		}

		bool Eligible(RE::Actor* a_corpse)
		{
			if (!a_corpse || !a_corpse->IsDead() || a_corpse->IsPlayerRef()) {
				return false;
			}
			auto* npc = a_corpse->GetActorBase();
			auto* race = a_corpse->GetRace();
			if (!npc || !race || !race->data.flags.all(RE::RACE_DATA::Flag::kFaceGenHead) || race->IsChildRace()) {
				return false;
			}
			if (Registry::Get().ByRace(race)) {
				return false;  // an animal: that is a heart, not a face
			}
			return (npc->GetFormID() >> 24) != 0xFF;  // a generated NPC could not be found again after a load
		}

		void ShowChoice();

		// The game is paused while the loot window is open, so the per-frame hook never runs then: watch for the
		// menu opening instead.
		class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			static MenuSink* Get()
			{
				static MenuSink m;
				return &m;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && a_event->opening && a_event->menuName == RE::ContainerMenu::MENU_NAME && g_offerId && !g_shown && !g_queued.exchange(true)) {
					SKSE::GetTaskInterface()->AddTask([]() { ShowChoice(); });
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// ---- the choice window (render thread) ----------------------------------------------------------

		void CloseOffer()
		{
			g_offerId = 0;
			g_wait = 0;
			g_shown = false;
			g_queued = false;
		}

		// The game's own message box: Loot / Copy appearance / Cancel.
		class Choice : public RE::IMessageBoxCallback
		{
		public:
			explicit Choice(RE::FormID a_id) :
				_id(a_id) {}

			void Run(std::uint8_t a_button) override
			{
				const auto id = _id;
				SKSE::GetTaskInterface()->AddTask([id, a_button]() {
					if (auto* a = RE::TESForm::LookupByID<RE::Actor>(id)) {
						if (a_button == 0) {
							g_bypass = id;
							a->ActivateRef(Player(), 0, nullptr, 1, false);
						} else if (a_button == 1) {
							Steal(a->GetActorBase(), a, true);
						}
					}
					CloseOffer();
				});
			}

		private:
			RE::FormID _id;
		};

		void ShowChoice()
		{
			// the game opened the loot window; close it and ask instead
			RE::UIMessageQueue::GetSingleton()->AddMessage(RE::ContainerMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
			const auto id = g_offerId.load();
			SKSE::GetTaskInterface()->AddTask([id]() {
				std::string name;
				{
					std::scoped_lock lk(g_lock);
					name = g_offerName;
				}
				auto* box = RE::UIMessageDataFactory::Create<RE::MessageBoxData>();
				if (!box) {
					logger::error("face offer: could not create the message box");
					CloseOffer();
					return;
				}
				box->bodyText = name.c_str();
				box->buttonText.push_back(RE::BSString("Loot"));
				box->buttonText.push_back(RE::BSString("Copy appearance"));
				box->buttonText.push_back(RE::BSString("Cancel"));
				box->cancelButtonIndex = 2;
				box->isCancellable = true;
				box->callback = RE::BSTSmartPointer<RE::IMessageBoxCallback>(new Choice(id));
				RE::MessageBoxMenu::QueueMessage(box);
				g_shown = true;
				g_queued = false;
				logger::info("face offer: loot window replaced by the choice box");
			});
		}
	}

	// ---- public --------------------------------------------------------------------------------------

	void Register() {}  // the choice is the game's own message box: nothing to register

	void Install()
	{
		LoadIni();
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuSink::Get());
		}
		logger::info("face stealing {} (CopyHeadData {}, WitnessRange {:.0f})", g_enabled ? "on" : "off", g_copyHeadData, g_range);
	}

	bool Disguised()
	{
		return State::Get().disguised;
	}

	bool OnCorpseActivated(RE::Actor* a_corpse)
	{
		auto* p = Player();
		auto& s = State::Get();
		if (!g_enabled || !a_corpse || !p) {
			return false;
		}
		if (g_bypass == a_corpse->GetFormID()) {
			g_bypass = 0;  // our own loot request coming back through the activation event
			return false;
		}
		if (!s.active.empty() || !p->HasSpell(Registry::Get().Starter()) || !p->IsSneaking() || !Eligible(a_corpse)) {
			return false;
		}
		{
			std::scoped_lock lk(g_lock);
			g_offerName = std::format("Body of {}", a_corpse->GetDisplayFullName());
		}
		g_offerId = a_corpse->GetFormID();
		g_wait = kOfferFrames;
		g_shown = false;
		if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) && !g_queued.exchange(true)) {
			ShowChoice();  // the loot window beat us to it
		}
		logger::info("face offer started for {:08X} '{}'", a_corpse->GetFormID(), a_corpse->GetName());
		return true;
	}

	void Steal(RE::TESNPC* a_source, RE::Actor* a_corpse, bool a_witnessed)
	{
		auto* p = Player();
		auto& s = State::Get();
		auto* base = p->GetActorBase();
		if (!a_source || !base || !a_source->GetRace()) {
			return;
		}
		if (!s.active.empty()) {
			Shift::Notify("You must return to your own form first.");
			return;
		}
		logger::info("=== steal face of {:08X} '{}' (race '{}', witnessed check {}) ===", a_source->GetFormID(), a_source->GetName(),
			a_source->GetRace()->GetName(), a_witnessed);
		if (s.disguised) {
			// swapping one stolen face for another: keep the player's own look from the first theft
			LeaveFactions();
		} else {
			SnapshotOwn(base);
		}
		if (a_witnessed) {
			Expose(a_corpse);
		}
		auto* race = a_source->GetRace();
		ApplyLook(a_source);
		if (g_joinFactions) {
			JoinFactions(a_source);
		}
		if (auto* fp = Registry::Get().FacePower(); fp && !p->HasSpell(fp)) {
			p->AddSpell(fp);
		}
		s.bankedNpc = a_source->GetFormID();
		s.disguised = true;
		p->SwitchRace(race, true);
		RefreshModel();
		Shift::Notify(std::format("You have taken the face of {}.", a_source->GetName()));
		Dump();
	}

	void TakeOff(std::string_view a_why)
	{
		auto* p = Player();
		auto& s = State::Get();
		if (!s.disguised) {
			return;
		}
		logger::info("=== take off the stolen face ({}) ===", a_why);
		LeaveFactions();
		RestoreLook();
		const auto* own = RE::TESForm::LookupByID<RE::TESRace>(s.ownRace);
		if (own) {
			p->SwitchRace(const_cast<RE::TESRace*>(own), true);
		} else {
			logger::error("own race {:08X} not found; staying in the stolen face's race", s.ownRace);
		}
		RefreshModel();
		s.disguised = false;
		Shift::Notify("You wear your own face again.");
		Dump();
	}

	void OnPower()
	{
		auto& s = State::Get();
		if (!s.active.empty()) {
			Shift::Notify("Return to your own form first.");
			return;
		}
		if (s.disguised) {
			TakeOff("power");
			return;
		}
		auto* npc = s.bankedNpc ? RE::TESForm::LookupByID<RE::TESNPC>(s.bankedNpc) : nullptr;
		if (!npc) {
			Shift::Notify(s.bankedNpc ? "The face you kept can no longer be found." : "You have no face kept. Sneak next to a body and activate it.");
			return;
		}
		Steal(npc, nullptr, false);
	}

	void OnFrame()
	{
		const auto id = g_offerId.load();
		if (!id) {
			return;
		}
		auto* p = Player();
		auto* corpse = RE::TESForm::LookupByID<RE::Actor>(id);
		if (!corpse || !p || p->IsDead() || !State::Get().active.empty()) {
			CloseOffer();
			return;
		}
		if (!g_shown) {
			if (--g_wait <= 0) {
				logger::info("face offer: the loot window never opened; dropping the offer");
				CloseOffer();
			}
			return;
		}
		// the box is open: the callback ends the offer
	}

	void OnGameLoaded()
	{
		auto& s = State::Get();
		g_orig = Borrowed{};
		if (!s.disguised) {
			return;
		}
		auto* npc = s.bankedNpc ? RE::TESForm::LookupByID<RE::TESNPC>(s.bankedNpc) : nullptr;
		if (!npc) {
			logger::warn("loaded a save made in a stolen face whose source is gone; putting the own face back");
			s.disguised = false;
			return;
		}
		logger::info("loaded a save made in the face of '{}': re-applying", npc->GetName());
		ApplyLook(npc);
		RefreshModel();
	}

	void BeforeLoad()
	{
		CloseOffer();
		if (g_orig.on) {
			logger::info("handing the player's own face data back before loading");
			RestoreLook();
		}
	}

	void Dump()
	{
		auto* p = Player();
		auto& s = State::Get();
		auto* base = p->GetActorBase();
		if (!base) {
			return;
		}
		{
			std::string parts;
			for (int i = 0; i < base->numHeadParts && base->headParts; ++i) {
				if (auto* hp = base->headParts[i]) {
					parts += std::format(" {}({:08X})", hp->GetFormEditorID(), hp->GetFormID());
				}
			}
			logger::info("face parts:{} tint layers {}", parts, base->tintLayers ? base->tintLayers->size() : 0);
		}
		logger::info("face: disguised {} bank {:08X} race '{}' female {} height {:.3f} weight {:.1f} faceNPC '{}' headParts {} faction(s) {} exposed {}",
			s.disguised, s.bankedNpc, p->GetRace() ? p->GetRace()->GetName() : "?",
			base->actorData.actorBaseFlags.all(RE::ACTOR_BASE_DATA::Flag::kFemale), base->height, base->weight,
			base->faceNPC ? base->faceNPC->GetName() : "-", static_cast<int>(base->numHeadParts), s.disguiseFactions.size(), s.exposed.size());
	}
}
