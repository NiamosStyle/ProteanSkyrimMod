cmake_minimum_required(VERSION 3.21)

project(
	Protean
	VERSION 1.0.0
	LANGUAGES CXX
)

# Same recipe as the Portal Spheres project: CommonLibSSE-NG (alandtse) at tag v12.1.0 built from source
# (the prebuilt bundle needs a newer MSVC than VS 2022 ships), static vcpkg libs (triplet x64-windows-static-md).

set(PROTEAN_DEPLOY_DIR "" CACHE PATH "Optional: copy the built DLL + data here (e.g. an MO2 mod folder)")

set(BUILD_TESTS OFF CACHE BOOL "CommonLib unit tests" FORCE)
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG" commonlib EXCLUDE_FROM_ALL)
include("${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG/cmake/CommonLibSSE.cmake")  # only auto-included on the prebuilt path

add_commonlibsse_plugin(
	${PROJECT_NAME}
	NAME "Protean"
	AUTHOR "NiamosStyle"
	VERSION ${PROJECT_VERSION}
	SOURCES
		src/main.cpp
		src/Attacks.cpp
		src/Camera.cpp
		src/Claws.cpp
		src/Consume.cpp
		src/Dev.cpp
		src/Evolve.cpp
		src/Face.cpp
		src/Fly.cpp
		src/Hooks.cpp
		src/Hud.cpp
		src/Move.cpp
		src/Registry.cpp
		src/State.cpp
		src/Shift.cpp
		src/Tendril.cpp
		src/Keen.cpp
		src/Rush.cpp
		src/Flight.cpp
		src/Carapace.cpp
		src/Predator.cpp
		src/Feed.cpp
		src/Stalker.cpp
		src/Skills.cpp
		src/Journal.cpp
		src/Provoke.cpp
		src/Deceiver.cpp
		src/Metamorphosis.cpp
		src/Devastator.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# SKSE Menu Framework API header (LGPL-2.1, header-only; resolves its DLL at runtime). See extern/SKSEMenuFramework/SOURCE.txt.
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/extern/SKSEMenuFramework")
target_compile_options(${PROJECT_NAME} PRIVATE /external:W0)
target_precompile_headers(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/PCH.h")
target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8 /Zc:preprocessor)
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)  # windows.h min/max macros break std::min/max

if(PROTEAN_DEPLOY_DIR)
	add_custom_command(
		TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROTEAN_DEPLOY_DIR}/SKSE/Plugins"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${PROTEAN_DEPLOY_DIR}/SKSE/Plugins/"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/data/Protean.esp" "${PROTEAN_DEPLOY_DIR}/"
		COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data/SKSE" "${PROTEAN_DEPLOY_DIR}/SKSE"
		COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data/meshes" "${PROTEAN_DEPLOY_DIR}/meshes"
		COMMENT "Deploying Protean to ${PROTEAN_DEPLOY_DIR}"
		VERBATIM
	)
endif()
