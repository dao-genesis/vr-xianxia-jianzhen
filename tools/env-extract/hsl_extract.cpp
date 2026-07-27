// hsl_extract — headless official-env → glTF 2.0 extractor (no Vulkan/GLFW).
// Reuses the project's reverse-engineered SceneLoader (RENDMESH/RENDTXTR/HSTF/scene.zip)
// and gltf_export to dump a modern (V205) Meta env APK to a Blender/three.js-ready glTF.
//
//   hsl_extract <env.apk> <outDir> [name] [companion.apk]
#include "loaders/scene_loader.h"
#include "io/gltf_export.h"
#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: hsl_extract <env.apk> <outDir> [name] [companion.apk]\n");
        return 2;
    }
    std::string apk = argv[1];
    std::string outDir = argv[2];
    std::string name = (argc > 3 && argv[3][0]) ? argv[3] : "env";

    SceneLoader loader;
    loader.verbose = (std::getenv("HSL_VERBOSE") != nullptr);
    if (!loader.load(apk)) {
        fprintf(stderr, "[extract] FATAL: scene load failed for %s\n", apk.c_str());
        return 1;
    }
    fprintf(stderr, "[extract] loaded %zu meshes from %s\n", loader.meshes.size(), apk.c_str());

    // Optional companion (e.g. vista backdrop merged with a home) at shared origin.
    if (argc > 4 && argv[4][0]) {
        SceneLoader comp;
        comp.verbose = (std::getenv("HSL_VERBOSE") != nullptr);
        if (comp.load(argv[4])) {
            size_t before = loader.meshes.size();
            for (auto& m : comp.meshes) loader.meshes.push_back(std::move(m));
            fprintf(stderr, "[extract] companion merged: +%zu (total %zu)\n",
                    loader.meshes.size() - before, loader.meshes.size());
        }
    }

    // Baked lightmap overrides (vista ships them) — best-effort, harmless if none.
    loader.applyLightmapOverrides(loader.meshes);

    bool ok = gltfexport::exportEnv(loader.meshes, outDir, name, "");
    fprintf(stderr, "[extract] %s: %zu meshes -> %s/%s.gltf\n",
            ok ? "OK" : "FAILED", loader.meshes.size(), outDir.c_str(), name.c_str());
    return ok ? 0 : 1;
}
