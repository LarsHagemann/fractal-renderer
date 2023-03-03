#include <cstdio>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <semaphore>

#include <SFML/Graphics.hpp>

#include <imgui-SFML.h>
#include <imgui.h>

#include "timer.hpp"
#include "gradient.hpp"

#ifndef PARALLEL_COUNT
#define PARALLEL_COUNT 4
#endif

bool s_EnableTimerOutput = false;

struct RenderContext {
    sf::Image   Image;
    sf::Texture Texture;
    sf::Sprite  Sprite;
    sf::Uint8*  Pixels = nullptr;
    sf::View    View;
    uint32_t    Width  = 0;
    uint32_t    Height = 0;
    Gradient    DrawGradient;
};

using FType   = double;
using complex = sf::Vector2<FType>;

static sf::Vector2f s_C = { -0.8f, 0.4f };
static uint32_t s_MaxIterations = 100;

static bool s_ApplySmoothing = true;
static bool s_IsRunning      = true;

std::vector<std::jthread>     s_Threads;
static constexpr auto         s_NumThreads = 16u;
static std::counting_semaphore<s_NumThreads> s_ThreadsFinished{ 0 };
static std::counting_semaphore<s_NumThreads> s_ThreadsReady{ 0 };

constexpr static auto s_BufferSize = 2048u;
static char           s_Buffer[s_BufferSize];

void DrawPixelFunction(FType, FType, sf::Uint8*, const RenderContext&);

void ResizeRenderTexture(RenderContext* context, uint32_t width, uint32_t height) {
    Timer t{ "ResizeRenderTexture" };
    if (context->Pixels) {
        delete[] context->Pixels;
        context->Pixels = nullptr;
    }

    context->Pixels = new sf::Uint8[width * height * 4];
    context->Width = width;
    context->Height = height;

    context->View.reset(sf::FloatRect{ 0.f, 0.f, static_cast<float>(width), static_cast<float>(height) });
}

void UpdateRenderContext(RenderContext&, size_t);
void RenderWorker(RenderContext& context, size_t index);

void DrawRenderContext(RenderContext& context, sf::RenderTarget* target) {
    Timer t{ "DrawRenderContext" };
    context.Image.create(context.Width, context.Height, context.Pixels);
    context.Texture.loadFromImage(context.Image);
    context.Sprite.setTexture(context.Texture);
    target->draw(context.Sprite);
}

int main() {
    printf_s("Creating window...\n");
    sf::RenderWindow window{ sf::VideoMode{ 1200, 800 }, "Fractal Renderer" };
    window.setFramerateLimit(30);
    sf::Event event;
    sf::Clock clock;
    sf::Clock frametime;
    uint32_t  fps = 0u;

    printf_s("Parallel count: %u\n", PARALLEL_COUNT);

    RenderContext context{};
    context.DrawGradient.SetDomain({ 0., static_cast<double>(s_MaxIterations) });
    context.DrawGradient.AddKey({ 0., sf::Color::White });
    context.DrawGradient.AddKey({ s_MaxIterations / 3.f, sf::Color::Red });
    context.DrawGradient.AddKey({ 2.f * s_MaxIterations / 3.f, sf::Color::Green });
    context.DrawGradient.AddKey({ s_MaxIterations, sf::Color::Blue });

    ImGui::SFML::Init(window);

    for (auto i = 0u; i < s_NumThreads; ++i) {
        s_Threads.emplace_back(std::jthread(&RenderWorker, std::ref(context), i));
    }

    // Debug print all gradient keys
    for (auto i = 0u; i < context.DrawGradient.GetNumKeys(); ++i) {
        const auto& key = context.DrawGradient.GetKey(i);
        printf_s("Key %u: %f, %u, %u, %u\n", i, key.first, key.second.r, key.second.g, key.second.b);
    }

    ResizeRenderTexture(&context, window.getSize().x, window.getSize().y);

    sf::Vector2i mousePos{ 0, 0 };
    bool         mouseDown = false;

    const auto& io = ImGui::GetIO();

    double zoomCache = 1.;

    printf_s("Starting rendering loop...\n");
    while (window.isOpen()) {
        Timer t{ "Main Loop" };
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);

            if (io.WantCaptureMouse)
                continue;
            
            switch (event.type) {
                case sf::Event::EventType::Closed:
                    window.close();
                    break;
                case sf::Event::EventType::Resized:
                    printf("Resizing...\n");
                    ResizeRenderTexture(&context, event.size.width, event.size.height);
                    break;
                case sf::Event::EventType::KeyPressed:
                    if (event.key.code == sf::Keyboard::Key::Escape) {
                        window.close();
                    }
                    else if (event.key.code == sf::Keyboard::Key::Space) {
                        s_ApplySmoothing = !s_ApplySmoothing;
                    }
                    break;
                case sf::Event::EventType::MouseWheelScrolled:
                    if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
                        const auto delta = event.mouseWheelScroll.delta;
                        const auto x = event.mouseWheelScroll.x;
                        const auto y = event.mouseWheelScroll.y;
                        
                        auto& view = context.View;
                        const auto mouseInWindow = sf::Mouse::getPosition(window);
                        const auto center = view.getCenter();

                        const auto mouseInWorld = window.mapPixelToCoords(mouseInWindow, view);
                        const auto mouseInWorldDelta = mouseInWorld - center;

                        const auto zoom = 1.f + -delta * 0.1f;
                        if (zoom < 1.f) 
                            view.move(mouseInWorldDelta * 0.2f);
                        
                        view.zoom(zoom);
                        zoomCache *= zoom;
                        context.View = view;
                    }
                    break;
                case sf::Event::EventType::MouseMoved:
                    if (mouseDown) {
                        const auto delta = mousePos - sf::Vector2i{ event.mouseMove.x, event.mouseMove.y };
                        auto& view = context.View;
                        view.move(static_cast<double>(delta.x) * zoomCache, static_cast<double>(delta.y) * zoomCache);
                        mousePos = { event.mouseMove.x, event.mouseMove.y };
                    }
                    break;
                case sf::Event::EventType::MouseButtonPressed:
                    if (event.mouseButton.button == sf::Mouse::Button::Left) {
                        mouseDown = true;
                        mousePos = { event.mouseButton.x, event.mouseButton.y };
                    }
                    break;
                case sf::Event::EventType::MouseButtonReleased:
                    if (event.mouseButton.button == sf::Mouse::Button::Left) {
                        mouseDown = false;
                    }
                    break;
            }
        }

        ImGui::SFML::Update(window, frametime.restart());

        ImGui::SetNextWindowPos({ io.DisplaySize.x * 0.7f, 0.f }, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ io.DisplaySize.x * 0.3f, io.DisplaySize.y }, ImGuiCond_Always);
        if (ImGui::Begin("Fractal Renderer")) {
            const auto& view = context.View;
            if (ImGui::TreeNode("View")) {
                ImGui::Text("Center: %f, %f", view.getCenter().x, view.getCenter().y);
                ImGui::Text("Size: %f, %f", view.getSize().x, view.getSize().y);
                ImGui::Text("Zoom: %.18f", zoomCache);
                ImGui::Text("Float epsilon : %.18f", std::numeric_limits<float>::epsilon());
                ImGui::Text("Double epsilon: %.18f", std::numeric_limits<double>::epsilon());

                ImGui::TreePop();
            }

            ImGui::Checkbox("Debug output", &s_EnableTimerOutput);
            ImGui::Checkbox("Apply smoothing", &s_ApplySmoothing);
            ImGui::DragFloat2("C", &s_C.x, 0.01f, -1.f, 1.f);
            auto iMax = static_cast<int32_t>(s_MaxIterations);
            if (ImGui::DragInt("Max iterations", &iMax, 1.f, 1, 1000)) {
                s_MaxIterations = iMax;
                context.DrawGradient.SetDomain({ 0., static_cast<double>(s_MaxIterations) }, false);
            }

            ImGui::InputTextMultiline("hi", s_Buffer, s_BufferSize);
        }
        ImGui::End();
        
        window.clear();

        s_ThreadsReady.release(s_NumThreads);

        auto numThreadsFinished = 0;
        while (numThreadsFinished < s_NumThreads) {
            s_ThreadsFinished.acquire();
            ++numThreadsFinished;
        }

        DrawRenderContext(context, &window);

        ImGui::SFML::Render(window);
        window.display();

        if (clock.getElapsedTime().asSeconds() > 1.f) {
            window.setTitle("Fractal Renderer (" + std::to_string(fps) + "FPS)");
            fps = 0;
            clock.restart();
        }
        ++fps;
    }
    printf_s("Cleanup...\n");

    s_IsRunning = false;

    if (context.Pixels) {
        delete[] context.Pixels;
        context.Pixels = nullptr;
    }

    ImGui::SFML::Shutdown();
}

complex ComplexModifier(complex z) {
    // compute z²+c
    const auto z2 = complex{ z.x * z.x - z.y * z.y, 2.f * z.x * z.y };
    return z2 + complex(s_C);
}

FType LengthSquared(complex z) {
    return z.x * z.x + z.y * z.y;
}

template<typename T>
T map(T value, T x0, T y0, T x1, T y1) {
    const auto p = 
        static_cast<FType>(value - x0) / static_cast<FType>(y0 - x0);
    
    return static_cast<T>(p * (y1 - x1)) + x1;
}

void RenderWorker(RenderContext& context, size_t index) {
    while (s_IsRunning) {
        Timer t("RenderWorker: " + std::to_string(index));
        while (!s_ThreadsReady.try_acquire_for(std::chrono::milliseconds(10))) {
            if (!s_IsRunning) {
                return;
            }
        }

        UpdateRenderContext(context, index);
        s_ThreadsFinished.release();
    }
}

void UpdateRenderContext(RenderContext& context, size_t workerIndex) {
    const auto view = context.View;
    const auto viewLeft = static_cast<FType>(view.getCenter().x - view.getSize().x * 0.5f);
    const auto viewTop = static_cast<FType>(view.getCenter().y - view.getSize().y * 0.5f);

    const auto ySliceBegin = (context.Height / s_NumThreads) * workerIndex;
    const auto ySliceEnd = (context.Height / s_NumThreads) * (workerIndex + 1);

    for (auto y = ySliceBegin; y < ySliceEnd; ++y) {
        for (auto x = 0; x < context.Width; ++x) {
            // Convert screen-space coordinates to view-space coordinates
            const auto viewX = map(static_cast<FType>(x), FType(0.f), static_cast<FType>(context.Width), viewLeft, viewLeft + view.getSize().x);
            const auto viewY = map(static_cast<FType>(y), FType(0.f), static_cast<FType>(context.Height), viewTop, viewTop + view.getSize().y);

            const auto index = (y * context.Width + x) * 4;
            DrawPixelFunction(viewY, viewX, &context.Pixels[index], context);
        }
    }
}

void DrawPixelFunction(FType x, FType y, sf::Uint8* output, const RenderContext& context) {
    const auto scale = 1.f / FType(context.Height / 2.f);
    auto z = complex{
        (y - context.Height / 2.f) * scale,
        (x - context.Width / 2.f) * scale 
    };
    uint32_t i;
    for (i = 0u; i < s_MaxIterations; ++i) {
        z = ComplexModifier(z);
        if (LengthSquared(z) >= 4.f) {
            break;
        }
    }

    if (s_ApplySmoothing) {
        const auto length = std::sqrt(LengthSquared(z));
        const auto smoothIteration = FType(i) - std::log2(std::max(std::log2(length), static_cast<FType>(1.)));
        const auto color = context.DrawGradient.GetColor(smoothIteration);

        output[0] = color.r;
        output[1] = color.g;
        output[2] = color.b;
        output[3] = color.a;
        return;
    }
    else {
        const auto color = context.DrawGradient.GetColor(i);
        output[0] = color.r;
        output[1] = color.g;
        output[2] = color.b;
        output[3] = color.a;
        return;
    }
}
