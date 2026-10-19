#include "Camera.h"

namespace protean::Camera
{
	namespace
	{
		constexpr auto kNodeName = "ProteanCam"sv;

		bool                    g_active{ false };
		float                   g_height{ 0.0f };
		int                     g_frames{ 0 };
		int                     g_restoreIn{ -1 };  // frames until we re-bind the engine's own node after a revert
		RE::NiPointer<RE::NiNode> g_node;           // held so the engine can never see a dangling pointer

		RE::ThirdPersonState* Tps()
		{
			auto* cam = RE::PlayerCamera::GetSingleton();
			if (!cam) {
				return nullptr;
			}
			return static_cast<RE::ThirdPersonState*>(cam->GetRuntimeData().cameraStates[RE::CameraStates::kThirdPerson].get());
		}

		void Describe(const char* a_tag, RE::PlayerCharacter* a_p, RE::ThirdPersonState* a_tps)
		{
			auto*       root = a_p->Get3D(false);
			const char* camName = (a_tps && a_tps->thirdPersonCameraObj) ? a_tps->thirdPersonCameraObj->name.c_str() : "(none)";
			float       camZ = (a_tps && a_tps->thirdPersonCameraObj) ? a_tps->thirdPersonCameraObj->world.translate.z : 0.0f;
			logger::info("[camera:{}] bound node '{}' worldZ {:.1f}; player Z {:.1f}; root '{}'", a_tag, camName, camZ, a_p->GetPositionZ(),
				root ? root->name.c_str() : "(none)");
			if (root) {
				if (auto* n = root->AsNode()) {
					std::string kids;
					for (auto& c : n->GetChildren()) {
						if (c) {
							kids += std::string(c->name.c_str()) + " | ";
						}
					}
					logger::info("[camera:{}] root children: {}", a_tag, kids);
				}
			}
		}
	}

	void Reset()
	{
		g_active = false;
		g_restoreIn = -1;
		g_frames = 0;
		g_node.reset();
	}

	void Begin(float a_height)
	{
		g_active = true;
		g_height = a_height;
		g_frames = 0;
		g_restoreIn = -1;
	}

	void End()
	{
		g_active = false;
		g_restoreIn = 20;  // give the engine time to reload the human 3D first
	}

	void OnFrame(RE::PlayerCharacter* a_p)
	{
		auto* tps = Tps();
		if (!tps || !a_p) {
			return;
		}

		if (g_restoreIn >= 0) {
			if (--g_restoreIn == 0) {
				g_restoreIn = -1;
				auto* root = a_p->Get3D(false);
				RE::NiAVObject* own = root ? root->GetObjectByName("Camera3rd [Cam3]"sv) : nullptr;
				Describe("restore", a_p, tps);
				if (own) {
					tps->thirdPersonCameraObj = own;
					logger::info("[camera] re-bound the engine's own Camera3rd node");
				} else {
					logger::warn("[camera] no Camera3rd node found after revert; toggling the camera to make the engine re-bind");
					if (auto* cam = RE::PlayerCamera::GetSingleton()) {
						cam->ForceFirstPerson();
						cam->ForceThirdPerson();
					}
				}
				g_node.reset();
			}
			return;
		}

		if (!g_active) {
			return;
		}
		auto* root = a_p->Get3D(false);
		auto* rootNode = root ? root->AsNode() : nullptr;
		if (!rootNode) {
			return;
		}

		// find or create our node under the current root (a race swap reloads the 3D)
		RE::NiAVObject* found = rootNode->GetObjectByName(kNodeName);
		if (!found) {
			auto* n = RE::NiNode::Create(0);
			n->name = kNodeName;
			n->local.translate = RE::NiPoint3{ 0.0f, 0.0f, g_height };
			rootNode->AttachChild(n, true);
			g_node.reset(n);
			found = n;
			logger::info("[camera] created '{}' at height {:.0f}", kNodeName, g_height);
		} else {
			found->local.translate.z = g_height;
		}

		if (g_frames == 90) {  // about a second in: record what the engine did, for the log
			Describe("transformed", a_p, tps);
		}
		++g_frames;
		if (tps->thirdPersonCameraObj != found) {
			tps->thirdPersonCameraObj = found;
		}
	}
}
