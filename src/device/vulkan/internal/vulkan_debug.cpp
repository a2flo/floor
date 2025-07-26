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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include "vulkan_debug.hpp"
#include <floor/device/vulkan/vulkan_context.hpp>
#include <floor/core/core.hpp>
#include <floor/floor.hpp>
#include <cassert>
#include <cstring>
#include <regex>

namespace fl {

#if defined(FLOOR_DEBUG)
using namespace std::literals;

VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
													 VkDebugUtilsMessageTypeFlagsEXT message_types floor_unused,
													 const VkDebugUtilsMessengerCallbackDataEXT* cb_data,
													 /* vulkan_context* */ void* ctx) {
	const auto vk_ctx = (const vulkan_context*)ctx;
	if (vk_ctx->is_vulkan_validation_ignored()) {
		return VK_FALSE; // don't abort
	}
	if (cb_data != nullptr) {
		if ((cb_data->messageIdNumber == 181611958 || cb_data->messageIdNumber == 555635515) &&
			vk_ctx->get_vulkan_vr_context() != nullptr) {
			// ignore UNASSIGNED-BestPractices-vkCreateDevice-deprecated-extension and
			// VUID-VkDeviceCreateInfo-pNext-02830 when we're using a VR context
			return VK_FALSE;
		}
		if (cb_data->pMessage && cb_data->messageIdNumber == -628989766 &&
			(strstr(cb_data->pMessage, "VK_KHR_maintenance6") != nullptr ||
			 strstr(cb_data->pMessage, "VK_EXT_robustness2") != nullptr ||
			 strstr(cb_data->pMessage, "VK_EXT_swapchain_maintenance1") != nullptr)) {
			// ignore warnings about deprecated extensions:
			//  * VK_KHR_maintenance6: still needed in Vulkan 1.4, since we still need to enable it for VK_EXT_descriptor_buffer functionality ...
			//  * VK_EXT_robustness2/VK_EXT_swapchain_maintenance1: we support the KHR variants, but still need to support the EXT variants as well
			return VK_FALSE;
		}
	}
	
	std::string debug_message;
	if (cb_data != nullptr) {
		static const std::unordered_set<int32_t> ignore_msg_ids {
			-2027362524, // UNASSIGNED-BestPractices-vkCreateCommandPool-command-buffer-reset
			141128897, // BestPractices-vkCreateCommandPool-command-buffer-reset
			1218486124, // UNASSIGNED-BestPractices-pipeline-stage-flags
			561140764, // BestPractices-pipeline-stage-flags2-compute
			-298369678, // BestPractices-pipeline-stage-flags2-graphics
			-394667308, // UNASSIGNED-BestPractices-vkBeginCommandBuffer-simultaneous-use
			1231549373, // BestPractices-vkBeginCommandBuffer-simultaneous-use
			-1993010233, // UNASSIGNED-Descriptor uninitialized (NOTE/TODO: not updated for descriptor buffer use?)
			67123586, // UNASSIGNED-BestPractices-vkCreateRenderPass-image-requires-memory
			1016899250, // BestPractices-vkCreateRenderPass-image-requires-memory
			1734198062, // BestPractices-specialuse-extension
			-1443561624, // BestPractices-SyncObjects-HighNumberOfFences
			-539066078, // BestPractices-SyncObjects-HighNumberOfSemaphores
			-222910232, // BestPractices-NVIDIA-CreatePipelineLayout-SeparateSampler
			1469440330, // BestPractices-NVIDIA-CreatePipelineLayout-LargePipelineLayout
			-2047828895, // BestPractices-AMD-LocalWorkgroup-Multiple64 (seems to ignore actual work-group size?)
			1829508205, // BestPractices-Pipeline-SortAndBind
			-267480408, // BestPractices-NVIDIA-CreateImage-Depth32Format
			-1819900685, // BestPractices-AMD-VkCommandBuffer-AvoidSecondaryCmdBuffers
			1063606403, // BestPractices-AMD-vkImage-DontUseStorageRenderTargets
		};
		// separate list for when heap allocations are disabled
		static const std::unordered_set<int32_t> ignore_msg_ids_no_heap {
			-602362517, // UNASSIGNED-BestPractices-vkAllocateMemory-small-allocation
			-40745094, // BestPractices-vkAllocateMemory-small-allocation
			-1277938581, // UNASSIGNED-BestPractices-vkBindMemory-small-dedicated-allocation
			280337739, // BestPractices-vkBindBufferMemory-small-dedicated-allocation
			1147161417, // BestPractices-vkBindImageMemory-small-dedicated-allocation
			1484263523, // UNASSIGNED-BestPractices-vkAllocateMemory-too-many-objects
			-1265507290, // BestPractices-vkAllocateMemory-too-many-objects
			-1955647590, // BestPractices-NVIDIA-AllocateMemory-SetPriority
			11102936, // BestPractices-NVIDIA-BindMemory-NoPriority
			-954943182, // BestPractices-NVIDIA-AllocateMemory-ReuseAllocations
		};
		
		if (ignore_msg_ids.contains(cb_data->messageIdNumber)) {
			// ignore and don't abort
			return VK_FALSE;
		}
		if (!vk_ctx->has_vulkan_device_heaps() &&
			ignore_msg_ids_no_heap.contains(cb_data->messageIdNumber)) {
			return VK_FALSE;
		}
		
		static const auto log_binaries = floor::get_toolchain_log_binaries();
		if (log_binaries && cb_data->messageIdNumber == 358835246) {
			// ignore UNASSIGNED-BestPractices-vkCreateDevice-specialuse-extension-devtools
			return VK_FALSE;
		}
		
		debug_message += "\n\t";
		if (cb_data->pMessageIdName) {
			debug_message += cb_data->pMessageIdName + " ("s + std::to_string(cb_data->messageIdNumber) + ")\n";
		} else {
			debug_message += std::to_string(cb_data->messageIdNumber) + "\n";
		}
		
		std::vector<uint64_t> thread_ids;
		if (cb_data->pMessage) {
			auto tokens = core::tokenize(cb_data->pMessage, '|');
			for (auto& token : tokens) {
				token = core::trim(token);
				debug_message += "\t" + token + "\n";
				
				// if this is a threading error, extract the thread ids
				if (token.find("THREADING") != std::string::npos) {
					static const std::regex rx_thread_id("thread ((0x)?[0-9a-fA-F]+)");
					std::smatch regex_result;
					while (regex_search(token, regex_result, rx_thread_id)) {
						const auto is_hex = regex_result[1].str().starts_with("0x");
						thread_ids.emplace_back(strtoull(regex_result[1].str().c_str(), nullptr, is_hex ? 16 : 10));
						token = regex_result.suffix();
					}
				}
			}
		}
		
		if (cb_data->queueLabelCount > 0 && cb_data->pQueueLabels != nullptr) {
			debug_message += "\tqueue labels: ";
			for (uint32_t qidx = 0; qidx < cb_data->queueLabelCount; ++qidx) {
				debug_message += (cb_data->pQueueLabels[qidx].pLabelName ? cb_data->pQueueLabels[qidx].pLabelName : "<no-queue-label>");
				if (qidx + 1 < cb_data->queueLabelCount){
					debug_message += ", ";
				}
			}
			debug_message += '\n';
		}
		if (cb_data->cmdBufLabelCount > 0 && cb_data->pCmdBufLabels != nullptr) {
			debug_message += "\tcommand buffer labels: ";
			for (uint32_t cidx = 0; cidx < cb_data->cmdBufLabelCount; ++cidx) {
				debug_message += (cb_data->pCmdBufLabels[cidx].pLabelName ? cb_data->pCmdBufLabels[cidx].pLabelName : "<no-command-buffer-label>");
				if (cidx + 1 < cb_data->cmdBufLabelCount){
					debug_message += ", ";
				}
			}
			debug_message += '\n';
		}
		if (cb_data->objectCount > 0 && cb_data->pObjects != nullptr) {
			debug_message += "\tobjects:\n";
			for (uint32_t oidx = 0; oidx < cb_data->objectCount; ++oidx) {
				debug_message += "\t\t";
				debug_message += (cb_data->pObjects[oidx].pObjectName ? cb_data->pObjects[oidx].pObjectName : "<no-object-name>");
				debug_message += " ("s + vulkan_object_type_to_string(cb_data->pObjects[oidx].objectType) + ", ";
				debug_message += std::to_string(cb_data->pObjects[oidx].objectHandle) + ")\n";
			}
		}
		if (!thread_ids.empty()) {
			debug_message += "\tthreads:\n";
			for (const auto& tid : thread_ids) {
				debug_message += "\t\t" + std::to_string(tid);
#if !defined(__WINDOWS__)
				static constexpr const size_t max_thread_name_length { 16 };
				char thread_name[max_thread_name_length];
				if (pthread_getname_np((pthread_t)tid, thread_name, max_thread_name_length) == 0) {
					thread_name[max_thread_name_length - 1] = '\0';
					if (strlen(thread_name) > 0) {
						debug_message += " ("s + thread_name + ")";
					}
				}
#endif
				debug_message += '\n';
			}
		}
	} else {
		debug_message = " <callback-data-is-nullptr>";
	}
	
	if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) > 0) {
		log_error("Vulkan error:$", debug_message);
	} else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) > 0) {
		log_warn("Vulkan warning:$", debug_message);
	} else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) > 0) {
		log_msg("Vulkan info:$", debug_message);
	} else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) > 0) {
		log_debug("Vulkan verbose:$", debug_message);
	} else {
		assert(false && "unknown severity");
	}
	logger::flush();
	return VK_FALSE; // don't abort
}

void set_vulkan_debug_label(const vulkan_device& dev, const VkObjectType type, const uint64_t& handle, const std::string& label) {
	if (vkSetDebugUtilsObjectNameEXT == nullptr) {
		return;
	}
	
	const VkDebugUtilsObjectNameInfoEXT name_info {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = type,
		.objectHandle = handle,
		.pObjectName = label.c_str(),
	};
	vkSetDebugUtilsObjectNameEXT(dev.device, &name_info);
}

void vulkan_begin_cmd_debug_label(const VkCommandBuffer& cmd_buffer, const char* label) {
	if (vkCmdBeginDebugUtilsLabelEXT == nullptr) {
		return;
	}
	
	const VkDebugUtilsLabelEXT debug_label {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		.pNext = nullptr,
		.pLabelName = label,
		.color = {},
	};
	vkCmdBeginDebugUtilsLabelEXT(cmd_buffer, &debug_label);
}

void vulkan_end_cmd_debug_label(const VkCommandBuffer& cmd_buffer) {
	if (vkCmdEndDebugUtilsLabelEXT == nullptr) {
		return;
	}
	vkCmdEndDebugUtilsLabelEXT(cmd_buffer);
}

void vulkan_insert_cmd_debug_label(const VkCommandBuffer& cmd_buffer, const char* label) {
	if (vkCmdInsertDebugUtilsLabelEXT == nullptr) {
		return;
	}
	
	const VkDebugUtilsLabelEXT debug_label {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		.pNext = nullptr,
		.pLabelName = label,
		.color = {},
	};
	vkCmdInsertDebugUtilsLabelEXT(cmd_buffer, &debug_label);
}

#endif

} // namespace fl

#endif // FLOOR_NO_VULKAN
