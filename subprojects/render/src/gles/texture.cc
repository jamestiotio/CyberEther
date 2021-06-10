#include "render/gles/texture.hpp"

namespace Render {

Result GLES::Texture::create() {
    if (!GLES::cudaInteropSupported() && cfg.cudaInterop) {
        cfg.cudaInterop = false;
        return Result::ERROR;
    }

    pfmt = convertPixelFormat(cfg.pfmt);
    ptype = convertPixelType(cfg.ptype);
    dfmt = convertDataFormat(cfg.dfmt);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    auto ptr = (cfg.cudaInterop) ? nullptr : cfg.buffer;
    glTexImage2D(GL_TEXTURE_2D, 0, dfmt, cfg.width, cfg.height, 0, pfmt, ptype, ptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    if (cfg.cudaInterop) {
#ifdef RENDER_CUDA_INTEROP_AVAILABLE
        cudaGraphicsGLRegisterImage(&cuda_tex_resource, tex, GL_TEXTURE_2D,
                cudaGraphicsRegisterFlagsNone);
#endif
    }

    return GLES::getError(__FUNCTION__, __FILE__, __LINE__);
}

Result GLES::Texture::destroy() {
    glDeleteTextures(1, &tex);

    return GLES::getError(__FUNCTION__, __FILE__, __LINE__);
}

Result GLES::Texture::start() {
    glBindTexture(GL_TEXTURE_2D, tex);

    return GLES::getError(__FUNCTION__, __FILE__, __LINE__);
}

Result GLES::Texture::end() {
    glBindTexture(GL_TEXTURE_2D, 0);

    return GLES::getError(__FUNCTION__, __FILE__, __LINE__);
}

uint GLES::Texture::raw() {
    return tex;
}

Result GLES::Texture::fill() {
    if (cfg.cudaInterop) {
#ifdef RENDER_CUDA_INTEROP_AVAILABLE
        size_t i = (cfg.pfmt == PixelFormat::RED) ? 1 : 3;
        size_t n = cfg.width * cfg.height * i * ((cfg.ptype == PixelType::F32) ? sizeof(float) : sizeof(uint));
        // this is undefined behavior, but mapping the resouce every loop is too slow
        cudaGraphicsMapResources(1, &cuda_tex_resource);
        cudaGraphicsSubResourceGetMappedArray(&texture_ptr, cuda_tex_resource, 0, 0);
        cudaMemcpyToArray(texture_ptr, 0, 0, cfg.buffer, n, cudaMemcpyDeviceToDevice);
        cudaGraphicsUnmapResources(1, &cuda_tex_resource);
#endif
    } else {
        RENDER_ASSERT_SUCCESS(this->start());
        glTexImage2D(GL_TEXTURE_2D, 0, dfmt, cfg.width, cfg.height, 0, pfmt, ptype, cfg.buffer);
        RENDER_ASSERT_SUCCESS(this->end());
    }

    return GLES::getError(__FUNCTION__, __FILE__, __LINE__);
}

Result GLES::Texture::fill(int yo, int xo, int w, int h) {
    RENDER_ASSERT_SUCCESS(this->start());
    size_t offset = yo * w * ((cfg.ptype == PixelType::F32) ? sizeof(float) : sizeof(uint));
    glTexSubImage2D(GL_TEXTURE_2D, 0, xo, yo, w, h, pfmt, ptype, cfg.buffer + offset);
    RENDER_ASSERT_SUCCESS(this->end());

    return GLES::getError(__FUNCTION__, __FILE__, __LINE__);
}

Result GLES::Texture::pour() {
    // TBD
    return GLES::getError(__FUNCTION__, __FILE__, __LINE__);
}

} // namespace Render
