#include "State.h"

namespace protean
{
	namespace
	{
		constexpr std::uint32_t kUniqueId = 'PRTN';
		constexpr std::uint32_t kRecord = 'STAT';
		constexpr std::uint32_t kVersion = 12;  // 3 to 11 are still read

		template <class T>
		void Put(SKSE::SerializationInterface* a, const T& v)
		{
			a->WriteRecordData(&v, sizeof(T));
		}

		void PutStr(SKSE::SerializationInterface* a, const std::string& s)
		{
			Put<std::uint32_t>(a, static_cast<std::uint32_t>(s.size()));
			if (!s.empty()) {
				a->WriteRecordData(s.data(), static_cast<std::uint32_t>(s.size()));
			}
		}

		template <class T>
		bool Get(SKSE::SerializationInterface* a, T& v)
		{
			return a->ReadRecordData(&v, sizeof(T)) == sizeof(T);
		}

		bool GetStr(SKSE::SerializationInterface* a, std::string& s)
		{
			std::uint32_t n = 0;
			if (!Get(a, n) || n > 4096) {
				return false;
			}
			s.assign(n, '\0');
			return n == 0 || a->ReadRecordData(s.data(), n) == n;
		}

		void PutForm(SKSE::SerializationInterface* a, RE::FormID id) { Put(a, id); }

		// FormIDs in a co-save are load-order dependent; this maps one back (0 if its plugin is gone).
		RE::FormID GetForm(SKSE::SerializationInterface* a)
		{
			RE::FormID raw = 0, out = 0;
			if (!Get(a, raw) || raw == 0) {
				return 0;
			}
			return a->ResolveFormID(raw, out) ? out : 0;
		}

		void PutLook(SKSE::SerializationInterface* a, const State::Look& l)
		{
			Put<std::uint8_t>(a, l.valid ? 1 : 0);
			PutForm(a, l.race);
			Put<std::uint8_t>(a, l.female ? 1 : 0);
			Put(a, l.height);
			Put(a, l.weight);
			a->WriteRecordData(l.body, 3);
			PutForm(a, l.hairColor);
			PutForm(a, l.faceDetails);
			PutForm(a, l.skin);
			PutForm(a, l.faceNpc);
			Put<std::uint32_t>(a, static_cast<std::uint32_t>(l.headParts.size()));
			for (auto f : l.headParts) {
				PutForm(a, f);
			}
			Put(a, l.morphs);
			Put(a, l.presets);
			Put<std::uint8_t>(a, l.hasFaceData ? 1 : 0);
			Put<std::uint32_t>(a, static_cast<std::uint32_t>(l.tints.size()));
			for (const auto& t : l.tints) {
				Put(a, t.type);
				Put(a, t.index);
				a->WriteRecordData(t.rgba, 4);
				Put(a, t.alpha);
				PutStr(a, t.texture);
			}
		}

		bool GetLook(SKSE::SerializationInterface* a, State::Look& l)
		{
			std::uint8_t b = 0;
			std::uint32_t n = 0;
			if (!Get(a, b)) {
				return false;
			}
			l.valid = b != 0;
			l.race = GetForm(a);
			Get(a, b);
			l.female = b != 0;
			Get(a, l.height);
			Get(a, l.weight);
			a->ReadRecordData(l.body, 3);
			l.hairColor = GetForm(a);
			l.faceDetails = GetForm(a);
			l.skin = GetForm(a);
			l.faceNpc = GetForm(a);
			if (!Get(a, n) || n > 64) {
				return false;
			}
			for (std::uint32_t i = 0; i < n; ++i) {
				if (auto f = GetForm(a)) {
					l.headParts.push_back(f);
				}
			}
			Get(a, l.morphs);
			Get(a, l.presets);
			Get(a, b);
			l.hasFaceData = b != 0;
			if (!Get(a, n) || n > 256) {
				return false;
			}
			for (std::uint32_t i = 0; i < n; ++i) {
				State::Tint t;
				Get(a, t.type);
				Get(a, t.index);
				a->ReadRecordData(t.rgba, 4);
				Get(a, t.alpha);
				if (!GetStr(a, t.texture)) {
					return false;
				}
				l.tints.push_back(std::move(t));
			}
			if (l.valid && !l.race) {
				l.valid = false;  // its plugin is gone
			}
			return true;
		}

		void OnSave(SKSE::SerializationInterface* a)
		{
			const auto& s = State::Get();
			if (!a->OpenRecord(kRecord, kVersion)) {
				logger::error("co-save: could not open record");
				return;
			}
			Put<std::uint32_t>(a, static_cast<std::uint32_t>(s.known.size()));
			for (const auto& k : s.known) {
				PutStr(a, k);
			}
			PutStr(a, s.active);
			PutForm(a, s.realRace);
			Put(a, s.healthBonus);
			Put<std::uint32_t>(a, static_cast<std::uint32_t>(s.worn.size()));
			for (const auto& w : s.worn) {
				PutForm(a, w.base);
				Put(a, w.uniqueId);
				Put<std::uint8_t>(a, w.left ? 1 : 0);
			}
			PutForm(a, s.leftHand);
			PutForm(a, s.rightHand);
			PutForm(a, s.power);
			Put<std::uint32_t>(a, static_cast<std::uint32_t>(s.addedFactions.size()));
			for (auto f : s.addedFactions) {
				PutForm(a, f);
			}
			Put<std::uint32_t>(a, static_cast<std::uint32_t>(s.witnesses.size()));
			for (auto f : s.witnesses) {
				PutForm(a, f);
			}
			Put<std::uint32_t>(a, static_cast<std::uint32_t>(s.hostileWitnesses.size()));
			for (auto f : s.hostileWitnesses) {
				PutForm(a, f);
			}
			Put<std::uint32_t>(a, static_cast<std::uint32_t>(s.looted.size()));
			for (auto f : s.looted) {
				PutForm(a, f);
			}
			Put<std::uint32_t>(a, static_cast<std::uint32_t>(s.moves.size()));
			for (const auto& [id, pair] : s.moves) {
				PutStr(a, id);
				PutStr(a, pair.first);
				PutStr(a, pair.second);
			}
			Put<std::uint8_t>(a, s.disguised ? 1 : 0);
			PutForm(a, s.bankedNpc);
			PutForm(a, s.ownRace);
			Put<std::uint8_t>(a, s.ownFemale ? 1 : 0);
			Put(a, s.ownHeight);
			Put(a, s.ownWeight);
			a->WriteRecordData(s.ownTint, 3);
			PutForm(a, s.ownFaceNpc);
			for (auto* list : { &s.disguiseFactions, &s.exposed }) {
				Put<std::uint32_t>(a, static_cast<std::uint32_t>(list->size()));
				for (auto f : *list) {
					PutForm(a, f);
				}
			}
			Put<std::uint32_t>(a, static_cast<std::uint32_t>(s.crime.size()));
			for (const auto& c : s.crime) {
				PutForm(a, c.faction);
				Put(a, c.violent);
				Put(a, c.nonViolent);
			}
			PutLook(a, s.own);
			PutLook(a, s.bank);
			PutForm(a, s.bodySkin);
			Put(a, s.bodyHeight);
			Put(a, s.sprintBoost);
			Put(a, s.armorBonus);
			Put(a, s.damageBonus);
			Put(a, s.evoTotal);
			Put(a, s.facePoints);
			Put<std::uint32_t>(a, static_cast<std::uint32_t>(s.feeds.size()));
			for (const auto& [id, n] : s.feeds) {
				PutStr(a, id);
				Put(a, n);
			}
			Put<std::uint32_t>(a, static_cast<std::uint32_t>(s.boosts.size()));
			for (float t : s.boosts) {
				Put(a, t);
			}
			Put(a, s.boostApplied);
			Put(a, s.diseaseApplied);
			Put(a, s.rushApplied);
			logger::info("co-save written: {} known form(s), active '{}'", s.known.size(), s.active);
		}

		void OnLoad(SKSE::SerializationInterface* a)
		{
			auto& s = State::Get();
			s.Reset();
			std::uint32_t type = 0, version = 0, length = 0;
			while (a->GetNextRecordInfo(type, version, length)) {
				if (type != kRecord || version < 3 || version > kVersion) {
					logger::warn("co-save: skipping unknown record");
					continue;
				}
				std::uint32_t n = 0;
				if (!Get(a, n) || n > 1000) {
					return;
				}
				for (std::uint32_t i = 0; i < n; ++i) {
					std::string k;
					if (!GetStr(a, k)) {
						return;
					}
					s.known.insert(k);
				}
				if (!GetStr(a, s.active)) {
					return;
				}
				s.realRace = GetForm(a);
				Get(a, s.healthBonus);
				if (!Get(a, n) || n > 1000) {
					return;
				}
				for (std::uint32_t i = 0; i < n; ++i) {
					WornItem w;
					w.base = GetForm(a);
					Get(a, w.uniqueId);
					std::uint8_t l = 0;
					Get(a, l);
					w.left = l != 0;
					if (w.base) {
						s.worn.push_back(w);
					}
				}
				s.leftHand = GetForm(a);
				s.rightHand = GetForm(a);
				s.power = GetForm(a);
				for (auto* list : { &s.addedFactions, &s.witnesses, &s.hostileWitnesses }) {
					if (!Get(a, n) || n > 100000) {
						return;
					}
					for (std::uint32_t i = 0; i < n; ++i) {
						if (auto f = GetForm(a)) {
							list->push_back(f);
						}
					}
				}
				if (!Get(a, n) || n > 1000000) {
					return;
				}
				for (std::uint32_t i = 0; i < n; ++i) {
					if (auto f = GetForm(a)) {
						s.looted.insert(f);
					}
				}
				if (Get(a, n) && n <= 1000) {
					for (std::uint32_t i = 0; i < n; ++i) {
						std::string id, l, r;
						if (!GetStr(a, id) || !GetStr(a, l) || !GetStr(a, r)) {
							break;
						}
						s.moves[id] = { l, r };
					}
				}
				if (version >= 4) {
					std::uint8_t b = 0;
					Get(a, b);
					s.disguised = b != 0;
					s.bankedNpc = GetForm(a);
					s.ownRace = GetForm(a);
					Get(a, b);
					s.ownFemale = b != 0;
					Get(a, s.ownHeight);
					Get(a, s.ownWeight);
					a->ReadRecordData(s.ownTint, 3);
					s.ownFaceNpc = GetForm(a);
					for (auto* list : { &s.disguiseFactions, &s.exposed }) {
						if (!Get(a, n) || n > 100000) {
							break;
						}
						for (std::uint32_t i = 0; i < n; ++i) {
							if (auto f = GetForm(a)) {
								list->push_back(f);
							}
						}
					}
					if (version >= 5 && Get(a, n) && n <= 100000) {
						for (std::uint32_t i = 0; i < n; ++i) {
							State::CrimeSnap c;
							c.faction = GetForm(a);
							Get(a, c.violent);
							Get(a, c.nonViolent);
							if (c.faction) {
								s.crime.push_back(c);
							}
						}
					}
					if (version >= 6 && !(GetLook(a, s.own) && GetLook(a, s.bank))) {
						logger::warn("co-save: the saved looks could not be read");
					}
					if (version >= 7) {
						s.bodySkin = GetForm(a);
						Get(a, s.bodyHeight);
					}
					if (version >= 8) {
						Get(a, s.sprintBoost);
					}
					if (version >= 9) {
						Get(a, s.armorBonus);
						Get(a, s.damageBonus);
					}
					if (version >= 10) {
						Get(a, s.evoTotal);
						Get(a, s.facePoints);
						// v10 also kept a list of bought perks; since v11 they are the player's real perks
						std::map<std::string, std::uint32_t> oldPerks;
						for (auto* m : { &s.feeds, &oldPerks }) {
							if (version < 11 || m == &s.feeds) {
								if (!Get(a, n) || n > 10000) {
									return;
								}
								for (std::uint32_t i = 0; i < n; ++i) {
									std::string   id;
									std::uint32_t v = 0;
									if (!GetStr(a, id) || !Get(a, v)) {
										return;
									}
									(*m)[id] = v;
								}
							}
						}
						if (!Get(a, n) || n > 10000) {
							return;
						}
						for (std::uint32_t i = 0; i < n; ++i) {
							float t = 0.0f;
							Get(a, t);
							s.boosts.push_back(t);
						}
						Get(a, s.boostApplied);
						Get(a, s.diseaseApplied);
					}
					if (version >= 12) {
						Get(a, s.rushApplied);
					}
				}
			}
			logger::info("co-save read: {} known form(s), active '{}'", s.known.size(), s.active);
		}

		void OnRevert(SKSE::SerializationInterface*)
		{
			State::Get().Reset();
		}
	}

	State& State::Get()
	{
		static State s;
		return s;
	}

	void State::Reset()
	{
		*this = State{};
	}

	namespace Persist
	{
		void Install()
		{
			auto* ser = SKSE::GetSerializationInterface();
			ser->SetUniqueID(kUniqueId);
			ser->SetSaveCallback(OnSave);
			ser->SetLoadCallback(OnLoad);
			ser->SetRevertCallback(OnRevert);
		}
	}
}
