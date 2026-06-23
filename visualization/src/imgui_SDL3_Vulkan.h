#pragma once
// Dear ImGui: standalone example application for SDL3 + Vulkan

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important note to the reader who wish to integrate imgui_impl_vulkan.cpp/.h in their own
// engine/app.
// - Common ImGui_ImplVulkan_XXX functions and structures are used to interface with
// imgui_impl_vulkan.cpp/.h.
//   You will use those if you want to use this rendering backend in your engine/app.
// - Helper ImGui_ImplVulkanH_XXX functions and structures are only used by this example (main.cpp)
// and by
//   the backend itself (imgui_impl_vulkan.cpp), but should PROBABLY NOT be used by your own
//   engine/app code.
// Read comments in imgui_impl_vulkan.h.

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdlib.h>  // abort

#include "imgui.h"
#include "imgui_impl_vulkan.h"

#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
#define VOLK_IMPLEMENTATION
#include <volk.h>
#endif

// #define APP_USE_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
#endif

struct VulkanContext {
    // Data
    VkAllocationCallbacks* Allocator = nullptr;
    VkInstance Instance = VK_NULL_HANDLE;
    VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
    VkDevice Device = VK_NULL_HANDLE;
    uint32_t QueueFamily = (uint32_t)-1;
    VkQueue Queue = VK_NULL_HANDLE;
    VkPipelineCache PipelineCache = VK_NULL_HANDLE;
    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;

    ImGui_ImplVulkanH_Window MainWindowData;
    uint32_t MinImageCount = 2;
    bool SwapChainRebuild = false;
    SDL_Window* window;
    ImGui_ImplVulkanH_Window* wd;
    bool VsyncEnabled = true;
};
// Volk headers

int init_sdl3_vulkan(VulkanContext* ctx);
void cleanup(VulkanContext* ctx);
void ToggleVsyncSwapChain(VulkanContext* ctx, bool vsyncOn);
void imguiHandleInput(SDL_Event* event);
void check_vk_result(VkResult err);
bool IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension);
void SetupVulkan(VulkanContext* ctx, ImVector<const char*> instance_extensions);
void SetupVulkanWindow(VulkanContext* ctx, VkSurfaceKHR surface, int width, int height);
void CleanupVulkan(VulkanContext* ctx);
void CleanupVulkanWindow(VulkanContext* ctx);
void FrameRender(VulkanContext* ctx, ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data);
void FramePresent(VulkanContext* ctx, ImGui_ImplVulkanH_Window* wd);
void ResizeSwapChain(VulkanContext* ctx, int fb_width, int fb_height);
