#ifndef RENDER_GLES_INSTANCE_H
#define RENDER_GLES_INSTANCE_H

#define GLFW_INCLUDE_ES3
#include <GLFW/glfw3.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "render/base/instance.hpp"

namespace Render {

class GLES : public Render::Instance {
public:
    class Program;
	class Surface;
    class Texture;

    GLES(Config& c) : Render::Instance(c) {};

    Result init();
    Result terminate();

    Result clear();
    Result draw();
    Result step();

    bool keepRunning();

    std::string renderer_str();
    std::string version_str();
    std::string vendor_str();
    std::string glsl_str();

protected:
    ImGuiIO* io;
    ImGuiStyle* style;

    uint vao, vbo, ebo;

    Result createBuffers();
    Result destroyBuffers();

    Result createImgui();
    Result destroyImgui();
    Result startImgui();
    Result endImgui();

    static Result getError(std::string, std::string, int);
    
    std::string cached_renderer_str;
    std::string cached_version_str;
    std::string cached_vendor_str;
    std::string cached_glsl_str;
};

struct State : Render::GLES {
    GLFWwindow* window;
};

} // namespace Render

#endif