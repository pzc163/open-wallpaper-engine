#pragma once

#include "vvk/vulkan_wrapper.hpp"

#include <array>

namespace wallpaper::vulkan
{

// Render targets can have been written by a draw, clear, or copy. Sampling
// arbitrary UVs needs a global dependency, not a framebuffer-local one.
inline void RecordTextureReadBarrier(vvk::CommandBuffer& cmd, VkImage image,
                                     VkImageLayout layout) {
    VkImageMemoryBarrier barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = layout,
        .newLayout = layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS,
                              0, VK_REMAINING_ARRAY_LAYERS },
    };
    cmd.PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0, barrier);
}

// Both images are single-sampled color render targets. Equal extents retain
// exact copies; different extents resample the entire image in both directions.
inline void RecordRenderTargetCopy(vvk::CommandBuffer& cmd,
                                    VkImage src, VkExtent3D src_extent,
                                    VkImage dst, VkExtent3D dst_extent) {
    if (src == dst) return;

    const VkImageSubresourceRange range { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    const VkImageSubresourceLayers layers { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    std::array before {
        VkImageMemoryBarrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = src,
            .subresourceRange = range,
        },
        VkImageMemoryBarrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = dst,
            .subresourceRange = range,
        },
    };
    cmd.PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, {}, {}, before);

    if (src_extent.width == dst_extent.width && src_extent.height == dst_extent.height) {
        VkImageCopy copy {
            .srcSubresource = layers,
            .dstSubresource = layers,
            .extent = { src_extent.width, src_extent.height, 1 },
        };
        cmd.CopyImage(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy);
    } else {
        VkImageBlit blit {
            .srcSubresource = layers,
            .srcOffsets = { { 0, 0, 0 }, { static_cast<int32_t>(src_extent.width),
                                          static_cast<int32_t>(src_extent.height), 1 } },
            .dstSubresource = layers,
            .dstOffsets = { { 0, 0, 0 }, { static_cast<int32_t>(dst_extent.width),
                                          static_cast<int32_t>(dst_extent.height), 1 } },
        };
        cmd.BlitImage(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, blit, VK_FILTER_LINEAR);
    }

    auto after = before;
    after[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    after[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    for (auto& barrier : after) {
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.oldLayout = barrier.newLayout;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    cmd.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        0, {}, {}, after);
}

} // namespace wallpaper::vulkan
