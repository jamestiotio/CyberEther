#ifndef JETSTREAM_RENDER_UTILS_HH
#define JETSTREAM_RENDER_UTILS_HH

#include "jetstream/render/base.hh"
#include "jetstream/memory/base.hh"
#include "jetstream/types.hh"

namespace Jetstream {

template<Device D, typename T>
std::tuple<void*, bool> ConvertToOptimalStorage(const std::shared_ptr<Render::Window>& window, 
                                                Tensor<D, T>& tensor) {
    void* buffer = nullptr;
    bool enableZeroCopy = false;

    Device renderDevice = window->device();

    if (tensor.compatible_devices().contains(renderDevice)) {
#ifdef JETSTREAM_BACKEND_CPU_AVAILABLE
        if (renderDevice == Device::CPU) {
            auto optimal = MapOn<Device::CPU>(tensor);
            if (optimal.device_native()) {
                enableZeroCopy = true;
            }
            buffer = optimal.data();
        }
#endif

#ifdef JETSTREAM_BACKEND_METAL_AVAILABLE
        if (renderDevice == Device::Metal) {
            auto optimal = MapOn<Device::Metal>(tensor);
            if (optimal.device_native()) {
                enableZeroCopy = true;
            }
            buffer = optimal.data();
        }
#endif

#ifdef JETSTREAM_BACKEND_VULKAN_AVAILABLE
        if (renderDevice == Device::Vulkan) {
            auto optimal = MapOn<Device::Vulkan>(tensor);
            if (optimal.device_native()) {
                enableZeroCopy = true;
            }
            buffer = optimal.data();
        }
#endif

#ifdef JETSTREAM_BACKEND_CUDA_AVAILABLE
        if (renderDevice == Device::CUDA) {
            auto optimal = MapOn<Device::CUDA>(tensor);
            if (optimal.device_native()) {
                enableZeroCopy = true;
            }
            buffer = optimal.data();
        }
#endif
    } else {
        buffer = MapOn<Device::CPU>(tensor).data();
        enableZeroCopy = false;
    }

    return {buffer, enableZeroCopy};
}

}  // namespace Jetstream

#endif
