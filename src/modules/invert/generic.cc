#include "jetstream/modules/invert.hh"

#include "benchmark.cc"

namespace Jetstream {

template<Device D, typename T>
Result Invert<D, T>::create() {
    JST_DEBUG("Initializing Invert module.");
    JST_INIT_IO();

    // Allocate output.

    output.buffer = Tensor<D, T>(input.buffer.shape());

    return Result::SUCCESS;
}

template<Device D, typename T>
void Invert<D, T>::info() const {
    JST_DEBUG("  None");
}

}  // namespace Jetstream
