#ifndef RENDER_BASE_SURFACE_H
#define RENDER_BASE_SURFACE_H

#include "render/types.hpp"
#include "texture.hpp"
#include "program.hpp"

namespace Render {

class Surface {
public:
    struct Config {
        std::shared_ptr<Texture> framebuffer;
        std::vector<std::shared_ptr<Program>> programs;
    };

    Config& cfg;
    Surface(Config& c) : cfg(c) {};
    virtual ~Surface() = default;

protected:
    virtual Result create() = 0;
    virtual Result destroy() = 0;
    virtual Result draw() = 0;
};

} // namespace Render

#endif
