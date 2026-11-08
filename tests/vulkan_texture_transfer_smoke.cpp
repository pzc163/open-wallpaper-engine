#include "VulkanRender/TextureTransfer.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace
{
struct Recording {
    std::vector<char> operations;
    VkExtent3D source {}, destination {};
};

Recording& recording(VkCommandBuffer command) {
    return *reinterpret_cast<Recording*>(command);
}

VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer command, VkPipelineStageFlags src_stage,
                                   VkPipelineStageFlags dst_stage, VkDependencyFlags flags,
                                   uint32_t, const VkMemoryBarrier*, uint32_t,
                                   const VkBufferMemoryBarrier*, uint32_t count,
                                   const VkImageMemoryBarrier* images) {
    auto& record = recording(command);
    // Downsampling samples outside the destination pixel's framebuffer region.
    assert(flags == 0);
    for (uint32_t i = 0; i < count; ++i) {
        assert(images[i].srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
        assert(images[i].dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
    }
    if (count == 1) {
        assert(src_stage == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        assert(dst_stage & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        assert(images[0].srcAccessMask & VK_ACCESS_MEMORY_WRITE_BIT);
        assert(images[0].dstAccessMask == VK_ACCESS_SHADER_READ_BIT);
        record.operations.push_back('R');
    } else {
        assert(count == 2);
        if (dst_stage == VK_PIPELINE_STAGE_TRANSFER_BIT) {
            assert(src_stage == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
            assert(images[0].srcAccessMask & VK_ACCESS_MEMORY_WRITE_BIT);
            assert(images[0].dstAccessMask == VK_ACCESS_TRANSFER_READ_BIT);
            assert(images[1].dstAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT);
            record.operations.push_back('B');
        } else {
            assert(src_stage == VK_PIPELINE_STAGE_TRANSFER_BIT);
            assert(images[1].srcAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT);
            for (uint32_t i = 0; i < count; ++i) {
                assert(images[i].newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            record.operations.push_back('A');
        }
    }
}

VKAPI_ATTR void VKAPI_CALL copy(VkCommandBuffer command, VkImage, VkImageLayout src_layout,
                                VkImage, VkImageLayout dst_layout, uint32_t count,
                                const VkImageCopy* regions) {
    auto& record = recording(command);
    assert(src_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    assert(dst_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    assert(count == 1);
    assert(record.source.width == record.destination.width);
    assert(record.source.height == record.destination.height);
    assert(regions[0].extent.width == record.destination.width);
    assert(regions[0].extent.height == record.destination.height);
    record.operations.push_back('C');
}

VKAPI_ATTR void VKAPI_CALL blit(VkCommandBuffer command, VkImage, VkImageLayout src_layout,
                                VkImage, VkImageLayout dst_layout, uint32_t count,
                                const VkImageBlit* regions, VkFilter filter) {
    auto& record = recording(command);
    assert(src_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    assert(dst_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    assert(count == 1 && filter == VK_FILTER_LINEAR);
    const auto& region = regions[0];
    assert(region.srcOffsets[0].x == 0 && region.srcOffsets[0].y == 0);
    assert(region.dstOffsets[0].x == 0 && region.dstOffsets[0].y == 0);
    assert(region.srcOffsets[1].x == static_cast<int32_t>(record.source.width));
    assert(region.srcOffsets[1].y == static_cast<int32_t>(record.source.height));
    // Upsampling must cover the whole destination, not only a half/quarter corner.
    assert(region.dstOffsets[1].x == static_cast<int32_t>(record.destination.width));
    assert(region.dstOffsets[1].y == static_cast<int32_t>(record.destination.height));
    assert(region.srcOffsets[1].z == 1 && region.dstOffsets[1].z == 1);
    record.operations.push_back('S');
}

void checkTransfer(VkExtent3D src, VkExtent3D dst, char operation) {
    Recording record { .source = src, .destination = dst };
    vvk::DeviceDispatch dispatch;
    dispatch.vkCmdPipelineBarrier = barrier;
    dispatch.vkCmdCopyImage = copy;
    dispatch.vkCmdBlitImage = blit;
    vvk::CommandBuffer command(reinterpret_cast<VkCommandBuffer>(&record), dispatch);
    const auto src_image = reinterpret_cast<VkImage>(uintptr_t { 1 });
    const auto dst_image = reinterpret_cast<VkImage>(uintptr_t { 2 });
    wallpaper::vulkan::RecordRenderTargetCopy(command, src_image, src, dst_image, dst);
    wallpaper::vulkan::RecordTextureReadBarrier(
        command, dst_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    assert((record.operations == std::vector<char> { 'B', operation, 'A', 'R' }));
}
} // namespace

int main() {
    const VkExtent3D full { 3840, 2160, 1 }, half { 1920, 1080, 1 }, quarter { 960, 540, 1 };
    checkTransfer(full, full, 'C');
    checkTransfer(full, half, 'S');
    checkTransfer(half, quarter, 'S');
    checkTransfer(quarter, half, 'S');
    checkTransfer(half, full, 'S');
    checkTransfer(quarter, full, 'S');
    checkTransfer({ 3361, 1915, 1 }, { 1681, 958, 1 }, 'S');
    checkTransfer({ 1, 1, 1 }, full, 'S');
}
