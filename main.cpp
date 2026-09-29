#include <SDL3/SDL.h>
#include <GLES3/gl3.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

#include <nlohmann/json.hpp>

#include <SDL3/SDL_video.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

struct MenuEntry {
    std::string label;
    std::vector<std::string> argv;
};

static std::string configPath() 
{
    if (const char *env = std::getenv("KIOSK_LAUNCHER_CONFIG")) {
        return env;
    }
    return "/etc/kiosk-launcher/menu.json";
}

static std::vector<MenuEntry> loadMenu(const std::string &path, std::string &error)
{
    std::vector<MenuEntry> result;

    std::ifstream f(path);
    if (!f.is_open()) {
        error = "not open " + path;
        return result;
    }

    nlohmann::json j;
    try {
        f >> j;
    } catch (const nlohmann::json::parse_error &e) {
        error = "error parse JSON: " + std::string(e.what());
        return result;
    }

    if (!j.is_array()) {
        error = "config must be JSON array";
        return result;
    }

    for (const auto &item : j) {
        MenuEntry entry;

        entry.label = item.value("label", "");
        if (entry.label.empty()) {
            continue;
        }
        if (item.contains("exec") && item["exec"].is_array()) {
            for (const auto &arg : item["exec"]) {
                if (arg.is_string()) {
                    entry.argv.push_back(arg.get<std::string>());
                }
            }
        }
        if (entry.argv.empty()) {
            continue;
        }
        result.push_back(std::move(entry));
    }

    if (result.empty() && error.empty()) {
        error = "config read, but not getted valid row";
    }
    return result;
}

extern char **environ;

[[noreturn]] static void launch(const MenuEntry &entry)
{
    std::vector<char *> argv;
    argv.reserve(entry.argv.size() + 1);
    for (const auto &a : entry.argv) {
        argv.push_back(const_cast<char *>(a.c_str()));
    }
    argv.push_back(nullptr);

    execve(argv[0], argv.data(), environ);

    std::fprintf(stderr, "execve(%s) failed: %s\n", argv[0], strerror(errno));
    std::exit(1);
}

int main(int, char **) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_Window *window = SDL_CreateWindow(
        "kiosk-launcher", 1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    if (!gl_ctx) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(1);

    int gamepad_count = 0;
    SDL_JoystickID *gamepads = SDL_GetGamepads(&gamepad_count);
    SDL_Gamepad *gamepad = nullptr;
    if (gamepads && gamepad_count > 0) {
        gamepad = SDL_OpenGamepad(gamepads[0]);
    }
    if (gamepads) SDL_free(gamepads);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 300 es");

    std::string menuError;
    std::vector<MenuEntry> menu = loadMenu(configPath(), menuError);

    bool running = true;
    const MenuEntry *chosen = nullptr;

    while (running && !chosen) {
        SDL_Event event;
        while (SDL_PollEvent (&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
        ImGui::Begin("##menu", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoSavedSettings);

        if (!menuError.empty()) {
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                    "Error menu config");
            ImGui::TextWrapped("%s", menuError.c_str());
            ImGui::Spacing();
            ImGui::Text("Path: %s", configPath().c_str());
        } else {
            const float btn_w = 400, btn_h = 100, gap = 20;
            const float total_h = menu.size() * btn_h + (menu.size() - 1) * gap;
            float y = (h - total_h) * 0.5f;
            static bool nav_init = false;

            for (size_t i = 0; i < menu.size(); ++i) {
                ImGui::SetCursorPos(ImVec2((w - btn_w) * 0.5f, y));
                if (!nav_init && i == 0) {
                    ImGui::SetKeyboardFocusHere(0);
                }
                std::string id = menu[i].label + "##" + std::to_string(i);
                if (ImGui::Button (id.c_str(), ImVec2 (btn_w, btn_h))) {
                    chosen = &menu[i];
                }
                y += btn_h + gap;
            }
            nav_init = true;
        }

        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, w, h);
        glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    if (chosen) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        if (gamepad) SDL_CloseGamepad(gamepad);
        SDL_GL_DestroyContext(gl_ctx);
        SDL_DestroyWindow(window);
        SDL_Quit();

        launch(*chosen);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    if (gamepad) SDL_CloseGamepad(gamepad);
    SDL_GL_DestroyContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
