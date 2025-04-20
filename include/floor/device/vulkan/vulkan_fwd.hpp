/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

// NOTE: this adds various forward declarations for Vulkan, so that we don't have to publicly include Vulkan headers or volk

#pragma once

struct VkBuffer_T;
using VkBuffer = VkBuffer_T*;
struct VkImage_T;
using VkImage = VkImage_T*;
struct VkInstance_T;
using VkInstance = VkInstance_T*;
struct VkPhysicalDevice_T;
using VkPhysicalDevice = VkPhysicalDevice_T*;
struct VkDevice_T;
using VkDevice = VkDevice_T*;
struct VkQueue_T;
using VkQueue = VkQueue_T*;
struct VkSemaphore_T;
using VkSemaphore = VkSemaphore_T*;
struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
struct VkFence_T;
using VkFence = VkFence_T*;
struct VkDeviceMemory_T;
using VkDeviceMemory = VkDeviceMemory_T*;
struct VkImageView_T;
using VkImageView = VkImageView_T*;
struct VkShaderModule_T;
using VkShaderModule = VkShaderModule_T*;
struct VkPipelineCache_T;
using VkPipelineCache = VkPipelineCache_T*;
struct VkPipelineLayout_T;
using VkPipelineLayout = VkPipelineLayout_T*;
struct VkPipeline_T;
using VkPipeline = VkPipeline_T*;
struct VkRenderPass_T;
using VkRenderPass = VkRenderPass_T*;
struct VkDescriptorSetLayout_T;
using VkDescriptorSetLayout = VkDescriptorSetLayout_T*;
struct VkSampler_T;
using VkSampler = VkSampler_T*;
struct VkDescriptorSet_T;
using VkDescriptorSet = VkDescriptorSet_T*;
struct VkFramebuffer_T;
using VkFramebuffer = VkFramebuffer_T*;
struct VkCommandPool_T;
using VkCommandPool = VkCommandPool_T*;

using VkDeviceAddress = uint64_t;
using VkDeviceSize = uint64_t;
struct VkImageMemoryBarrier2;
struct VkPhysicalDeviceMemoryProperties;
struct VkBufferImageCopy2;
struct VkInstanceCreateInfo;
struct VkDeviceCreateInfo;

using VkFlags64 = uint64_t;
using VkBufferUsageFlags2 = VkFlags64;
using VkAccessFlags2 = VkFlags64;

union VkClearValue;

struct VolkDeviceTable;
