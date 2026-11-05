#include "Attacks.h"
#include "Camera.h"
#include "Claws.h"
#include "Consume.h"
#include "Deceiver.h"
#include "Devastator.h"
#include "Dev.h"
#include "Evolve.h"
#include "Feed.h"
#include "Face.h"
#include "Fly.h"
#include "Hooks.h"
#include "Journal.h"
#include "Metamorphosis.h"
#include "Provoke.h"
#include "Move.h"
#include "Registry.h"
#include "Shield.h"
#include "Shift.h"
#include "State.h"
#include "Tendril.h"
#include "Keen.h"
#include "Flight.h"
#include "Rush.h"
#include "Predator.h"
#include "Skills.h"
#include "Stalker.h"

namespace
{
	// Everything the game tells us arrives through these sinks. They only look and queue: the actual
	// work runs on the main thread through SKSE's task interface.
	class Sink : public RE::BSTEventSink<RE::TESEquipEvent>,
	             public RE::BSTEventSink<RE::TESSpellCastEvent>,
	             public RE::BSTEventSink<RE::TESMagicEffectApplyEvent>
	{
	public:
		static Sink* Get()
		{
			static Sink s;
			return &s;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
		{
			// the claw weapon going into or out of a hand (equipped or not): the claw pieces follow
			if (a_event && a_event->actor && a_event->actor->IsPlayerRef()) {
				auto& reg = protean::Registry::Get();
				if (reg.ClawWeapon() && a_event->baseObject == reg.ClawWeapon()->GetFormID()) {
					SKSE::GetTaskInterface()->AddTask([]() { protean::Claws::Sync(); });
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		// casting one of our powers
		RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* a_event, RE::BSTEventSource<RE::TESSpellCastEvent>*) override
		{
			if (a_event && a_event->object && a_event->object->IsPlayerRef()) {
				auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(a_event->spell);
				auto& reg = protean::Registry::Get();
				if (spell && (spell == reg.Revert() || spell == reg.Starter() || spell == reg.FacePower() || spell == reg.Evolution() || reg.ByPower(spell->GetFormID()))) {
					SKSE::GetTaskInterface()->AddTask([spell]() { protean::Shift::OnPowerCast(spell); });
				}
				if (spell && std::ranges::find(reg.KeenPowers(), spell) != reg.KeenPowers().end()) {
					SKSE::GetTaskInterface()->AddTask([spell]() { protean::Keen::OnPowerCast(spell); });
				}
				if (spell && spell == reg.DevastatorPower()) {
					SKSE::GetTaskInterface()->AddTask([]() { protean::Devastator::OnCast(); });
				}
				if (spell && spell == reg.RegenPower()) {
					SKSE::GetTaskInterface()->AddTask([]() { protean::Regeneration::OnCast(); });
				}
				if (spell && spell == reg.MetamorphosisPower()) {
					SKSE::GetTaskInterface()->AddTask([]() { protean::Metamorphosis::OnCast(); });
				}
				if (spell && spell == reg.InfectPower()) {
					SKSE::GetTaskInterface()->AddTask([]() { protean::Deceiver::OnInfectCast(); });
				}
				if (spell && spell == reg.StalkerPower()) {
					SKSE::GetTaskInterface()->AddTask([]() { protean::Stalker::OnPowerCast(); });
				}
				if (spell && spell == reg.StealthPower()) {
					SKSE::GetTaskInterface()->AddTask([]() { protean::Consume::OnStealthCast(); });
				}
				if (spell && spell == reg.Tendril()) {
					SKSE::GetTaskInterface()->AddTask([spell]() { protean::Tendril::OnCast(spell); });
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		// the Tendril's damage landing on someone: it may pull
		RE::BSEventNotifyControl ProcessEvent(const RE::TESMagicEffectApplyEvent* a_event, RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override
		{
			auto* tendril = protean::Registry::Get().Tendril();
			if (a_event && tendril && a_event->caster && a_event->caster->IsPlayerRef() && a_event->target &&
				a_event->magicEffect == tendril->effects[0]->baseEffect->GetFormID()) {
				if (auto* actor = a_event->target->As<RE::Actor>()) {
					const auto id = actor->GetFormID();
					SKSE::GetTaskInterface()->AddTask([id]() {
						if (auto* a = RE::TESForm::LookupByID<RE::Actor>(id)) {
							protean::Tendril::OnHit(a);
						}
					});
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	bool g_ready = false;

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kPostLoad:
			protean::Journal::Register();  // every plugin is loaded now, SKSEMenuFramework.dll among them
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			if (protean::Registry::Get().Load()) {
				auto* events = RE::ScriptEventSourceHolder::GetSingleton();
				events->AddEventSink<RE::TESEquipEvent>(Sink::Get());
				events->AddEventSink<RE::TESSpellCastEvent>(Sink::Get());
				events->AddEventSink<RE::TESMagicEffectApplyEvent>(Sink::Get());
				protean::Dev::Init();
				protean::Attacks::Install();
				protean::Move::Install();
				protean::Fly::Install();
				protean::Face::Install();
				protean::Hooks::Install();
				protean::Keen::Install();
				protean::Rush::Install();
				protean::Flight::Install();
				protean::Predator::Install();
				protean::Stalker::Install();
				protean::Consume::Install();
				protean::Skills::Install();
				protean::Provoke::Install();
				protean::Deceiver::Install();
				protean::Metamorphosis::Install();
				protean::Shield::Install();
				g_ready = true;
				logger::info("Protean is active");
			}
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			protean::Face::BeforeLoad();
			protean::Shift::BeforeLoad();
			protean::Camera::Reset();
			protean::Attacks::Cancel();
			protean::Move::Reset();
			protean::Keen::Reset();
			protean::Feed::Cancel();
			protean::Consume::Cancel();
			protean::Stalker::Reset();
			protean::Devastator::Reset();
			break;
		case SKSE::MessagingInterface::kNewGame:
			protean::Face::BeforeLoad();
			protean::Shift::BeforeLoad();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			if (g_ready) {
				protean::Face::OnGameLoaded();
				protean::Shift::OnGameLoaded();
				protean::Stalker::OnGameLoaded();
				protean::Skills::OnGameLoaded();
				protean::Registry::Get().SettleDials(protean::State::Get().dialDefaults);
				protean::Claws::Sync();
			}
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse, { .logName = "Protean" });  // -> Documents/My Games/<game>/SKSE/Protean.log
	spdlog::set_level(spdlog::level::info);
	spdlog::flush_on(spdlog::level::info);
	logger::info("Protean {} on runtime {}", "1.0.0", REL::Module::get().version().string());

	protean::Persist::Install();
	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		logger::critical("could not register for SKSE messages");
		return false;
	}
	return true;
}
