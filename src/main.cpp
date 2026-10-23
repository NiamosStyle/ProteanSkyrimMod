#include "Attacks.h"
#include "Camera.h"
#include "Dev.h"
#include "Evolve.h"
#include "Face.h"
#include "Fly.h"
#include "Hooks.h"
#include "Move.h"
#include "Registry.h"
#include "Shift.h"
#include "State.h"

namespace
{
	// Everything the game tells us arrives through these sinks. They only look and queue: the actual
	// work runs on the main thread through SKSE's task interface.
	class Sink : public RE::BSTEventSink<RE::TESEquipEvent>,
	             public RE::BSTEventSink<RE::TESSpellCastEvent>,
	             public RE::BSTEventSink<RE::TESActivateEvent>,
             public RE::BSTEventSink<RE::TESDeathEvent>
	{
	public:
		static Sink* Get()
		{
			static Sink s;
			return &s;
		}

		// eating a heart
		RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
		{
			if (a_event && a_event->equipped && a_event->actor && a_event->actor->IsPlayerRef()) {
				if (const auto* c = protean::Registry::Get().ByHeart(a_event->baseObject)) {
					SKSE::GetTaskInterface()->AddTask([c]() { protean::Shift::Learn(*c); });
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
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		// a kill by the player: a stack of the Unbound Form boost
		RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override
		{
			if (a_event && a_event->dead && a_event->actorKiller && a_event->actorKiller->IsPlayerRef() && a_event->actorDying &&
				!a_event->actorDying->IsPlayerRef()) {
				SKSE::GetTaskInterface()->AddTask([]() { protean::Evolve::OnKill(); });
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		// opening a dead animal
		RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* a_event, RE::BSTEventSource<RE::TESActivateEvent>*) override
		{
			if (a_event && a_event->actionRef && a_event->actionRef->IsPlayerRef() && a_event->objectActivated) {
				if (auto* actor = a_event->objectActivated->As<RE::Actor>(); actor && actor->IsDead()) {
					const auto id = actor->GetFormID();
					SKSE::GetTaskInterface()->AddTask([id]() {
						if (auto* a = RE::TESForm::LookupByID<RE::Actor>(id)) {
							protean::Shift::OnCorpseActivated(a);
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
			protean::Face::Register();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			if (protean::Registry::Get().Load()) {
				auto* events = RE::ScriptEventSourceHolder::GetSingleton();
				events->AddEventSink<RE::TESEquipEvent>(Sink::Get());
				events->AddEventSink<RE::TESSpellCastEvent>(Sink::Get());
				events->AddEventSink<RE::TESActivateEvent>(Sink::Get());
				events->AddEventSink<RE::TESDeathEvent>(Sink::Get());
				protean::Dev::Init();
				protean::Attacks::Install();
				protean::Move::Install();
				protean::Fly::Install();
				protean::Face::Install();
				protean::Hooks::Install();
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
			break;
		case SKSE::MessagingInterface::kNewGame:
			protean::Face::BeforeLoad();
			protean::Shift::BeforeLoad();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			if (g_ready) {
				protean::Face::OnGameLoaded();
				protean::Shift::OnGameLoaded();
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
