#include "core/PluginModuleAbi.hpp"
#include "core/PluginHost.hpp"
#include "core/PluginTypes.hpp"

#include "MemorySignature.hpp"
#include "ctx.hpp"
#include <frida-gum.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

using namespace ds::plugin;

constexpr std::array<uint8_t, 5> kPatch5{0xB0, 0x01, 0xC3, 0x90, 0x90};
constexpr std::array<uint8_t, 6> kPatch6{0xB0, 0x01, 0xC3, 0x90, 0x90, 0x90};

struct SignaturePatch {
    const char *name;
    const char *pattern;
    const uint8_t *replacement;
    size_t replacement_size;
};

constexpr std::array<SignaturePatch, 4> kPatches{{
    {"sig1", "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 49 8B E8 48 8B F2 48 8B F9 E8",
     kPatch5.data(), kPatch5.size()},
    {"sig2", "40 53 48 83 EC ?? 4C 8B 81 ?? ?? ?? ?? 48",
     kPatch6.data(), kPatch6.size()},
    {"sig3", "48 89 5C 24 ?? 48 89 54 24 ?? 48 89 4C 24 ?? 55 56 57 48 83 EC ?? 48 8B F1",
     kPatch5.data(), kPatch5.size()},
    {"sig4", "48 89 5C 24 ?? 57 48 83 EC ?? 48 8D 4C 24 ?? 48 8B FA E8",
     kPatch5.data(), kPatch5.size()},
}};

struct PatchData {
    const uint8_t *bytes;
    size_t size;
};

void apply_patch(gpointer writable_memory, gpointer user_data) {
    const auto *patch = static_cast<const PatchData *>(user_data);
    std::memcpy(writable_memory, patch->bytes, patch->size);
}

struct SkinsVerifyPatchPlugin final : IPlugin {
    PluginManifest man{};

    SkinsVerifyPatchPlugin() {
        man.id = "skins_verify_patch";
        man.version = "1.0.0";
        man.phases = PluginPhase::EarlyNative;
        man.support_reload = false;
        man.priority = 55;
        man.options.kind = OptionRuleKind::AlwaysOn;
    }

    const PluginManifest &manifest() const override { return man; }

    bool can_load(const PluginContext &) const override {
#ifdef _WIN32
        return true;
#else
        return false;
#endif
    }

    void load(PluginContext &) override {
        if (!function_relocation::init_ctx()) {
            std::fprintf(stderr, "[skins_verify_patch] failed to initialize signature scanner\n");
            return;
        }

        size_t patched_count = 0;
        for (const auto &entry : kPatches) {
            function_relocation::MemorySignature signature{entry.pattern, 0, false};
            signature.prot_flag = GUM_PAGE_EXECUTE;
            signature.log = false;
            if (signature.scan(nullptr) == 0 || signature.targets.empty()) {
                std::fprintf(stderr, "[skins_verify_patch] %s signature not found\n", entry.name);
                continue;
            }

            PatchData patch{entry.replacement, entry.replacement_size};
            auto *address = reinterpret_cast<gpointer>(signature.targets.front());
            if (!gum_memory_patch_code(address, entry.replacement_size, apply_patch,
                                       &patch)) {
                std::fprintf(stderr, "[skins_verify_patch] failed to patch %s at %p\n",
                             entry.name, address);
                continue;
            }

            ++patched_count;
            std::fprintf(stderr, "[skins_verify_patch] patched %s at %p\n", entry.name, address);
        }

        std::fprintf(stderr, "[skins_verify_patch] patched %zu/4 signatures\n", patched_count);
    }

    void unload(PluginContext &) override {
        // Code patches remain active for the process lifetime.
    }
};

SkinsVerifyPatchPlugin g_plugin;

} // namespace

DS_PLUGIN_MODULE_EXPORT const char *ds_plugin_module_abi_version() {
    return DS_PLUGIN_ABI_VERSION;
}

DS_PLUGIN_MODULE_EXPORT bool ds_plugin_module_init(ds::plugin::PluginHost *host) {
    if (!host) {
        return false;
    }
    host->register_plugin(&g_plugin);
    std::fprintf(stderr, "[skins_verify_patch] module init registered skins_verify_patch\n");
    return true;
}
