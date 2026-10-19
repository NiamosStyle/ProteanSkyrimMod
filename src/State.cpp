#include "State.h"

namespace protean
{
	namespace
	{
		constexpr std::uint32_t kUniqueId = 'PRTN';
		constexpr std::uint32_t kRecord = 'STAT';
		constexpr std::uint32_t kVersion = 2;

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
			logger::info("co-save written: {} known form(s), active '{}'", s.known.size(), s.active);
		}

		void OnLoad(SKSE::SerializationInterface* a)
		{
			auto& s = State::Get();
			s.Reset();
			std::uint32_t type = 0, version = 0, length = 0;
			while (a->GetNextRecordInfo(type, version, length)) {
				if (type != kRecord || version != kVersion) {
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
