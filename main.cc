#define RENDER_DEBUG

#include <iostream>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "render/base.hpp"

#include "samurai/samurai.hpp"

#include "jetstream/fft/base.hpp"
#include "jetstream/lineplot/base.hpp"

using namespace Samurai;

ChannelId rx;
auto device = Airspy::Device();
std::shared_ptr<Render::Instance> render;

std::shared_ptr<Jetstream::FFT::Generic> fft;
std::shared_ptr<Jetstream::Lineplot::Generic> lpt;

std::shared_ptr<std::vector<std::complex<float>>> input;

void render_loop() {
    render->start();

    ASSERT_SUCCESS(device.ReadStream(rx, input->data(), input->size(), 1000));

    fft->compute();
    lpt->compute(fft);
    lpt->present();

    ImGui::Begin("Lineplot");
    ImGui::GetWindowDrawList()->AddImage(
        (void*)lpt->tex()->raw(), ImVec2(ImGui::GetCursorScreenPos()),
        ImVec2(ImGui::GetCursorScreenPos().x + lpt->conf().width/2.0,
        ImGui::GetCursorScreenPos().y + lpt->conf().height/2.0), ImVec2(0, 1), ImVec2(1, 0));
    ImGui::End();

    ImGui::Begin("Samurai Info");
    ImGui::Text("Buffer Fill: %ld", device.BufferOccupancy(rx));
    ImGui::End();

    render->end();
}

int main() {
    std::cout << "Welcome to CyberEther!" << std::endl;

    Render::Instance::Config renderCfg;
    renderCfg.width = 3000;
    renderCfg.height = 1080;
    renderCfg.resizable = true;
    renderCfg.enableImgui = true;
    renderCfg.enableDebug = true;
    renderCfg.enableVsync = false;
    renderCfg.title = "CyberEther";
    render = Render::Instantiate(Render::API::GLES, renderCfg);

    Device::Config deviceConfig{};
    deviceConfig.sampleRate = 10e6;
    device.Enable(deviceConfig);

    Channel::Config channelConfig{};
    channelConfig.mode = Mode::RX;
    channelConfig.dataFmt = Format::F32;
    ASSERT_SUCCESS(device.EnableChannel(channelConfig, &rx));

    Channel::State channelState{};
    channelState.enableAGC = true;
    channelState.frequency = 545.5e6;
    ASSERT_SUCCESS(device.UpdateChannel(rx, channelState));

    input = std::make_shared<std::vector<std::complex<float>>>();
    input->resize(221184);

    Jetstream::FFT::Config fftCfg;
    fftCfg.input = input;
    fft = std::make_shared<Jetstream::FFT::CPU>(fftCfg);

    Jetstream::Lineplot::Config lptCfg;
    lptCfg.input = fftCfg.output;
    lptCfg.render = render;
    lpt = std::make_shared<Jetstream::Lineplot::CPU>(lptCfg);

    render->create();
    device.StartStream();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(render_loop, 0, 1);
#else
    while(render->keepRunning())
        render_loop();
#endif

    device.StopStream();
    render->destroy();

    std::cout << "Goodbye from CyberEther!" << std::endl;
}
