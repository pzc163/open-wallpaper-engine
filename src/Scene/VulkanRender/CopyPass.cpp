#include "CopyPass.hpp"
#include "VulkanRender/TextureTransfer.hpp"
#include "SpecTexs.hpp"
#include "Utils/Logging.h"
#include "Utils/AutoDeletor.hpp"
#include "Resource.hpp"
#include "PassCommon.hpp"

using namespace wallpaper::vulkan;

CopyPass::CopyPass(const Desc& desc): m_desc(desc) {}

CopyPass::~CopyPass() {};

void CopyPass::prepare(Scene& scene, const Device& device, RenderingResources& rr) {
    const std::string src_name = scene.ResolveRenderTargetName(m_desc.src);
    const std::string dst_name = scene.ResolveRenderTargetName(m_desc.dst);
    if (!scene.HasRenderTarget(src_name)) {
        LOG_ERROR("%s not found", m_desc.src.c_str());
        return;
    }
    if (!scene.HasRenderTarget(dst_name)) {
        auto& rt                            = *scene.FindRenderTarget(src_name);
        scene.renderTargets[dst_name]       = rt;
        scene.renderTargets[dst_name].allowReuse = true;
    }

    std::array<std::string, 2>      textures    = { src_name, dst_name };
    std::array<ImageParameters*, 2> vk_textures = { &m_desc.vk_src, &m_desc.vk_dst };
    for (usize i = 0; i < textures.size(); i++) {
        auto& tex_name = textures[i];
        if (tex_name.empty()) continue;

        ImageParameters img;
        if (IsSpecTex(tex_name)) {
            auto& rt  = *scene.FindRenderTarget(tex_name);
            auto  opt = device.tex_cache().Query(tex_name, ToTexKey(rt), ! rt.allowReuse);
            if (opt.has_value())
                img = opt.value();
            else
                LOG_ERROR("query image from cache failed");
        } else {
            LOG_ERROR("can't copy image source");
            return;
        }
        *vk_textures[i] = img;
    }

    for (auto& tex : releaseTexs()) {
        device.tex_cache().MarkShareReady(tex);
    }

    setPrepared();
};
void CopyPass::execute(const Device& device, RenderingResources& rr) {
    auto& src = m_desc.vk_src;
    auto& dst = m_desc.vk_dst;
    if (! (src.handle && dst.handle)) {
        assert(src.handle && dst.handle);
        return;
    }
    RecordRenderTargetCopy(rr.command, src.handle, src.extent, dst.handle, dst.extent);
    if (dst.mipmap_level > 1) {
        device.tex_cache().RecGenerateMipmaps(rr.command, dst);
    }
};
void CopyPass::destory(const Device&, RenderingResources&) {}
