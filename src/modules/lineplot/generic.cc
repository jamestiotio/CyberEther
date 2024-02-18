#include "jetstream/modules/lineplot.hh"
#include "shaders/lineplot_shaders.hh"
#include "jetstream/render/utils.hh"

#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "benchmark.cc"

namespace Jetstream {

template<Device D, typename T>
struct Lineplot<D, T>::GImpl {
    struct {
        glm::mat4 transform;
    } shaderUniforms;
};

template<Device D, typename T>
Result Lineplot<D, T>::create() {
    JST_DEBUG("Initializing Lineplot module.");
    JST_INIT_IO();

    // Check parameters.

    if (input.buffer.rank() > 2) {
        JST_ERROR("Invalid input rank ({}). It should be `1` or `2`.", input.buffer.rank());
        return Result::ERROR;
    }

    // Calculate parameters.

    const U64 last_axis = input.buffer.rank() - 1;
    numberOfElements = input.buffer.shape()[last_axis];
    numberOfBatches = (input.buffer.rank() == 2) ? input.buffer.shape()[0] : 1;
    normalizationFactor = 1.0f / (0.5f * numberOfBatches);

    // Allocate internal buffers.

    grid = Tensor<Device::CPU, T>({config.numberOfVerticalLines + config.numberOfHorizontalLines, 6, 3});
    signal = Tensor<D, T>({numberOfElements - 1, 4, 3});

    return Result::SUCCESS;
}

template<Device D, typename T>
void Lineplot<D, T>::info() const {
    JST_INFO("  Size: [{}, {}]", config.viewSize.width, config.viewSize.height);
}

template<Device D, typename T>
Result Lineplot<D, T>::createPresent() {
    // Configure render pipeline.

    Render::Buffer::Config gridVerticesConf;
    gridVerticesConf.buffer = grid.data();
    gridVerticesConf.elementByteSize = sizeof(F32);
    gridVerticesConf.size = grid.size();
    gridVerticesConf.target = Render::Buffer::Target::VERTEX;
    gridVerticesConf.enableZeroCopy = false;
    JST_CHECK(window->build(gridVerticesBuffer, gridVerticesConf));

    Render::Vertex::Config gridVertexCfg;
    gridVertexCfg.buffers = {
        {gridVerticesBuffer, 3},
    };
    JST_CHECK(window->build(gridVertex, gridVertexCfg));

    Render::Draw::Config drawGridVertexCfg;
    drawGridVertexCfg.buffer = gridVertex;
    drawGridVertexCfg.mode = Render::Draw::Mode::TRIANGLES;
    JST_CHECK(window->build(drawGridVertex, drawGridVertexCfg));

    auto [buffer, enableZeroCopy] = ConvertToOptimalStorage(window, signal);

    Render::Buffer::Config lineVerticesConf;
    lineVerticesConf.buffer = buffer;
    lineVerticesConf.elementByteSize = sizeof(F32);
    lineVerticesConf.size = signal.size();
    lineVerticesConf.target = Render::Buffer::Target::VERTEX;
    lineVerticesConf.enableZeroCopy = enableZeroCopy;
    JST_CHECK(window->build(lineVerticesBuffer, lineVerticesConf));

    Render::Vertex::Config lineVertexCfg;
    lineVertexCfg.buffers = {
        {lineVerticesBuffer, 3},
    };
    JST_CHECK(window->build(lineVertex, lineVertexCfg));

    Render::Draw::Config drawLineVertexCfg;
    drawLineVertexCfg.buffer = lineVertex;
    drawLineVertexCfg.mode = Render::Draw::Mode::TRIANGLE_STRIP;
    JST_CHECK(window->build(drawLineVertex, drawLineVertexCfg));

    Render::Texture::Config lutTextureCfg;
    lutTextureCfg.size = {256, 1};
    lutTextureCfg.buffer = (uint8_t*)Render::Extras::TurboLutBytes;
    JST_CHECK(window->build(lutTexture, lutTextureCfg));

    Render::Buffer::Config uniformCfg;
    uniformCfg.buffer = &gimpl->shaderUniforms;
    uniformCfg.elementByteSize = sizeof(gimpl->shaderUniforms);
    uniformCfg.size = 1;
    uniformCfg.target = Render::Buffer::Target::UNIFORM;
    JST_CHECK(window->build(gridUniformBuffer, uniformCfg));
    JST_CHECK(window->build(signalUniformBuffer, uniformCfg));

    Render::Program::Config gridProgramCfg;
    gridProgramCfg.shaders = ShadersPackage["grid"];
    gridProgramCfg.draw = drawGridVertex;
    gridProgramCfg.buffers = {
        {gridUniformBuffer, Render::Program::Target::VERTEX | 
                            Render::Program::Target::FRAGMENT},
    };
    JST_CHECK(window->build(gridProgram, gridProgramCfg));

    Render::Program::Config signalProgramCfg;
    signalProgramCfg.shaders = ShadersPackage["signal"];
    signalProgramCfg.draw = drawLineVertex;
    signalProgramCfg.textures = {lutTexture};
    signalProgramCfg.buffers = {
        {signalUniformBuffer, Render::Program::Target::VERTEX | 
                              Render::Program::Target::FRAGMENT},
    };
    JST_CHECK(window->build(signalProgram, signalProgramCfg));

    Render::Texture::Config textureCfg;
    textureCfg.size = config.viewSize;
    JST_CHECK(window->build(texture, textureCfg));

    Render::Surface::Config surfaceCfg;
    surfaceCfg.framebuffer = texture;
    surfaceCfg.programs = {gridProgram, signalProgram};
    JST_CHECK(window->build(surface, surfaceCfg));
    JST_CHECK(window->bind(surface));

    // Initialize variables.

    updateTransform();
    updateScaling();
    updateGridVertices();

    return Result::SUCCESS;
}

template<Device D, typename T>
Result Lineplot<D, T>::destroyPresent() {
    JST_CHECK(window->unbind(surface));

    return Result::SUCCESS;
}

template<Device D, typename T>
Result Lineplot<D, T>::present() {
    lineVerticesBuffer->update();
    return Result::SUCCESS;
}

template<Device D, typename T>
const Size2D<U64>& Lineplot<D, T>::viewSize(const Size2D<U64>& viewSize) {
    if (surface->size(viewSize) != this->viewSize()) {
        config.viewSize = surface->size();
    }

    updateScaling();
    updateGridVertices();

    return this->viewSize();
}

template<Device D, typename T>
std::pair<F32, F32> Lineplot<D, T>::zoom(const std::pair<F32, F32>& mouse_pos, const F32& zoom) {
    const auto& before_mouse_x = windowToPlotCoords(mouse_pos);

    if (zoom < 1.0f) {
        config.zoom = 1.0f;
        config.translation = 0.0f;
    } else {
        config.zoom = zoom;
        const auto& after_mouse_x = windowToPlotCoords(mouse_pos);
        config.translation += after_mouse_x - before_mouse_x;
    }

    updateTransform();
    updateScaling();
    updateGridVertices();

    return {config.zoom, config.translation};
}

template<Device D, typename T>
const F32& Lineplot<D, T>::translation(const F32& translation) {
    config.translation = translation;

    updateTransform();
    updateGridVertices();

    return config.translation;
}

template<Device D, typename T>
void Lineplot<D, T>::updateTransform() {
    const F32 maxTranslation = std::abs((1.0f / config.zoom) - 1.0f);
    config.translation = std::clamp(config.translation, -maxTranslation, maxTranslation);

    auto& transform = gimpl->shaderUniforms.transform;

    // Reset transform.
    transform = glm::mat4(1.0f);

    // Apply the translation according to the mouse position.
    transform = glm::translate(transform, glm::vec3(config.translation * config.zoom, 0.0f, 0.0f));

    // Apply the zoom.
    transform = glm::scale(transform, glm::vec3(config.zoom, 1.0f, 1.0f));

    // Update the signal and grid uniform buffers.
    signalUniformBuffer->update();
    gridUniformBuffer->update();
}

template<Device D, typename T>
void Lineplot<D, T>::updateScaling() {
    auto& [x, y] = thickness;
    x = ((2.0f / config.viewSize.width) * config.thickness) / config.zoom / 2.0f;
    y = ((2.0f / config.viewSize.height) * config.thickness) / 2.0f;
}

template<Device D, typename T>
void Lineplot<D, T>::updateGridVertices() {
    const U64& num_cols = config.numberOfVerticalLines;
    const U64& num_rows = config.numberOfHorizontalLines;
    const auto& [thickness_x, thickness_y] = thickness;

    const F32 x_step  = +2.0f / (num_cols - 1);
    const F32 y_step  = +2.0f / (num_rows - 1);
    const F32 x_start = -1.0f;
    const F32 y_start = -1.0f;
    const F32 x_end   = +1.0f;
    const F32 y_end   = +1.0f;

    for (U64 row = 0; row < num_rows; row++) {
        const F32 y = y_start + row * y_step;

        // 0
        grid[{row, 0, 0}] = x_start;
        grid[{row, 0, 1}] = y + thickness_y;
        grid[{row, 0, 2}] = 0.0f;

        // 1
        grid[{row, 1, 0}] = x_start;
        grid[{row, 1, 1}] = y - thickness_y;
        grid[{row, 1, 2}] = 0.0f;

        // 2
        grid[{row, 2, 0}] = x_end;
        grid[{row, 2, 1}] = y + thickness_y;
        grid[{row, 2, 2}] = 0.0f;

        // 2
        grid[{row, 3, 0}] = x_end;
        grid[{row, 3, 1}] = y + thickness_y;
        grid[{row, 3, 2}] = 0.0f;

        // 3
        grid[{row, 4, 0}] = x_end;
        grid[{row, 4, 1}] = y - thickness_y;
        grid[{row, 4, 2}] = 0.0f;

        // 0
        grid[{row, 5, 0}] = x_start;
        grid[{row, 5, 1}] = y - thickness_y;
        grid[{row, 5, 2}] = 0.0f;
    }

    for (U64 col = 0; col < num_cols; col++) {
        const F32 x = x_start + col * x_step;

        // 0
        grid[{col + num_rows, 0, 0}] = x + thickness_x;
        grid[{col + num_rows, 0, 1}] = y_start;
        grid[{col + num_rows, 0, 2}] = 0.0f;

        // 1
        grid[{col + num_rows, 1, 0}] = x - thickness_x;
        grid[{col + num_rows, 1, 1}] = y_start;
        grid[{col + num_rows, 1, 2}] = 0.0f;

        // 2
        grid[{col + num_rows, 2, 0}] = x + thickness_x;
        grid[{col + num_rows, 2, 1}] = y_end;
        grid[{col + num_rows, 2, 2}] = 0.0f;

        // 2
        grid[{col + num_rows, 3, 0}] = x + thickness_x;
        grid[{col + num_rows, 3, 1}] = y_end;
        grid[{col + num_rows, 3, 2}] = 0.0f;

        // 3
        grid[{col + num_rows, 4, 0}] = x - thickness_x;
        grid[{col + num_rows, 4, 1}] = y_end;
        grid[{col + num_rows, 4, 2}] = 0.0f;

        // 0
        grid[{col + num_rows, 5, 0}] = x - thickness_x;
        grid[{col + num_rows, 5, 1}] = y_start;
        grid[{col + num_rows, 5, 2}] = 0.0f;
    }

    gridVerticesBuffer->update();
}

template<Device D, typename T>
F32 Lineplot<D, T>::windowToPlotCoords(const std::pair<F32, F32>& mouse_pos) {
    const auto& [x, _1] = mouse_pos;
    const auto& [width, _2] = config.viewSize;

    const F32 norm_x = (x / width) * 2.0f - 1.0f;
    const F32 plot_x = norm_x / config.zoom;

    return plot_x;
};

template<Device D, typename T>
Render::Texture& Lineplot<D, T>::getTexture() {
    return *texture;
};

}  // namespace Jetstream
