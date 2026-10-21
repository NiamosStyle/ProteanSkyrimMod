#include "Camera.h"

namespace protean::Camera
{
	namespace
	{
		constexpr auto kNodeName = "ProteanCam"sv;
		constexpr auto kIni = "Data/SKSE/Plugins/Protean.ini";
		constexpr float kBigRadius = 180.0f;  // model radius above which the camera moves back (horse 143, giant 156)
		constexpr float kBigBack = 2.5f;      // per unit of radius above that (1.0 left the mammoth's too close)

		// Tuning, re-read from Protean.ini at every transform so it can be adjusted without restarting.
		struct Tuning
		{
			float heightScale{ 1.0f };  // multiplies the camera height (measured from the model, or from the table)
			bool  autoHeight{ true };   // measure the animal's loaded model; false = use the table value only
			float topFactor{ 0.9f };    // camera height = model centre + this * bounding radius
			float shoulderX{ 0.0f };    // engine over-the-shoulder offset: 30 = right shoulder, 0 = centred, negative = left
			float shoulderZ{ 0.0f };    // extra vertical engine offset
			float sideOffset{ 0.0f };   // our camera node, sideways in the animal's space (+ = right)
			float backOffset{ 0.0f };   // our camera node, forwards/back in the animal's space (+ = forward)
			float zoom{ 0.35f };        // starting camera distance (0 = closest); the wheel still works
		};

		Tuning g_tune;
		bool   g_active{ false };
		float  g_height{ 0.0f };
		int    g_frames{ 0 };
		int    g_restoreIn{ -1 };  // frames until we re-bind the engine's own node after a revert
		RE::NiPointer<RE::NiNode> g_node;  // held so the engine can never see a dangling pointer

		void LoadTuning()
		{
			g_tune = Tuning{};
			std::ifstream in(kIni);
			std::string   line;
			while (std::getline(in, line)) {
				if (line.empty() || line[0] == ';' || line[0] == '[') {
					continue;
				}
				const auto eq = line.find('=');
				if (eq == std::string::npos) {
					continue;
				}
				const std::string key = line.substr(0, eq);
				float             v = 0.0f;
				try {
					v = std::stof(line.substr(eq + 1));
				} catch (...) {
					continue;
				}
				if (key == "HeightScale") {
					g_tune.heightScale = v;
				} else if (key == "ShoulderX") {
					g_tune.shoulderX = v;
				} else if (key == "ShoulderZ") {
					g_tune.shoulderZ = v;
				} else if (key == "SideOffset") {
					g_tune.sideOffset = v;
				} else if (key == "BackOffset") {
					g_tune.backOffset = v;
				} else if (key == "AutoHeight") {
					g_tune.autoHeight = v != 0.0f;
				} else if (key == "TopFactor") {
					g_tune.topFactor = v;
				} else if (key == "Zoom") {
					g_tune.zoom = v;
				}
			}
			logger::info("[camera] tuning: heightScale {} auto {} topFactor {} shoulderX {} shoulderZ {} side {} back {} zoom {}", g_tune.heightScale,
				g_tune.autoHeight, g_tune.topFactor, g_tune.shoulderX, g_tune.shoulderZ, g_tune.sideOffset, g_tune.backOffset, g_tune.zoom);
		}

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
			if (a_tps) {
				logger::info("[camera:{}] zoom {:.2f}/{:.2f} offsets actual ({:.1f},{:.1f},{:.1f})", a_tag, a_tps->currentZoomOffset, a_tps->targetZoomOffset,
					a_tps->posOffsetActual.x, a_tps->posOffsetActual.y, a_tps->posOffsetActual.z);
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
		LoadTuning();
		g_active = true;
		g_height = a_height;  // the table's value: used until (or unless) the model can be measured
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
				auto*           root = a_p->Get3D(false);
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
		// Height: measure the loaded model once it has settled (the bounding sphere is empty for the first
		// frames after the race swap), so any animal, including modded ones, gets a camera above its back.
		float base = g_height;
		if (g_tune.autoHeight && g_frames >= 20 && root->worldBound.radius > 1.0f) {
			base = (root->worldBound.center.z - root->world.translate.z) + root->worldBound.radius * g_tune.topFactor;
			if (g_frames == 20) {
				logger::info("[camera] measured model: centre dz {:.1f} radius {:.1f} -> height {:.1f} (table said {:.1f})",
					root->worldBound.center.z - root->world.translate.z, root->worldBound.radius, base, g_height);
			}
		}
		// A big body (the mammoth, radius ~370) hides the view at the usual distance: move the camera back by
		// how much the model is bigger than a horse-sized one.
		float back = g_tune.backOffset;
		if (g_tune.autoHeight && g_frames >= 20 && root->worldBound.radius > kBigRadius) {
			back -= (root->worldBound.radius - kBigRadius) * kBigBack;
		}
		const RE::NiPoint3 where{ g_tune.sideOffset, back, base * g_tune.heightScale };
		RE::NiAVObject*    found = rootNode->GetObjectByName(kNodeName);
		if (!found) {
			auto* n = RE::NiNode::Create(0);
			n->name = kNodeName;
			n->local.translate = where;
			rootNode->AttachChild(n, true);
			g_node.reset(n);
			found = n;
			logger::info("[camera] created '{}' at ({:.0f},{:.0f},{:.0f})", kNodeName, where.x, where.y, where.z);
		} else {
			found->local.translate = where;
		}
		if (tps->thirdPersonCameraObj != found) {
			tps->thirdPersonCameraObj = found;
		}

		// the engine's shoulder offset: centre it (written after the engine's own update each frame)
		tps->posOffsetExpected.x = tps->posOffsetActual.x = g_tune.shoulderX;
		tps->posOffsetExpected.z = tps->posOffsetActual.z = g_tune.shoulderZ;

		// Start at our own distance. Done a few times in the first half second because switching from first
		// person plays a camera transition that rewrites the zoom; the wheel is free afterwards.
		if (g_frames == 5 || g_frames == 15 || g_frames == 30) {
			if (auto* cam = RE::PlayerCamera::GetSingleton(); cam && !cam->IsInThirdPerson()) {
				cam->ForceThirdPerson();
			}
			tps->targetZoomOffset = tps->currentZoomOffset = g_tune.zoom;
		}
		if (g_frames == 90) {
			Describe("transformed", a_p, tps);
		}
		++g_frames;
	}
}
