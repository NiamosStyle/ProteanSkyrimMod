#pragma once

// CommonLibSSE-NG headers are PCH-dependent (see extern/CommonLibSSE-NG/PREBUILT.md).
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

using namespace std::literals;

namespace logger = SKSE::log;
