#include "Face.h"

#include "Evolve.h"

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

		bool  g_menuTakeOff{ false };     // [Face] TakeOffOnMenu: the game menu (save, load, quit) takes a stolen face off
		bool  g_joinFactions{ true };    // [Face] JoinFactions: take on the corpse's factions (test switch)
		float g_range{ 3000.0f };        // [Face] WitnessRange, game units
		bool  g_enabled{ true };         // [Face] Enabled

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
				} else if (inSection && line.starts_with("TakeOffOnMenu=")) {
					g_menuTakeOff = line.size() > 14 && line[14] == '1';
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
		//
		// The player's head is always built by the game from the player's own record: head parts, face sliders,
		// hair colour and the colour layers ("tint masks") it paints into the face texture. A stolen face is put on
		// by writing the corpse's values into those, then asking the game to redraw (the same calls RaceMenu uses).
		// NPCs that ship finished face files (most named NPCs) use those files instead, which are exact.

		using Look = State::Look;
		using Tint = State::Tint;

		constexpr std::uint32_t kTintTypes = 15;    // RE::TintMask::Type::kTotal
		constexpr int           kRedrawDelay = 90;  // frames over which a newly worn look is redrawn
		constexpr int           kHeadAt = 82;       // RegenerateHead
		constexpr int           kPaintAt[] = { 74, 40, 1 };  // paint the colours (again, in case the model finished late)

		std::vector<Tint> g_pendingTints;  // painted with the redraw, after any race change has rebuilt the model
		int               g_redraw{ 0 };   // frames until the redraw (0 = none)

		RE::FormID Id(const RE::TESForm* a_form) { return a_form ? a_form->GetFormID() : 0; }

		bool Generated(RE::FormID a_id) { return (a_id >> 24) == 0xFF; }

		std::string Lower(const char* a_s)
		{
			std::string s = a_s ? a_s : "";
			std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		// The player's tint mask of a type, asked of the game itself and checked before use.
		RE::TintMask* Mask(RE::PlayerCharacter* a_p, std::uint32_t a_type, std::uint32_t a_index)
		{
			RE::TintMask* m = nullptr;
			__try {
				m = a_p->GetTintMask(a_type, a_index);
				if (reinterpret_cast<std::uintptr_t>(m) < 0x10000 || static_cast<std::uint32_t>(m->type.get()) != a_type) {
					m = nullptr;
				}
			} __except (1) {
				m = nullptr;
			}
			return m;
		}

		std::uint32_t MaskCount(RE::PlayerCharacter* a_p, std::uint32_t a_type)
		{
			std::uint32_t n = 0;
			__try {
				n = a_p->GetNumTints(a_type);
			} __except (1) {
				n = 0;
			}
			return n > 64 ? 0 : n;
		}

		std::string MaskTexture(const RE::TintMask* a_m)
		{
			return a_m->texture ? Lower(a_m->texture->textureName.c_str()) : std::string{};
		}

		bool g_ignoreFaceFiles{ false };  // dev: build every face from the data, to test that path

		// Does the game ship finished face files (head model and face texture) for this NPC?
		bool HasFaceFiles(const RE::TESNPC* a_npc)
		{
			if (!a_npc || Generated(a_npc->GetFormID())) {
				return false;
			}
			const auto* file = a_npc->GetFile(0);
			if (!file) {
				return false;
			}
			const bool light = file->recordFlags.all(RE::TESFile::RecordFlag::kSmallFile);
			const auto local = a_npc->GetFormID() & (light ? 0xFFFu : 0xFFFFFFu);
			const auto name = std::string(file->GetFilename());
			const auto mesh = std::format("meshes\\actors\\character\\facegendata\\facegeom\\{}\\{:08x}.nif", name, local);
			const auto tex = std::format("textures\\actors\\character\\facegendata\\facetint\\{}\\{:08x}.dds", name, local);
			RE::BSResourceNiBinaryStream m(mesh);
			RE::BSResourceNiBinaryStream t(tex);
			const bool ok = m.good() && t.good();
			logger::info("face files for {:08X}: {} ({})", a_npc->GetFormID(), ok ? "yes" : "no", mesh);
			return ok;
		}

		// One colour layer as stored in the plugin (TINI / TINC / TINV / TIAS).
		struct RawLayer
		{
			std::uint16_t index{ 0 };
			std::uint8_t  rgba[4]{ 0, 0, 0, 255 };
			float         alpha{ 1.0f };
		};

		constexpr std::uint32_t Code(const char (&a_c)[5])
		{
			return static_cast<std::uint32_t>(static_cast<std::uint8_t>(a_c[0])) | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(a_c[1])) << 8) |
			       (static_cast<std::uint32_t>(static_cast<std::uint8_t>(a_c[2])) << 16) | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(a_c[3])) << 24);
		}

		// The game keeps an NPC's colour layers in memory only when it has no finished face files, but they are
		// still in the plugin: read them from the record of the file that last changed the NPC.
		std::vector<RawLayer> ReadLayers(RE::TESNPC* a_npc)
		{
			std::vector<RawLayer> out;
			auto*                 file = a_npc->GetDescriptionOwnerFile();
			if (!file) {
				file = a_npc->GetFile(-1);
			}
			if (!file || Generated(a_npc->GetFormID())) {
				return out;
			}
			if (!file->OpenTES(RE::NiFile::OpenMode::kReadOnly, false)) {
				logger::warn("colour layers: could not open {}", file->GetFilename());
				return out;
			}
			if (file->SeekForm(a_npc)) {
				while (file->SeekNextSubrecord()) {
					const auto type = file->GetCurrentSubRecordType();
					const auto size = file->GetCurrentSubRecordSize();
					if (type == Code("TINI") && size >= 2) {
						RawLayer l;
						file->ReadData(&l.index, 2);
						out.push_back(l);
					} else if (type == Code("TINC") && size >= 4 && !out.empty()) {
						file->ReadData(out.back().rgba, 4);
					} else if (type == Code("TINV") && size >= 4 && !out.empty()) {
						std::int32_t v = 0;
						file->ReadData(&v, 4);
						out.back().alpha = static_cast<float>(v) / 100.0f;
					}
				}
			} else {
				logger::warn("colour layers: {:08X} not found in {}", a_npc->GetFormID(), file->GetFilename());
			}
			file->CloseTES(false);
			logger::info("  read {} colour layer(s) of {:08X} from {}", out.size(), a_npc->GetFormID(), file->GetFilename());
			return out;
		}

		// The look of an NPC, taken from the record its face really comes from (templates resolved).
		Look LookOf(RE::TESNPC* a_npc)
		{
			Look l;
			auto* root = a_npc->GetRootFaceNPC();
			if (!root) {
				root = a_npc;
			}
			// race, sex and body come from the same record as the face (a template's, for templated NPCs)
			auto* race = root->GetRace() ? root->GetRace() : a_npc->GetRace();
			l.valid = race != nullptr;
			l.race = Id(race);
			l.female = root->actorData.actorBaseFlags.all(RE::ACTOR_BASE_DATA::Flag::kFemale);
			l.height = root->height;
			l.weight = root->weight;
			l.body[0] = root->bodyTintColor.red;
			l.body[1] = root->bodyTintColor.green;
			l.body[2] = root->bodyTintColor.blue;
			if (root->headRelatedData) {
				l.hairColor = Id(root->headRelatedData->hairColor);
				l.faceDetails = Id(root->headRelatedData->faceDetails);
			}
			l.skin = Id(root->skin);
			for (int i = 0; root->headParts && i < root->numHeadParts; ++i) {
				if (auto* hp = root->headParts[i]) {
					l.headParts.push_back(hp->GetFormID());
				}
			}
			if (root->faceData) {
				l.hasFaceData = true;
				std::copy_n(root->faceData->morphs, l.morphs.size(), l.morphs.begin());
				std::copy_n(root->faceData->parts, l.presets.size(), l.presets.begin());
			}
			l.faceNpc = !g_ignoreFaceFiles && HasFaceFiles(root) ? root->GetFormID() : 0;

			logger::info("  face record {:08X}: race '{}', tint layer list {} ({} entr(ies)), face data {}", root->GetFormID(), race ? race->GetName() : "?",
				fmt::ptr(root->tintLayers), root->tintLayers ? root->tintLayers->size() : 0, fmt::ptr(root->faceData));
			// colour layers: each NPC layer names one of its race's tint assets by index
			auto* related = race ? race->faceRelatedData[l.female ? 1 : 0] : nullptr;
			std::vector<RawLayer> layers;
			if (root->tintLayers) {
				for (auto* layer : *root->tintLayers) {
					if (layer) {
						RawLayer r;
						r.index = layer->tintIndex;
						r.rgba[0] = layer->tintColor.red;
						r.rgba[1] = layer->tintColor.green;
						r.rgba[2] = layer->tintColor.blue;
						r.rgba[3] = layer->tintColor.alpha;
						r.alpha = layer->GetInterpolationValue();
						layers.push_back(r);
					}
				}
			} else {
				layers = ReadLayers(root);
			}
			if (related && related->tintMasks) {
				for (const auto& layer : layers) {
					std::uint8_t nth[kTintTypes]{};  // position of the asset among the assets of its type
					for (auto* asset : *related->tintMasks) {
						if (!asset) {
							continue;
						}
						const auto type = static_cast<std::uint8_t>(asset->texture.skinTone.get());
						if (asset->texture.index == layer.index) {
							Tint t;
							t.type = type;
							t.index = type < kTintTypes ? nth[type] : 0;
							std::copy_n(layer.rgba, 4, t.rgba);
							t.alpha = layer.alpha;
							t.texture = Lower(asset->texture.file.textureName.c_str());
							l.tints.push_back(std::move(t));
							break;
						}
						if (type < kTintTypes) {
							++nth[type];
						}
					}
				}
			}
			const auto skinType = static_cast<std::uint8_t>(RE::TintMask::Type::kSkinTone);
			if (std::ranges::none_of(l.tints, [&](const Tint& t) { return t.type == skinType; })) {
				Tint t;  // no skin layer: the body colour is the skin tone
				t.type = skinType;
				t.rgba[0] = l.body[0];
				t.rgba[1] = l.body[1];
				t.rgba[2] = l.body[2];
				t.rgba[3] = 255;
				t.alpha = 1.0f;
				l.tints.push_back(std::move(t));
			}
			logger::info("look of {:08X} '{}' (face from {:08X}): race '{}', {} head part(s), {} colour layer(s), {}", a_npc->GetFormID(), a_npc->GetName(),
				root->GetFormID(), race ? race->GetName() : "?", l.headParts.size(), l.tints.size(),
				l.faceNpc ? "finished face files" : "face built from data");
			return l;
		}

		// The player's look as it is now.
		Look OwnLook()
		{
			Look  l;
			auto* p = Player();
			auto* base = p->GetActorBase();
			l.valid = true;
			l.race = Id(p->GetRace());
			l.female = base->actorData.actorBaseFlags.all(RE::ACTOR_BASE_DATA::Flag::kFemale);
			l.height = base->height;
			l.weight = base->weight;
			l.body[0] = base->bodyTintColor.red;
			l.body[1] = base->bodyTintColor.green;
			l.body[2] = base->bodyTintColor.blue;
			if (base->headRelatedData) {
				l.hairColor = Id(base->headRelatedData->hairColor);
				l.faceDetails = Id(base->headRelatedData->faceDetails);
			}
			l.skin = Id(base->skin);
			l.faceNpc = Id(base->faceNPC);
			for (int i = 0; base->headParts && i < base->numHeadParts; ++i) {
				if (auto* hp = base->headParts[i]) {
					l.headParts.push_back(hp->GetFormID());
				}
			}
			if (base->faceData) {
				l.hasFaceData = true;
				std::copy_n(base->faceData->morphs, l.morphs.size(), l.morphs.begin());
				std::copy_n(base->faceData->parts, l.presets.size(), l.presets.begin());
			}
			for (std::uint32_t type = 0; type < kTintTypes; ++type) {
				const auto n = MaskCount(p, type);
				for (std::uint32_t i = 0; i < n; ++i) {
					if (auto* m = Mask(p, type, i)) {
						Tint t;
						t.type = static_cast<std::uint8_t>(type);
						t.index = static_cast<std::uint8_t>(i);
						t.rgba[0] = m->color.red;
						t.rgba[1] = m->color.green;
						t.rgba[2] = m->color.blue;
						t.rgba[3] = m->color.alpha;
						t.alpha = m->alpha;
						t.texture = MaskTexture(m);
						l.tints.push_back(std::move(t));
					}
				}
			}
			logger::info("own look saved: {} head part(s), {} colour layer(s)", l.headParts.size(), l.tints.size());
			return l;
		}

		// Write a look into the player's record (everything but the race and the colour layers).
		void WriteRecord(const Look& a_l)
		{
			auto* base = Player()->GetActorBase();
			if (!base) {
				return;
			}
			base->faceNPC = a_l.faceNpc ? RE::TESForm::LookupByID<RE::TESNPC>(a_l.faceNpc) : nullptr;
			if (a_l.female) {
				base->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kFemale);
			} else {
				base->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kFemale);
			}
			base->height = a_l.height;
			base->weight = a_l.weight;
			base->bodyTintColor.red = a_l.body[0];
			base->bodyTintColor.green = a_l.body[1];
			base->bodyTintColor.blue = a_l.body[2];
			base->skin = a_l.skin ? RE::TESForm::LookupByID<RE::TESObjectARMO>(a_l.skin) : nullptr;
			if (!base->headRelatedData) {
				base->headRelatedData = new RE::TESNPC::HeadRelatedData();
			}
			base->headRelatedData->hairColor = a_l.hairColor ? RE::TESForm::LookupByID<RE::BGSColorForm>(a_l.hairColor) : nullptr;
			base->headRelatedData->faceDetails = a_l.faceDetails ? RE::TESForm::LookupByID<RE::BGSTextureSet>(a_l.faceDetails) : nullptr;

			// a fresh head-part list owned by the player's record, never shared with another NPC
			std::vector<RE::BGSHeadPart*> parts;
			for (auto id : a_l.headParts) {
				if (auto* hp = RE::TESForm::LookupByID<RE::BGSHeadPart>(id)) {
					parts.push_back(hp);
				}
			}
			auto** list = RE::calloc<RE::BGSHeadPart*>(std::max<std::size_t>(parts.size(), 1));
			std::ranges::copy(parts, list);
			base->headParts = list;
			base->numHeadParts = static_cast<std::int8_t>(parts.size());

			if (!base->faceData) {
				base->faceData = RE::calloc<RE::TESNPC::FaceData>(1);
			}
			if (a_l.hasFaceData) {
				std::ranges::copy(a_l.morphs, base->faceData->morphs);
				std::ranges::copy(a_l.presets, base->faceData->parts);
			} else {
				std::ranges::fill(base->faceData->morphs, 0.0f);
				std::ranges::fill(base->faceData->parts, 0);
			}
		}

		// Paint the player's colour layers: each mask takes the look's layer with the same texture (or, failing
		// that, the same type and position); masks the look does not use are turned off.
		void WriteTints(const std::vector<Tint>& a_tints)
		{
			auto*             p = Player();
			std::vector<bool> used(a_tints.size(), false);
			int               set = 0, off = 0;
			for (std::uint32_t type = 0; type < kTintTypes; ++type) {
				const auto n = MaskCount(p, type);
				for (std::uint32_t i = 0; i < n; ++i) {
					auto* m = Mask(p, type, i);
					if (!m) {
						continue;
					}
					const auto  tex = MaskTexture(m);
					std::size_t pick = a_tints.size();
					for (std::size_t k = 0; k < a_tints.size() && pick == a_tints.size(); ++k) {
						if (!used[k] && a_tints[k].type == type && !tex.empty() && a_tints[k].texture == tex) {
							pick = k;
						}
					}
					for (std::size_t k = 0; k < a_tints.size() && pick == a_tints.size(); ++k) {
						if (!used[k] && a_tints[k].type == type && a_tints[k].index == i) {
							pick = k;
						}
					}
					if (pick == a_tints.size()) {
						m->alpha = 0.0f;
						++off;
						continue;
					}
					used[pick] = true;
					const auto& t = a_tints[pick];
					m->color.red = t.rgba[0];
					m->color.green = t.rgba[1];
					m->color.blue = t.rgba[2];
					m->color.alpha = t.rgba[3];
					m->alpha = t.alpha;
					++set;
				}
			}
			logger::info("colour layers: {} painted, {} turned off, {} of the look's not placed", set, off, std::ranges::count(used, false));
		}

		void CallGame(const char* a_fn)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm) {
				return;
			}
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
			vm->DispatchStaticCall("Game", a_fn, RE::MakeFunctionArguments(), cb);
		}

		void CallPlayer(const char* a_fn)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			auto* p = Player();
			if (!vm || !p) {
				return;
			}
			const auto handle = vm->GetObjectHandlePolicy()->GetHandleForObject(p->GetFormType(), p);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
			vm->DispatchMethodCall2(handle, "Actor", a_fn, RE::MakeFunctionArguments(), cb);
		}

		// Put a look on: the record, colours and race now (a race change rebuilds the model), then the head, face
		// colours and hair are redrawn a few frames later once the model is back.
		void Wear(const Look& a_l)
		{
			auto* p = Player();
			WriteRecord(a_l);
			WriteTints(a_l.tints);  // before the rebuild: the body takes its colour from these when the model is built
			auto* race = RE::TESForm::LookupByID<RE::TESRace>(a_l.race);
			if (race && p->GetRace() != race) {
				p->SwitchRace(race, true);
			} else {
				SKSE::GetTaskInterface()->AddTask([]() {
					if (auto* pl = Player(); pl && pl->Is3DLoaded()) {
						pl->DoReset3D(true);
					}
				});
			}
			g_pendingTints = a_l.tints;
			g_redraw = kRedrawDelay;
		}

		// Every frame: the delayed half of Wear. The model may take a while to rebuild, so the face colours are
		// painted more than once.
		void RedrawTick()
		{
			if (g_redraw <= 0) {
				return;
			}
			--g_redraw;
			const bool animal = !State::Get().active.empty();  // changed into an animal meanwhile: no head to redraw
			if (animal) {
				g_redraw = 0;
				return;
			}
			if (g_redraw == kHeadAt) {
				CallPlayer("RegenerateHead");
			}
			if (std::ranges::find(kPaintAt, g_redraw) != std::end(kPaintAt)) {
				WriteTints(g_pendingTints);
				CallGame("UpdateTintMaskColors");
				CallGame("UpdateHairColor");
				Player()->UpdateSkinColor();
			}
			if (g_redraw == 0) {
				g_pendingTints.clear();
				logger::info("face redrawn");
			}
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
				return false;  // an animal: it teaches a form, not a face
			}
			return true;
		}

		// The game menu (save, load, quit) may take a stolen face off ([Face] TakeOffOnMenu).
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
				if (a_event && a_event->opening && a_event->menuName == RE::JournalMenu::MENU_NAME && g_menuTakeOff && State::Get().disguised) {
					// Saving, loading and quitting with a stolen face on is not safe yet: take it off when the game menu opens
					SKSE::GetTaskInterface()->AddTask([]() {
						TakeOff("the game menu opened");
						Shift::Notify("Your stolen face came off. Use Stolen Face to wear it again.");
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	// ---- public --------------------------------------------------------------------------------------

	void Install()
	{
		LoadIni();
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuSink::Get());
		}
		logger::info("face stealing {} (JoinFactions {}, TakeOffOnMenu {}, WitnessRange {:.0f})", g_enabled ? "on" : "off", g_joinFactions, g_menuTakeOff, g_range);
	}

	bool Disguised()
	{
		return State::Get().disguised;
	}

	// Borrowed Visage: putting a face on costs the stamina of a change of form (taking it off is free)
	bool PayForFace(RE::PlayerCharacter* a_p)
	{
		const float cost = Registry::Get().Dial("Protean_ChangeStaminaCost", 15.0f);
		auto*       av = a_p->AsActorValueOwner();
		if (cost > 0.0f && av->GetActorValue(RE::ActorValue::kStamina) < cost) {
			Shift::Notify("You are too tired to change your face.");
			logger::info("face: refused, stamina {:.0f} < {:.0f}", av->GetActorValue(RE::ActorValue::kStamina), cost);
			return false;
		}
		if (cost > 0.0f) {
			av->DamageActorValue(RE::ActorValue::kStamina, cost);
		}
		return true;
	}

	void Steal(RE::TESNPC* a_source, RE::Actor* a_corpse, bool a_witnessed, bool a_pay)
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
		if (a_pay && !PayForFace(p)) {
			return;
		}
		logger::info("=== steal face of {:08X} '{}' (race '{}', witnessed check {}) ===", a_source->GetFormID(), a_source->GetName(),
			a_source->GetRace()->GetName(), a_witnessed);
		if (s.disguised) {
			LeaveFactions();  // swapping one stolen face for another: the own look from the first theft stays
		} else {
			s.own = OwnLook();
		}
		if (a_witnessed) {
			Expose(a_corpse);
		}
		s.bank = LookOf(a_source);
		s.bankedNpc = a_source->GetFormID();
		Wear(s.bank);
		if (g_joinFactions) {
			JoinFactions(a_source);
		}
		if (auto* fp = Registry::Get().FacePower(); fp && !p->HasSpell(fp)) {
			p->AddSpell(fp);
		}
		if (!s.disguised) {
			Shift::SnapshotFaceCrime();  // a swap keeps the first face's snapshot
		}
		s.disguised = true;
		Shift::Notify(std::format("You have taken the face of {}.", a_source->GetName()));
		Dump();
	}

	void FromBody(RE::Actor* a_body, bool a_wear, bool a_free, bool a_witnessed)
	{
		auto* p = Player();
		auto& s = State::Get();
		auto* npc = a_body ? a_body->GetActorBase() : nullptr;
		if (!g_enabled || !p || !npc || !Eligible(a_body)) {
			return;
		}
		const float cost = Registry::Get().Dial("Protean_ChangeStaminaCost", 15.0f);
		if (a_wear && s.active.empty() &&
			(a_free || cost <= 0.0f || p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) >= cost)) {
			Steal(npc, a_body, a_witnessed, !a_free);
			return;
		}
		if (s.disguised) {
			// the worn face is the banked one: a new one would take its place under it
			Shift::Notify(std::format("You wear a stolen face already: the face of {} is lost.", npc->GetName()));
			logger::info("face: '{}' not kept, a stolen face is worn", npc->GetName());
			return;
		}
		s.bank = LookOf(npc);
		s.bankedNpc = npc->GetFormID();
		if (auto* fp = Registry::Get().FacePower(); fp && !p->HasSpell(fp)) {
			p->AddSpell(fp);
		}
		Shift::Notify(std::format("You keep the face of {}. Cast Stolen Face in your own form to wear it.", npc->GetName()));
		logger::info("face: '{}' {:08X} kept ({})", npc->GetName(), npc->GetFormID(), a_wear ? "too tired to wear it" : "animal form");
	}

	void SetIgnoreFaceFiles(bool a_on)
	{
		g_ignoreFaceFiles = a_on;
	}

	void TakeOff(std::string_view a_why)
	{
		auto& s = State::Get();
		if (!s.disguised) {
			return;
		}
		logger::info("=== take off the stolen face ({}) ===", a_why);
		LeaveFactions();
		if (s.own.valid) {
			Wear(s.own);
		} else {
			logger::error("no own look saved; cannot take the stolen face off cleanly");
		}
		s.disguised = false;
		Shift::ForgiveFaceCrime();
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
		if (!s.bank.valid && s.bankedNpc) {
			if (auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(s.bankedNpc)) {
				s.bank = LookOf(npc);  // a face kept by an older version of Protean
			}
		}
		if (!s.bank.valid) {
			Shift::Notify("You have no face kept. Consume a person to take theirs.");
			return;
		}
		if (!PayForFace(Player())) {
			return;
		}
		logger::info("=== wear the kept face ===");
		s.own = OwnLook();
		Wear(s.bank);
		if (auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(s.bankedNpc); npc && g_joinFactions) {
			JoinFactions(npc);
		}
		if (!s.disguised) {
			Shift::SnapshotFaceCrime();  // a swap keeps the first face's snapshot
		}
		s.disguised = true;
		Shift::Notify("You wear the face you kept.");
		Dump();
	}

	void OnFrame()
	{
		RedrawTick();
	}

	void OnGameLoaded()
	{
		auto& s = State::Get();
		g_redraw = 0;
		if (!s.disguised) {
			return;
		}
		if (!s.bank.valid) {
			logger::warn("loaded a save made in a stolen face that was not kept; staying as saved");
			return;
		}
		logger::info("loaded a save made in a stolen face: putting it on again");
		Wear(s.bank);
	}

	void BeforeLoad()
	{
		g_redraw = 0;
		g_pendingTints.clear();
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
