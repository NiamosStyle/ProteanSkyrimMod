// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#include "Face.h"

#include "Evolve.h"

#include "Registry.h"
#include "Shift.h"
#include "State.h"

#include <atomic>
#include <mutex>
#include <thread>

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

		// Every colour mask the player has, in the game's own order: the base list and, when the game keeps one, the
		// overlay list it draws from (GetTintMask reads only the list in use).
		struct ListedMask
		{
			char          list;  // 'b' base, 'o' overlay
			std::uint32_t pos;
			RE::TintMask* mask;
		};
		std::vector<ListedMask> AllMasks(RE::PlayerCharacter* a_p)
		{
			std::vector<ListedMask> out;
			// through CommonLib's runtime-data accessor: the fields sit 8 bytes later from 1.7.99 on (a hard-coded 1.6
			// offset crashed on 1.7.104, 2026-11-19)
			auto& rd = a_p->GetPlayerRuntimeData();
			auto* overlay = rd.overlayTintMasks;
			auto& base = rd.tintMasks;
			if (reinterpret_cast<std::uintptr_t>(overlay) < 0x10000) {
				overlay = nullptr;
			}
			auto  add = [&](RE::BSTArray<RE::TintMask*>& a_list, char a_name) {
				if (a_list.size() > 256) {
					return;
				}
				for (std::uint32_t i = 0; i < a_list.size(); ++i) {
					auto* m = a_list[i];
					if (reinterpret_cast<std::uintptr_t>(m) >= 0x10000 && static_cast<std::uint32_t>(m->type.get()) < kTintTypes) {
						out.push_back({ a_name, i, m });
					}
				}
			};
			add(base, 'b');
			if (overlay) {
				add(*overlay, 'o');
			}
			// check the lists against the game's own lookup: its skin-tone mask must be among them
			const auto skin = static_cast<std::uint32_t>(RE::TintMask::Type::kSkinTone);
			if (auto* gm = Mask(a_p, skin, 0); gm && std::ranges::none_of(out, [&](const ListedMask& m) { return m.mask == gm; })) {
				logger::error("colour masks: the lists read ({} mask(s)) do not hold the game's skin-tone mask; not used", out.size());
				out.clear();
			}
			return out;
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
			// every mask of the list the game draws from (the overlay list when there is one), as WriteTintsExact sees them
			const auto masks = AllMasks(p);
			if (masks.empty()) {  // the lists could not be read: the game's own lookup, by type and position
				for (std::uint32_t type = 0; type < kTintTypes; ++type) {
					for (std::uint32_t i = 0, n = MaskCount(p, type); i < n; ++i) {
						if (auto* m = Mask(p, type, i)) {
							Tint t;
							t.type = static_cast<std::uint8_t>(type);
							t.index = static_cast<std::uint8_t>(i);
							std::copy_n(&m->color.red, 4, t.rgba);
							t.alpha = m->alpha;
							t.texture = MaskTexture(m);
							l.tints.push_back(std::move(t));
						}
					}
				}
			}
			const char drawn = std::ranges::any_of(masks, [](const ListedMask& m) { return m.list == 'o'; }) ? 'o' : 'b';
			std::uint8_t nth[kTintTypes]{};
			for (const auto& lm : masks) {
				if (lm.list != drawn) {
					continue;
				}
				auto*      m = lm.mask;
				const auto type = static_cast<std::uint8_t>(m->type.get());
				Tint       t;
				t.type = type;
				t.index = nth[type]++;
				t.rgba[0] = m->color.red;
				t.rgba[1] = m->color.green;
				t.rgba[2] = m->color.blue;
				t.rgba[3] = m->color.alpha;
				t.alpha = m->alpha;
				t.texture = MaskTexture(m);
				l.tints.push_back(std::move(t));
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

		// Make the player's colour-mask list the race's own list for a sex: one mask per tint asset, in the race's order,
		// with its texture and type. The game builds that list only in character creation and when a save loads (a save
		// keeps each layer as a race-asset number); a race or sex change leaves the old list in place. So after loading a
		// save made in a female stolen face, a male player kept the female list (33 masks, femalehead... textures) and
		// his own layers had nowhere to go (Mercer, 2026-11-19). Masks are reused; any new one is zeroed (off).
		void RebuildMasks(RE::TESRace* a_race, bool a_female)
		{
			auto* p = Player();
			auto* related = a_race ? a_race->faceRelatedData[a_female ? 1 : 0] : nullptr;
			if (!p || !related || !related->tintMasks) {
				return;
			}
			auto& rd = p->GetPlayerRuntimeData();
			if (reinterpret_cast<std::uintptr_t>(rd.overlayTintMasks) >= 0x10000) {
				logger::warn("colour masks: the game keeps an overlay list now (RaceMenu?); the list is not rebuilt");
				return;
			}
			std::vector<RE::TESRace::FaceRelatedData::TintAsset*> assets;
			for (auto* a : *related->tintMasks) {
				if (a) {
					assets.push_back(a);
				}
			}
			if (assets.empty() || assets.size() > 256) {
				return;
			}
			auto& list = rd.tintMasks;
			if (list.size() > 256) {
				return;
			}
			bool same = list.size() == assets.size();
			for (std::uint32_t i = 0; same && i < list.size(); ++i) {
				auto* m = list[i];
				same = reinterpret_cast<std::uintptr_t>(m) >= 0x10000 && m->texture == &assets[i]->texture.file &&
				       static_cast<std::uint32_t>(m->type.get()) == static_cast<std::uint32_t>(assets[i]->texture.skinTone.get());
			}
			if (same) {
				return;
			}
			std::vector<RE::TintMask*> old;
			for (auto* m : list) {
				if (reinterpret_cast<std::uintptr_t>(m) >= 0x10000) {
					old.push_back(m);
				}
			}
			const auto before = list.size();
			const auto firstTex = !old.empty() ? MaskTexture(old.front()) : std::string{};
			list.clear();
			std::size_t made = 0;
			for (std::size_t i = 0; i < assets.size(); ++i) {
				RE::TintMask* m = i < old.size() ? old[i] : nullptr;
				if (!m) {
					m = RE::malloc<RE::TintMask>();
					std::memset(m, 0, sizeof(RE::TintMask));
					++made;
				}
				m->texture = &assets[i]->texture.file;
				m->type = static_cast<RE::TintMask::Type>(assets[i]->texture.skinTone.get());
				m->color = RE::Color(255, 255, 255, 0);
				m->alpha = 0.0f;
				list.push_back(m);
			}
			// masks beyond the race's count are left unused (a few bytes; freeing what the game allocated is not worth the risk)
			logger::warn("colour masks rebuilt for race '{}' ({}): {} -> {} mask(s), {} new (the list was another race's or sex's: first was '{}')",
				a_race->GetName(), a_female ? "female" : "male", before, list.size(), made, firstTex);
		}

		// Paint the player's OWN colour layers back: every mask takes the layer with exactly its texture and type,
		// never one picked by position (a list rebuilt by a load can be ordered or sized differently, and a position
		// match put one layer's colour on another: Mercer's war paint, 2026-11-19). Masks the look has no layer for are
		// turned off; layers with no mask to go on are named in the log.
		void WriteTintsExact(const std::vector<Tint>& a_tints)
		{
			auto* p = Player();
			const auto masks = AllMasks(p);
			int set = 0, off = 0;
			for (char list : { 'b', 'o' }) {
				std::vector<bool> used(a_tints.size(), false);
				std::uint32_t     inList = 0;
				for (const auto& lm : masks) {
					if (lm.list != list) {
						continue;
					}
					++inList;
					auto*       m = lm.mask;
					const auto  type = static_cast<std::uint8_t>(m->type.get());
					const auto  tex = MaskTexture(m);
					std::size_t pick = a_tints.size();
					// a mask is known by its texture; its type label must agree but can be wrong (Mercer's dirt #03 was
					// labelled freckles, 2026-11-19), so it does not decide the match. Untextured masks match by type.
					for (std::size_t k = 0; k < a_tints.size() && pick == a_tints.size(); ++k) {
						if (!used[k] && a_tints[k].texture == tex && (!tex.empty() || a_tints[k].type == type)) {
							pick = k;
						}
					}
					if (pick != a_tints.size() && a_tints[pick].type != type && a_tints[pick].type < kTintTypes) {
						logger::warn("  colour mask {}{} '{}' was labelled type {}: set back to {}", list, lm.pos, tex, type, a_tints[pick].type);
						m->type = static_cast<RE::TintMask::Type>(a_tints[pick].type);
					}
					if (pick == a_tints.size()) {
						if (m->alpha > 0.0f) {
							logger::info("  colour mask {}{} type {} '{}' has no own layer: turned off (was alpha {:.2f})", list, lm.pos, type, tex, m->alpha);
						}
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
				if (inList == 0) {
					continue;
				}
				for (std::size_t k = 0; k < a_tints.size(); ++k) {
					if (!used[k]) {
						logger::warn("  own colour layer type {} '{}' (alpha {:.2f}) has no mask in list {} to go on", a_tints[k].type,
							a_tints[k].texture, a_tints[k].alpha, list);
					}
				}
			}
			logger::info("own colour layers: {} painted, {} turned off ({} mask(s) in all)", set, off, masks.size());
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
		bool            g_pendingExact{ false };  // the pending layers are the player's own: painted by exact texture only
		RE::FormID      g_pendingRace{ 0 };       // the pending look's race and sex: its colour-mask list
		bool            g_pendingFemale{ false };
		std::thread::id g_mainThread{};           // set every frame; the mask list is rebuilt only on the main thread

		bool OnMainThread() { return std::this_thread::get_id() == g_mainThread; }

		void Paint(const std::vector<Tint>& a_tints, bool a_exact)
		{
			if (a_exact && !AllMasks(Player()).empty()) {
				WriteTintsExact(a_tints);
			} else {
				WriteTints(a_tints);
			}
		}

		// a_own: the look is the player's own (taking a stolen face off)
		void Wear(const Look& a_l, bool a_own = false)
		{
			auto* p = Player();
			auto* race = RE::TESForm::LookupByID<RE::TESRace>(a_l.race);
			WriteRecord(a_l);
			if (OnMainThread()) {
				RebuildMasks(race, a_l.female);  // the list must be this race's and sex's before anything is painted
			}  // (off the main thread: at the redraw, before the layers are painted again)
			Paint(a_l.tints, a_own);  // before the rebuild: the body takes its colour from these when the model is built
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
			g_pendingExact = a_own;
			g_pendingRace = a_l.race;
			g_pendingFemale = a_l.female;
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
			if (g_redraw == kRedrawDelay - 1 || std::ranges::find(kPaintAt, g_redraw) != std::end(kPaintAt)) {
				RebuildMasks(RE::TESForm::LookupByID<RE::TESRace>(g_pendingRace), g_pendingFemale);
				Paint(g_pendingTints, g_pendingExact);
				CallGame("UpdateTintMaskColors");
				CallGame("UpdateHairColor");
				Player()->UpdateSkinColor();
			}
			if (g_redraw == 0) {
				g_pendingTints.clear();
				logger::info("face redrawn");
			}
		}

		// ---- the true face ----------------------------------------------------------------------------
		//
		// The player's face as it was when Protean was gained (or last set in RaceMenu): kept in the co-save AND in a
		// file per character under the SKSE log folder. Taking a stolen face off paints it back, and every load and
		// change back from an animal compares the face with it and repairs any drift (user's design, 2026-11-19, after
		// Mercer's war paint drifted save by save). Every repair is logged in detail and shown on screen.

		std::filesystem::path TintFile(std::string_view a_name)
		{
			auto dir = SKSE::log::log_directory().value_or(std::filesystem::path{});
			return dir / std::format("Protean-tints-{}.txt", a_name.empty() ? "dump" : a_name);
		}

		// A tint dump (dev "tintdump"): the base list's layers. Empty when there is no such file.
		std::vector<Tint> ReadDump(std::string_view a_name)
		{
			std::vector<Tint> tints;
			std::ifstream     in(TintFile(a_name));
			std::string       line;
			while (std::getline(in, line)) {
				std::istringstream ls(line);
				std::string        where, tex;
				std::uint32_t      type = 0, r = 0, g = 0, b = 0, a = 0;
				float              alpha = 0.0f;
				if (!(ls >> where >> type >> r >> g >> b >> a >> alpha) || where.empty() || where[0] != 'b') {
					continue;  // the base list is the record of the face; an overlay copy would double it
				}
				std::getline(ls >> std::ws, tex);
				Tint t;
				t.type = static_cast<std::uint8_t>(type);
				t.rgba[0] = static_cast<std::uint8_t>(r);
				t.rgba[1] = static_cast<std::uint8_t>(g);
				t.rgba[2] = static_cast<std::uint8_t>(b);
				t.rgba[3] = static_cast<std::uint8_t>(a);
				t.alpha = alpha;
				t.texture = tex;
				tints.push_back(std::move(t));
			}
			return tints;
		}

		// A form as "<plugin>|<local id>", so the file survives a changed load order
		std::string FormKey(RE::FormID a_id)
		{
			auto* form = a_id ? RE::TESForm::LookupByID(a_id) : nullptr;
			auto* file = form ? form->GetFile(0) : nullptr;
			if (!form || !file) {
				return a_id ? std::format("|{:08X}", a_id) : "-";
			}
			const bool light = file->recordFlags.all(RE::TESFile::RecordFlag::kSmallFile);
			return std::format("{}|{:06X}", file->GetFilename(), a_id & (light ? 0xFFFu : 0xFFFFFFu));
		}

		RE::FormID ParseFormKey(const std::string& a_key)
		{
			if (a_key.empty() || a_key == "-") {
				return 0;
			}
			const auto bar = a_key.find('|');
			if (bar == std::string::npos) {
				return 0;
			}
			const auto local = static_cast<RE::FormID>(std::stoul(a_key.substr(bar + 1), nullptr, 16));
			if (bar == 0) {
				return local;  // a game-made form, kept as is
			}
			auto* dh = RE::TESDataHandler::GetSingleton();
			return dh ? dh->LookupFormID(local, a_key.substr(0, bar)) : 0;
		}

		std::string SafeName(const char* a_name)
		{
			std::string s = a_name ? a_name : "";
			std::ranges::replace_if(s, [](unsigned char c) { return !std::isalnum(c) && c != '-' && c != '_'; }, '_');
			return s.empty() ? "player" : s;
		}

		std::filesystem::path TrueFolder()
		{
			return SKSE::log::log_directory().value_or(std::filesystem::path{}) / "Protean-faces";
		}

		std::filesystem::path TrueFile(std::uint32_t a_id)
		{
			return TrueFolder() / std::format("{}-{:08X}.txt", SafeName(Player()->GetName()), a_id);
		}

		void WriteTrueFile(const Look& a_l, std::uint32_t a_id)
		{
			std::error_code ec;
			std::filesystem::create_directories(TrueFolder(), ec);
			const auto    path = TrueFile(a_id);
			std::ofstream out(path, std::ios::trunc);
			out << "# Protean true face. Lines: key values. Forms are plugin|local id.\n";
			out << std::format("race {}\nfemale {}\nheight {:.6f}\nweight {:.6f}\nbody {} {} {}\n", FormKey(a_l.race), a_l.female ? 1 : 0, a_l.height,
				a_l.weight, a_l.body[0], a_l.body[1], a_l.body[2]);
			out << std::format("hair {}\ndetails {}\nskin {}\nfacenpc {}\nhasface {}\n", FormKey(a_l.hairColor), FormKey(a_l.faceDetails),
				FormKey(a_l.skin), FormKey(a_l.faceNpc), a_l.hasFaceData ? 1 : 0);
			for (auto id : a_l.headParts) {
				out << "part " << FormKey(id) << "\n";
			}
			out << "morphs";
			for (float m : a_l.morphs) {
				out << std::format(" {:.9g}", m);
			}
			out << "\npresets";
			for (auto p : a_l.presets) {
				out << " " << p;
			}
			out << "\n";
			for (const auto& t : a_l.tints) {
				out << std::format("tint {} {} {} {} {} {:.6f} {}\n", t.type, t.rgba[0], t.rgba[1], t.rgba[2], t.rgba[3], t.alpha, t.texture);
			}
			logger::info("true face written to {}", path.string());
		}

		bool ReadTrueFile(const std::filesystem::path& a_path, Look& a_l)
		{
			std::ifstream in(a_path);
			if (!in) {
				return false;
			}
			Look        l;
			std::string line;
			std::uint8_t nth[kTintTypes]{};
			while (std::getline(in, line)) {
				std::istringstream ls(line);
				std::string        key, v;
				ls >> key;
				if (key == "race") {
					ls >> v;
					l.race = ParseFormKey(v);
				} else if (key == "female") {
					int f = 0;
					ls >> f;
					l.female = f != 0;
				} else if (key == "height") {
					ls >> l.height;
				} else if (key == "weight") {
					ls >> l.weight;
				} else if (key == "body") {
					int r = 0, g = 0, b = 0;
					ls >> r >> g >> b;
					l.body[0] = static_cast<std::uint8_t>(r);
					l.body[1] = static_cast<std::uint8_t>(g);
					l.body[2] = static_cast<std::uint8_t>(b);
				} else if (key == "hair") {
					ls >> v;
					l.hairColor = ParseFormKey(v);
				} else if (key == "details") {
					ls >> v;
					l.faceDetails = ParseFormKey(v);
				} else if (key == "skin") {
					ls >> v;
					l.skin = ParseFormKey(v);
				} else if (key == "facenpc") {
					ls >> v;
					l.faceNpc = ParseFormKey(v);
				} else if (key == "hasface") {
					int f = 0;
					ls >> f;
					l.hasFaceData = f != 0;
				} else if (key == "part") {
					ls >> v;
					if (auto id = ParseFormKey(v)) {
						l.headParts.push_back(id);
					}
				} else if (key == "morphs") {
					for (auto& m : l.morphs) {
						ls >> m;
					}
				} else if (key == "presets") {
					for (auto& p : l.presets) {
						ls >> p;
					}
				} else if (key == "tint") {
					Tint          t;
					std::uint32_t type = 0, r = 0, g = 0, b = 0, a = 0;
					ls >> type >> r >> g >> b >> a >> t.alpha;
					std::getline(ls >> std::ws, t.texture);
					t.type = static_cast<std::uint8_t>(type);
					t.index = type < kTintTypes ? nth[type]++ : 0;
					t.rgba[0] = static_cast<std::uint8_t>(r);
					t.rgba[1] = static_cast<std::uint8_t>(g);
					t.rgba[2] = static_cast<std::uint8_t>(b);
					t.rgba[3] = static_cast<std::uint8_t>(a);
					l.tints.push_back(std::move(t));
				}
			}
			l.valid = l.race != 0 && !l.headParts.empty();
			if (!l.valid) {
				return false;
			}
			a_l = std::move(l);
			return true;
		}

		// Everything in which the face now differs from a look (empty: the same)
		std::vector<std::string> Differences(const Look& a_want)
		{
			std::vector<std::string> out;
			const Look               now = OwnLook();
			auto                     form = [](RE::FormID id) { return FormKey(id); };
			if (now.female != a_want.female) {
				out.push_back("sex");
			}
			if (std::abs(now.height - a_want.height) > 0.001f) {
				out.push_back(std::format("height {:.3f} (true {:.3f})", now.height, a_want.height));
			}
			if (std::abs(now.weight - a_want.weight) > 0.01f) {
				out.push_back(std::format("weight {:.1f} (true {:.1f})", now.weight, a_want.weight));
			}
			if (!std::ranges::equal(now.body, a_want.body)) {
				out.push_back("body colour");
			}
			if (now.hairColor != a_want.hairColor) {
				out.push_back(std::format("hair colour {} (true {})", form(now.hairColor), form(a_want.hairColor)));
			}
			if (now.faceDetails != a_want.faceDetails) {
				out.push_back("face details");
			}
			if (now.skin != a_want.skin) {
				out.push_back(std::format("skin {} (true {})", form(now.skin), form(a_want.skin)));
			}
			if (now.faceNpc != a_want.faceNpc) {
				out.push_back("face files");
			}
			{
				auto a = now.headParts, b = a_want.headParts;
				std::ranges::sort(a);
				std::ranges::sort(b);
				if (a != b) {
					out.push_back(std::format("head parts ({} now, {} true)", a.size(), b.size()));
				}
			}
			if (now.hasFaceData && a_want.hasFaceData) {
				for (std::size_t i = 0; i < now.morphs.size(); ++i) {
					if (std::abs(now.morphs[i] - a_want.morphs[i]) > 0.001f) {
						out.push_back(std::format("face slider {} {:.2f} (true {:.2f})", i, now.morphs[i], a_want.morphs[i]));
					}
				}
				if (now.presets != a_want.presets) {
					out.push_back("face presets");
				}
			}
			// colour layers by texture; off layers (alpha 0) only matter for their label
			for (const auto& w : a_want.tints) {
				auto it = std::ranges::find_if(now.tints, [&](const Tint& t) { return t.texture == w.texture && (!w.texture.empty() || t.type == w.type); });
				if (it == now.tints.end()) {
					if (w.alpha > 0.0f) {
						out.push_back(std::format("colour layer '{}' missing", w.texture));
					}
					continue;
				}
				if (it->type != w.type) {
					out.push_back(std::format("colour layer '{}' labelled type {} (true {})", w.texture, it->type, w.type));
				}
				const bool on = it->alpha > 0.001f || w.alpha > 0.001f;
				if (on && (std::abs(it->alpha - w.alpha) > 0.005f || !std::equal(it->rgba, it->rgba + 3, w.rgba))) {
					out.push_back(std::format("colour layer '{}' type {}: rgb {} {} {} alpha {:.2f} (true rgb {} {} {} alpha {:.2f})", w.texture, w.type,
						it->rgba[0], it->rgba[1], it->rgba[2], it->alpha, w.rgba[0], w.rgba[1], w.rgba[2], w.alpha));
				}
			}
			for (const auto& t : now.tints) {
				if (t.alpha > 0.001f && std::ranges::none_of(a_want.tints, [&](const Tint& w) { return w.texture == t.texture; })) {
					out.push_back(std::format("colour layer '{}' on but not in the true face", t.texture));
				}
			}
			return out;
		}

		int g_checkIn{ 0 };       // frames until the true face is checked (0 = none)
		std::string g_checkWhy;
		int g_takeTick{ 0 };

		bool OwnFormNow()
		{
			const auto& s = State::Get();
			auto*       p = Player();
			return p && s.active.empty() && !s.disguised && g_redraw == 0 && p->Is3DLoaded();
		}

		void TakeTrueFace(std::string_view a_why)
		{
			auto& s = State::Get();
			s.truth = OwnLook();
			if (!s.truthId) {
				s.truthId = static_cast<std::uint32_t>(std::chrono::system_clock::now().time_since_epoch().count() & 0xFFFFFFFF);
			}
			logger::info("=== true face taken ({}): {} head part(s), {} colour layer(s) ===", a_why, s.truth.headParts.size(), s.truth.tints.size());
			WriteTrueFile(s.truth, s.truthId);
		}

		void CheckTrueFace()
		{
			auto& s = State::Get();
			if (!s.truth.valid || !OwnFormNow()) {
				return;
			}
			if (Id(Player()->GetRace()) != s.truth.race) {
				logger::info("true face not checked ({}): the race is not the true face's (a vampire?)", g_checkWhy);
				return;
			}
			const auto diffs = Differences(s.truth);
			if (diffs.empty()) {
				logger::info("true face checked ({}): unchanged", g_checkWhy);
				return;
			}
			logger::warn("=== true face DRIFTED ({}): {} difference(s); repairing ===", g_checkWhy, diffs.size());
			for (const auto& d : diffs) {
				logger::warn("  {}", d);
			}
			Wear(s.truth, true);
			Shift::Notify(std::format("Protean: your true face was restored ({} difference(s), see Protean.log).", diffs.size()));
		}

		void ScheduleCheck(std::string_view a_why, int a_frames = 120)
		{
			g_checkIn = a_frames;
			g_checkWhy = a_why;
		}

		// every frame: the scheduled check, and taking the true face once Protean is there and none is kept
		void TrueFaceTick()
		{
			if (g_checkIn > 0 && --g_checkIn == 0) {
				CheckTrueFace();
			}
			if (++g_takeTick < 60) {
				return;
			}
			g_takeTick = 0;
			auto& s = State::Get();
			auto* p = Player();
			auto* starter = Registry::Get().Starter();
			if (!s.truth.valid && starter && p && p->HasSpell(starter) && OwnFormNow() && g_checkIn == 0) {
				TakeTrueFace("Protean is known and no true face was kept");
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
				if (a_event && !a_event->opening && a_event->menuName == RE::RaceSexMenu::MENU_NAME) {
					// RaceMenu closed: a face changed on purpose becomes the true face (once Protean is known)
					SKSE::GetTaskInterface()->AddTask([]() {
						auto* p = Player();
						auto* starter = Registry::Get().Starter();
						if (p && starter && p->HasSpell(starter) && State::Get().active.empty() && !State::Get().disguised) {
							g_redraw = 0;
							TakeTrueFace("RaceMenu closed");
						}
					});
				}
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
		if (!s.disguised || !s.faceCrimeTaken) {  // (not taken: Assassin just forgave the last face's)
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
		if (s.truth.valid && (!s.own.valid || s.own.race == s.truth.race)) {
			Wear(s.truth, true);  // the true face, never a copy taken when the stolen one went on
		} else if (s.own.valid) {
			logger::info("own race differs from the true face's (a vampire?): the face from before the theft goes back");
			Wear(s.own, true);
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
		if (!s.disguised || !s.faceCrimeTaken) {  // (not taken: Assassin just forgave the last face's)
			Shift::SnapshotFaceCrime();  // a swap keeps the first face's snapshot
		}
		s.disguised = true;
		Shift::Notify("You wear the face you kept.");
		Dump();
	}

	void OnFrame()
	{
		g_mainThread = std::this_thread::get_id();
		RedrawTick();
		TrueFaceTick();
	}

	void AfterRevert()
	{
		ScheduleCheck("changed back from an animal");
	}

	void OnGameLoaded()
	{
		auto& s = State::Get();
		g_redraw = 0;
		if (!s.truth.valid) {
			// no true face in this save's co-save: the character's file, when there is exactly one for this name
			std::error_code ec;
			std::vector<std::filesystem::path> found;
			const auto prefix = SafeName(Player()->GetName()) + "-";
			for (const auto& e : std::filesystem::directory_iterator(TrueFolder(), ec)) {
				const auto n = e.path().filename().string();
				if (n.starts_with(prefix) && n.ends_with(".txt") && n.size() == prefix.size() + 12) {
					found.push_back(e.path());
				}
			}
			if (found.size() == 1 && ReadTrueFile(found[0], s.truth)) {
				s.truthId = static_cast<std::uint32_t>(std::stoul(found[0].stem().string().substr(prefix.size()), nullptr, 16));
				logger::info("true face read from {} (not in this save)", found[0].string());
			} else if (found.size() > 1) {
				logger::warn("true face: {} files for '{}', none used", found.size(), Player()->GetName());
			}
		}
		if (!s.disguised) {
			if (s.active.empty()) {
				ScheduleCheck("loaded a save", 150);
			}
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

	// dev "tintdump <name>": every colour mask of both lists, one per line:
	//   <list><pos> <type> <r> <g> <b> <a> <alpha> <texture>
	void DumpTints(std::string_view a_name)
	{
		auto*      p = Player();
		const auto path = TintFile(a_name);
		std::ofstream out(path, std::ios::trunc);
		const auto masks = AllMasks(p);
		for (const auto& lm : masks) {
			const auto* m = lm.mask;
			out << std::format("{}{} {} {} {} {} {} {:.6f} {}\n", lm.list, lm.pos, static_cast<std::uint32_t>(m->type.get()), m->color.red,
				m->color.green, m->color.blue, m->color.alpha, m->alpha, MaskTexture(m));
		}
		logger::info("tint dump: {} mask(s) ({} with colour) written to {}", masks.size(),
			std::ranges::count_if(masks, [](const ListedMask& m) { return m.mask->alpha > 0.0f; }), path.string());
		Shift::Notify(std::format("Protean: {} colour masks dumped ({}).", masks.size(), a_name));
	}

	// dev "tintrestore <name>": paint a dump back onto the player's own face (exact texture match), then redraw
	void RestoreTints(std::string_view a_name)
	{
		auto& s = State::Get();
		if (!s.active.empty() || s.disguised) {
			Shift::Notify("Protean: wear your own face in your own form first.");
			return;
		}
		const auto path = TintFile(a_name);
		auto       tints = ReadDump(a_name);
		if (tints.empty()) {
			Shift::Notify(std::format("Protean: no tint dump named {}.", a_name));
			return;
		}
		logger::info("=== tint restore from {}: {} layer(s) ===", path.string(), tints.size());
		WriteTintsExact(tints);
		g_pendingTints = tints;
		g_pendingExact = true;
		g_pendingRace = Id(Player()->GetRace());
		g_pendingFemale = Player()->GetActorBase()->actorData.actorBaseFlags.all(RE::ACTOR_BASE_DATA::Flag::kFemale);
		g_redraw = kRedrawDelay;
		Shift::Notify(std::format("Protean: {} colour layers painted from {}.", tints.size(), a_name));
	}

	void TrueFaceCommand(std::string_view a_cmd, std::string_view a_arg)
	{
		auto& s = State::Get();
		if (a_cmd == "truetake") {
			if (!OwnFormNow()) {
				Shift::Notify("Protean: wear your own face in your own form first.");
				return;
			}
			TakeTrueFace("dev command");
			Shift::Notify("Protean: this face is now the true face.");
		} else if (a_cmd == "truetints") {
			// the true face's colour layers from a tint dump (Mercer's from Save1, 2026-11-19), then a repair
			auto tints = ReadDump(a_arg);
			if (tints.empty() || !s.truth.valid) {
				Shift::Notify("Protean: no such dump, or no true face yet.");
				return;
			}
			std::uint8_t nth[kTintTypes]{};
			for (auto& t : tints) {
				t.index = t.type < kTintTypes ? nth[t.type]++ : 0;
			}
			s.truth.tints = std::move(tints);
			logger::info("true face colour layers replaced from dump '{}' ({} layer(s))", a_arg, s.truth.tints.size());
			WriteTrueFile(s.truth, s.truthId);
			g_checkWhy = "dev truetints";
			CheckTrueFace();
		} else if (a_cmd == "truecheck") {
			g_checkWhy = "dev command";
			CheckTrueFace();
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
