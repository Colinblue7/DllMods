﻿#include "Configuration.h"
#include "FxaaRenderer.h"

const std::array<const char*, 7> FxaaRenderer::SHADER_NAMES =
{
    "FxFXAALite",
    "FxFXAA_0",
    "FxFXAA_1",
    "FxFXAA_2",
    "FxFXAA_3",
    "FxFXAA_4",
    "FxFXAA_5",
};

//
// FxPipeline
// 

hh::mr::SShaderPair fxaaShaderPair;
boost::shared_ptr<hh::ygg::CYggTexture> fxaaFrameBuffer;

HOOK(void*, __fastcall, InitializeCrossFade, 0x10C21A0, Sonic::CFxJob* This)
{
    This->m_pScheduler->GetShader(fxaaShaderPair, "FxFilterNone", FxaaRenderer::SHADER_NAMES[(uint32_t)Configuration::fxaaIntensity - 1]);
    This->m_pScheduler->m_pMisc->m_pDevice->CreateTexture(fxaaFrameBuffer, 1.0f, 1.0f, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, NULL);

    return originalInitializeCrossFade(This);
}

HOOK(void*, __fastcall, ExecuteCrossFade, 0x10C22D0, Sonic::CFxJob* This)
{
    void* result = originalExecuteCrossFade(This);

    if (!fxaaShaderPair.m_spVertexShader || !fxaaShaderPair.m_spPixelShader || !fxaaFrameBuffer)
        return result;

    boost::shared_ptr<hh::ygg::CYggTexture> frameBuffer;
    This->GetDefaultTexture(frameBuffer);

    boost::shared_ptr<hh::ygg::CYggSurface> surface;
    fxaaFrameBuffer->GetSurface(surface, 0, 0);

    This->m_pScheduler->m_pMisc->m_pDevice->SetRenderTarget(0, surface);
    This->m_pScheduler->m_pMisc->m_pDevice->SetShader(fxaaShaderPair);

    This->m_pScheduler->m_pMisc->m_pDevice->SetTexture(0, frameBuffer);
    This->m_pScheduler->m_pMisc->m_pDevice->SetSamplerFilter(0, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_NONE);
    This->m_pScheduler->m_pMisc->m_pDevice->SetSamplerAddressMode(0, D3DTADDRESS_CLAMP);

    This->m_pScheduler->m_pMisc->m_pDevice->DrawQuad2D(nullptr, 0, 0);

    This->SetDefaultTexture(fxaaFrameBuffer);

    return result;
}

bool FxaaRenderer::enabled = false;

void FxaaRenderer::applyPatches()
{
    if (enabled)
        return;

    enabled = true;

    // Ignore Devil's Details' FXAA shader
    WRITE_NOP(0x64CC19, 2);

    if (Configuration::fxaaIntensity <= FxaaIntensity::DISABLED || 
        Configuration::fxaaIntensity > FxaaIntensity::INTENSITY_6)
        return;

    // FxPipeline
    INSTALL_HOOK(InitializeCrossFade);
    INSTALL_HOOK(ExecuteCrossFade);

    // MTFx
    {
        hh::fx::SScreenRenderParam* newScreenRenderParam = new hh::fx::SScreenRenderParam();
        memcpy(newScreenRenderParam, (void*)0x13DF5A8, sizeof(hh::fx::SScreenRenderParam));

        // Refer to ShaderLoader.cpp for shader indices
        newScreenRenderParam->m_ShaderIndex = 0x350;

        // Insert our own Draw Instance Param to Render Before Particle 3
        hh::fx::SDrawInstanceParam* renderBeforeParticle3Param = (hh::fx::SDrawInstanceParam*)0x13DDDC8;

        hh::fx::SDrawInstanceParam* newChildren = new hh::fx::SDrawInstanceParam[renderBeforeParticle3Param->m_ChildParamCount + 1];
        memcpy(newChildren, renderBeforeParticle3Param->m_ChildParams, sizeof(hh::fx::SDrawInstanceParam) * renderBeforeParticle3Param->m_ChildParamCount);

        // Initialize FXAA parameters
        hh::fx::SDrawInstanceParam* fxaaParam = &newChildren[renderBeforeParticle3Param->m_ChildParamCount];
        memset(fxaaParam, 0, sizeof(hh::fx::SDrawInstanceParam));

        fxaaParam->m_pCallback = (void*)0x651820;
        fxaaParam->m_ChildParams = newScreenRenderParam;
        fxaaParam->m_RenderTargetSurface = 10;
        fxaaParam->m_TemporaryRenderTargetSurface = 7;
        fxaaParam->m_S0Sampler = 0x80 | 10;
        fxaaParam->m_Unk0 = 0x3;
        fxaaParam->m_Unk2 = 0x101;

        // Pass new pointers
        WRITE_MEMORY(&renderBeforeParticle3Param->m_ChildParams, void*, newChildren);
        WRITE_MEMORY(&renderBeforeParticle3Param->m_ChildParamCount, uint32_t, renderBeforeParticle3Param->m_ChildParamCount + 1);
    }
}
