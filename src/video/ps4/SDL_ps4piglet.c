//
// Created by cpasjuste on 31/12/2021.
//
#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_OPENGL_EGL

#include <stdbool.h>
#include <orbis/libkernel.h>
#include <orbis/Pigletv2VSH.h>

#include "SDL_hints.h"
#include "SDL_error.h"
#include "SDL_ps4piglet.h"
#include "SDL_ps4video.h"

#define PIGLET_MODULE_NAME "libScePigletv2VSH.sprx"
#define SHACC_MODULE_NAME "libSceShaccVSH.sprx"

typedef void module_patch_cb_t(uint8_t *base);

OrbisKernelModule PS4_PigletModId;
static OrbisKernelModule shaccModId;

/* XXX: patches below are given for Piglet module from 4.74 Devkit PUP */
static void pgl_patches_cb(uint8_t *base)
{
    /* Patch runtime compiler check */
    const uint8_t p_set_eax_to_1[] = {
            0x31, 0xC0, 0xFF, 0xC0, 0x90,
    };
    memcpy(base + 0x5451F, p_set_eax_to_1, sizeof(p_set_eax_to_1));

    /* Tell that runtime compiler exists */
    *(uint8_t *) (base + 0xB2DEC) = 0;
    *(uint8_t *) (base + 0xB2DED) = 0;
    *(uint8_t *) (base + 0xB2DEE) = 1;
    *(uint8_t *) (base + 0xB2E21) = 1;

    /* Inform Piglet that we have shader compiler module loaded */
    *(int32_t *) (base + 0xB2E24) = shaccModId;
}

static unsigned int sceKernelGetModuleInfoByName(const char *name, OrbisKernelModuleInfo *info)
{
    OrbisKernelModuleInfo tmpInfo;
    OrbisKernelModule handles[256];
    size_t numModules;
    size_t i;
    int ret;

    LOG_DEBUG_PS4_VIDEO("sceKernelGetModuleInfoByName(%s)", name);

    if (!name || !info) {
        return 0x8002000E;
    }

    memset(handles, 0, sizeof(handles));

    ret = sceKernelGetModuleList(handles, SDL_arraysize(handles), &numModules);
    if (ret) {
        SDL_SetError("sceKernelGetModuleInfoByName: sceKernelGetModuleList failed (0x%08x)", ret);
        return ret;
    }

    LOG_DEBUG_PS4_VIDEO("sceKernelGetModuleInfoByName: found %zu modules", numModules);

    for (i = 0; i < numModules; ++i) {
        memset(&tmpInfo, 0, sizeof(tmpInfo));
        tmpInfo.size = sizeof(tmpInfo);
        ret = sceKernelGetModuleInfo(handles[i], &tmpInfo);
        if (ret) {
            SDL_SetError("sceKernelGetModuleInfoByName: sceKernelGetModuleInfo[%zu] failed (0x%08x)", i, ret);
            return ret;
        }

        LOG_DEBUG_PS4_VIDEO("sceKernelGetModuleInfoByName: [%zu]: %s", i, tmpInfo.name);

        if (strcmp(tmpInfo.name, name) == 0) {
            LOG_DEBUG_PS4_VIDEO("sceKernelGetModuleInfoByName: piglet module found: %s", tmpInfo.name);
            memcpy(info, &tmpInfo, sizeof(tmpInfo));
            return 0;
        }
    }

    return 0x80020002;
}

static unsigned int shaderCompilerGetModuleBase(const char *name, uint64_t *base, uint64_t *size)
{
    OrbisKernelModuleInfo moduleInfo;
    unsigned int ret;

    LOG_DEBUG_PS4_VIDEO("shaderCompilerGetModuleBase(%s)", name);

    ret = sceKernelGetModuleInfoByName(name, &moduleInfo);
    if (ret) {
        SDL_SetError("shaderCompilerGetModuleBase: sceKernelGetModuleInfoByName(%s) failed: 0x%08X", name, ret);
        return ret;
    }

    if (base) {
        *base = (uint64_t) moduleInfo.segmentInfo[0].address;
    }
    if (size) {
        *size = moduleInfo.segmentInfo[0].size;
    }

    return 0;
}

static int shaderCompilerPatchModule(const char *name, module_patch_cb_t *cb)
{
    uint64_t base, size;
    int ret;

    if (shaderCompilerGetModuleBase(name, &base, &size) != 0) {
        SDL_SetError("shaderCompilerPatchModule: getModuleBase return error");
        return 1;
    }

    LOG_DEBUG_PS4_VIDEO("shaderCompilerPatchModule: module base=0x%08lX size=%ld", base, size);

    ret = sceKernelMprotect((void *) base, size, ORBIS_KERNEL_PROT_CPU_ALL);
    if (ret) {
        SDL_SetError("shaderCompilerPatchModule: sceKernelMprotect(%s) failed: 0x%08X", name, ret);
        return 1;
    }

    LOG_DEBUG_PS4_VIDEO("shaderCompilerPatchModule: patching module");

    if (cb) {
        (*cb)((uint8_t *) base);
    }

    LOG_DEBUG_PS4_VIDEO("shaderCompilerPatchModule: patching module done");

    return 0;
}

int PS4_PigletInit()
{
    LOG_DEBUG_PS4_VIDEO("PS4_PigletInit");
    char module_path[512];
    OrbisPglConfig ps4_pgl_config;

    // load piglet and shader compiler module from specified path if requested
    // else load from piglet from device without shader compiler support
    const char *path = SDL_GetHint(SDL_HINT_PS4_PIGLET_MODULES_PATH);
    if (path) {
        snprintf(module_path, sizeof(module_path), "%s/%s", path, PIGLET_MODULE_NAME);
        LOG_DEBUG_PS4_VIDEO("PS4_PigletInit: loading piglet module from: %s", module_path);
        PS4_PigletModId = sceKernelLoadStartModule(module_path, 0, NULL, 0, NULL, NULL);
        if (!PS4_PigletModId) {
            SDL_SetError("PS4_PigletInit: could not load piglet module %s", module_path);
            return 1;
        }
        snprintf(module_path, sizeof(module_path), "%s/%s", path, SHACC_MODULE_NAME);
        LOG_DEBUG_PS4_VIDEO("PS4_PigletInit: loading shacc module from: %s", module_path);
        shaccModId = sceKernelLoadStartModule(module_path, 0, NULL, 0, NULL, NULL);
        if (!shaccModId) {
            SDL_SetError("PS4_PigletInit: could not load shacc module %s", module_path);
            return 1;
        }
        if (shaderCompilerPatchModule(PIGLET_MODULE_NAME, &pgl_patches_cb) != 0) {
            sceKernelStopUnloadModule(shaccModId, 0, NULL, 0, NULL, NULL);
            shaccModId = 0;
            SDL_SetError("PS4_PigletInit: unable to patch piglet module.");
            return 1;
        }
    } else {
        snprintf(module_path, sizeof(module_path), "/%s/common/lib/libScePigletv2VSH.sprx", sceKernelGetFsSandboxRandomWord());
        LOG_DEBUG_PS4_VIDEO("PS4_PigletInit: loading piglet module from: %s", module_path);
        PS4_PigletModId = sceKernelLoadStartModule(module_path, 0, NULL, 0, NULL, NULL);
        if (!PS4_PigletModId) {
            SDL_SetError("PS4_PigletInit: could not load piglet module %s", module_path);
            return 1;
        }
    }

    SDL_memset(&ps4_pgl_config, 0, sizeof(ps4_pgl_config));
    ps4_pgl_config.size = sizeof(ps4_pgl_config);
    ps4_pgl_config.flags = 0;
    ps4_pgl_config.systemSharedMemorySize = 256 * 1024 * 1024;
    ps4_pgl_config.videoSharedMemorySize = 256 * 1024 * 1024;
    ps4_pgl_config.drawCommandBufferSize = 1 * 1024 * 1024;
    ps4_pgl_config.lcueResourceBufferSize = 1 * 1024 * 1024;
    ps4_pgl_config.dbgPosCmd_0x40 = 1920;
    ps4_pgl_config.dbgPosCmd_0x44 = 1080;
    ps4_pgl_config.unk_0x5C = 2;

    if (!scePigletSetConfigurationVSH(&ps4_pgl_config)) {
        SDL_SetError("PS4_PigletInit: scePigletSetConfigurationVSH failed");
        return 1;
    }

    LOG_DEBUG_PS4_VIDEO("PS4_PigletInit: Ok");

    return 0;
}

void PS4_PigletExit()
{
    if (shaccModId) {
        sceKernelStopUnloadModule(shaccModId, 0, NULL, 0, NULL, NULL);
        shaccModId = 0;
    }
    if (PS4_PigletModId) {
        sceKernelStopUnloadModule(PS4_PigletModId, 0, NULL, 0, NULL, NULL);
        PS4_PigletModId = 0;
    }
}

SDL_bool PS4_PigletShaccAvailable()
{
    return shaccModId ? SDL_TRUE : SDL_FALSE;
}

#endif // SDL_VIDEO_OPENGL_EGL
