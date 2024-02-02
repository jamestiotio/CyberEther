#include "jetstream/compute/graph/cpu.hh"

namespace Jetstream {

CPU::CPU() {
    JST_DEBUG("Creating new CPU compute graph.");
    metadata = std::make_shared<RuntimeMetadata>();
    metadata->cpu.graph = this;
}

CPU::~CPU() {
    metadata.reset();
}

Result CPU::create() {
    for (const auto& block : blocks) {
        JST_CHECK(block->createCompute(*metadata));
    }
    return Result::SUCCESS;
}

Result CPU::computeReady() {
    for (const auto& block : blocks) {
        JST_CHECK(block->computeReady());
    }
    return Result::SUCCESS;
}

Result CPU::compute() {
    for (const auto& block : blocks) { 
        JST_CHECK(block->compute(*metadata));
    }
    return Result::SUCCESS;
}

Result CPU::destroy() {
    for (const auto& block : blocks) {
        JST_CHECK(block->destroyCompute(*metadata));
    }
    blocks.clear();
    return Result::SUCCESS;
}

}  // namespace Jetstream
