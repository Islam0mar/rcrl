#include <SDL.h>
#include <third_party/ImGuiColorTextEdit/TextEditor.h>
#include <third_party/imgui/backends/imgui_impl_sdl.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include "host_app.h"
#include "image.hpp"
#include "imgui.h"
#include "opengl.hpp"
#include "rcrl/rcrl.h"

using std::cerr;
using std::cout;
using std::endl;
using std::string;
int main() {
  bool console_visible = true;
  // Compiler
  char flags[255] = "";
  std::vector<string> args = {"-std=c++17", "-O0",    "-Wall",
                              "-Wextra",    "-ggdb3", "-lm"};
  rcrl::Plugin compiler("plugin", args);

  // Setup SDL
  // (Some versions of SDL before <2.0.10 appears to have performance/stalling
  // issues on a minority of Windows systems, depending on whether
  // SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version
  // of SDL is recommended!)
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) !=
      0) {
    printf("Error: %s\n", SDL_GetError());
    return -1;
  }

  // Decide GL+GLSL versions
#ifdef __APPLE__
  // GL 3.2 Core + GLSL 150
  const char *glsl_version = "#version 150";
  SDL_GL_SetAttribute(
      SDL_GL_CONTEXT_FLAGS,
      SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);  // Always required on Mac
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
  // GL 3.0 + GLSL 130
  const char *glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_WindowFlags window_flags = (SDL_WindowFlags)(
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Window *window = SDL_CreateWindow(
      "Read-Compile-Run-Loop - REPL for C++", SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, 1280, 1024, window_flags);
  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1);  // Enable vsync

  if (LoadOpenGL()) {
    fprintf(stderr, "Failed to initialize OpenGL loader!\n");
    return 1;
  }

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();

  // Setup Dear ImGui style
  ImGui::StyleColorsClassic();

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // remove rounding of the console window
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 0.f;

  io.Fonts->AddFontFromFileTTF(
      CMAKE_SOURCE_DIR "/src/third_party/imgui/misc/fonts/Cousine-Regular.ttf",
      17.f);

  // an editor instance - for the already submitted code
  TextEditor history;
  history.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
  history.SetReadOnly(true);

  // an editor instance - compiler output - with an empty language definition
  // and a custom palette
  TextEditor compiler_output;
  compiler_output.SetLanguageDefinition(TextEditor::LanguageDefinition());
  compiler_output.SetReadOnly(true);
  auto custom_palette = TextEditor::GetDarkPalette();
  custom_palette[(int)TextEditor::PaletteIndex::MultiLineComment] =
      0xcccccccc;  // text is treated as such by default
  compiler_output.SetPalette(custom_palette);

  // holds the standard output from while loading the plugin - same as the
  // compiler output as a theme
  TextEditor program_output(compiler_output);

  // an editor instance - for the core being currently written
  TextEditor editor;
  editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());

  // set some initial code
  editor.SetText(R"raw(
// global
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <utility>
#include <vector>

#include "host_app.h"

std::cout << "Hello world!" << std::endl;
// FIXME: use std::
auto getVec() { return vector<int>({1, 2, 3}); }
auto vec = getVec();
std::cout << vec.size() << std::endl;
)raw");

  // holds the exit code from the last compilation - there was an error when not
  int last_compiler_exitcode = 0;

  // limiting to 50 fps because on some systems the whole machine started
  // lagging when the demo was turned on
  using frames = std::chrono::duration<int64_t, std::ratio<1, 60>>;
  auto nextFrame = std::chrono::system_clock::now() + frames{0};

  // add objects in scene
  for (int i = 0; i < 4; ++i) {
    for (int k = 0; k < 4; ++k) {
      auto &obj = addObject(-7.5f + k * 5, -4.5f + i * 3);
      obj.colorize(float(i % 2), float(k % 2), 0);
    }
  }

  // Use loading image which will be displayed while compiling
  int my_image_width = 512;
  int my_image_height = 512;
  GLuint my_image_texture = 0;
  bool ret = GetTextureFromCStructImage(&my_image_texture, &my_image_width,
                                        &my_image_height);
  IM_ASSERT(ret);

  // main loop
  bool done = false;
  while (!done) {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to
    // tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to
    // your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input
    // data to your main application. Generally you may always pass all inputs
    // to dear imgui, and hide them from your application based on those two
    // flags.
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT) done = true;
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_CLOSE &&
          event.window.windowID == SDL_GetWindowID(window))
        done = true;
    }

    // console toggle
    // should be called before ImGui::NewFrame()
    if (!io.WantCaptureKeyboard && !io.WantTextInput &&
        ImGui::IsKeyPressed(SDL_SCANCODE_GRAVE, true)) {
      console_visible = !console_visible;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();

    // handle window stretching
    int window_w, window_h;
    SDL_GetWindowSize(window, &window_w, &window_h);

    // console should be always fixed
    ImGui::SetNextWindowSize({(float)window_w, -1.f}, ImGuiCond_Always);
    ImGui::SetNextWindowPos({0.f, 0.f}, ImGuiCond_Always);

    // sets breakpoints on the program_output instance of the text editor widget
    // - used to highlight new output
    auto do_breakpoints_on_output = [&](int old_line_count,
                                        const std::string &new_output) {
      TextEditor::Breakpoints bps;
      if (old_line_count == program_output.GetTotalLines() && new_output.size())
        bps.insert(old_line_count);
      for (auto curr_line = old_line_count;
           curr_line < program_output.GetTotalLines(); ++curr_line)
        bps.insert(curr_line);
      program_output.SetBreakpoints(bps);
    };

    // console setup
    if (console_visible &&
        ImGui::Begin("console", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove)) {
      const auto text_field_height = ImGui::GetTextLineHeight() * 14;
      const float left_right_ratio = 0.45f;
      // top left part
      ImGui::BeginChild("history code",
                        ImVec2(window_w * left_right_ratio, text_field_height));
      auto hcpos = history.GetCursorPosition();
      ImGui::Text("Executed code: %3d/%-3d %3d lines", hcpos.mLine + 1,
                  hcpos.mColumn + 1, editor.GetTotalLines());
      history.Render("History");
      ImGui::EndChild();
      ImGui::SameLine();
      // top right part
      ImGui::BeginChild("compiler output", ImVec2(0, text_field_height));
      auto new_output = compiler.get_new_compiler_output();
      if (new_output.size()) {
        auto total_output = compiler_output.GetText() + new_output;

        // scan for errors through the lines and highlight them with markers
        auto curr_pos = 0;
        auto line = 1;
        auto first_error_marker_line = 0;
        TextEditor::ErrorMarkers error_markers;
        do {
          auto new_curr_pos_1 = total_output.find(
              "error", curr_pos + 1);  // add 1 to skip new lines
          auto new_curr_pos_2 =
              total_output.find("\n", curr_pos + 1);  // add 1 to skip new lines
          if (new_curr_pos_1 < new_curr_pos_2) {
            error_markers.insert(std::make_pair(line, ""));
            if (!first_error_marker_line) first_error_marker_line = line;
          }
          if (new_curr_pos_2 < new_curr_pos_1) {
            line++;
          }
          curr_pos = std::min(new_curr_pos_1, new_curr_pos_2);
        } while (size_t(curr_pos) != string::npos);
        compiler_output.SetErrorMarkers(error_markers);

        // update compiler output
        compiler_output.SetText(move(total_output));
        if (first_error_marker_line)
          compiler_output.SetCursorPosition({first_error_marker_line, 1});
        else
          compiler_output.SetCursorPosition(
              {compiler_output.GetTotalLines(), 1});
      }
      if (last_compiler_exitcode)
        ImGui::TextColored({1, 0, 0, 1}, "Compiler output - ERROR!");
      else
        ImGui::Text("Compiler output:        ");
      ImGui::SameLine();
      auto cocpos = compiler_output.GetCursorPosition();
      ImGui::Text("%3d/%-3d %3d lines", cocpos.mLine + 1, cocpos.mColumn + 1,
                  compiler_output.GetTotalLines());
      compiler_output.Render("Compiler output");
      ImGui::EndChild();

      // bottom left part
      ImGui::BeginChild("source code",
                        ImVec2(window_w * left_right_ratio, text_field_height));
      auto ecpos = editor.GetCursorPosition();
      ImGui::Text("RCRL Console: %3d/%-3d %3d lines | %s", ecpos.mLine + 1,
                  ecpos.mColumn + 1, editor.GetTotalLines(),
                  editor.CanUndo() ? "*" : " ");
      editor.Render("Code");
      ImGui::EndChild();
      ImGui::SameLine();
      // bottom right part
      ImGui::BeginChild("program output", ImVec2(0, text_field_height));
      auto ocpos = program_output.GetCursorPosition();
      ImGui::Text("Program output: %3d/%-3d %3d lines", ocpos.mLine + 1,
                  ocpos.mColumn + 1, program_output.GetTotalLines());
      program_output.Render("Output");
      ImGui::EndChild();

      // bottom buttons
      // loading image to indicate that we are compiling
      if (compiler.IsCompiling()) {
        constexpr float angular_velocity = 10.0f;
        static float t = 0.0f;
        static const float scale = 1.2f;
        static const float angles[] = {0,
                                       45 * M_PI / 180.0f,
                                       90 * M_PI / 180.0f,
                                       135 * M_PI / 180.0f,
                                       180 * M_PI / 180.0f,
                                       225 * M_PI / 180.0f,
                                       270 * M_PI / 180.0f,
                                       315 * M_PI / 180.0f};
        ImVec2 p = ImGui::GetCursorScreenPos();
        t += io.DeltaTime;
        ImageRotated((void *)(intptr_t)my_image_texture,
                     ImVec2(p.x + ImGui::GetTextLineHeight() * scale / 2.0f,
                            p.y + ImGui::GetTextLineHeight() / 2.0f - 1),
                     ImVec2(ImGui::GetTextLineHeight() * scale,
                            ImGui::GetTextLineHeight() * scale),
                     angles[((int)(t * angular_velocity)) % 8]);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             ImGui::GetTextLineHeight() * scale + 5);
      }
      // input for compiler flags with button to set it
      ImGui::Text("Compiler flags:");
      ImGui::SameLine();
      ImGui::PushItemWidth(ImGui::GetTextLineHeight() * 10.0);
      ImGui::InputText("", flags, 255);
      ImGui::SameLine();
      if (ImGui::Button("Set compiler flags")) {
        size_t start;
        size_t end = 0;
        auto str = string(flags);
        args.clear();
        while ((start = str.find_first_not_of(' ', end)) != std::string::npos) {
          end = str.find(' ', start);
          args.emplace_back(str.substr(start, end - start));
        }
        compiler.set_flags(args);
      }
      ImGui::SameLine();
      auto compile = ImGui::Button("Compile and run");
      ImGui::SameLine();
      if (ImGui::Button("Cleanup Plugins") && !compiler.IsCompiling()) {
        compiler_output.SetText("");
        auto output_from_cleanup = compiler.CleanupPlugins(true);
        auto old_line_count = program_output.GetTotalLines();
        program_output.SetText(program_output.GetText() + output_from_cleanup);
        program_output.SetCursorPosition({program_output.GetTotalLines(), 0});

        last_compiler_exitcode = 0;

        // highlight the new stdout lines
        do_breakpoints_on_output(old_line_count, output_from_cleanup);
      }
      ImGui::SameLine();
      if (ImGui::Button("Clear Output")) program_output.SetText("");
      ImGui::SameLine();
      ImGui::Dummy({20, 0});
      ImGui::SameLine();
      ImGui::Text("Use Ctrl+Enter to submit code");
      compile |= (io.KeysDown[SDL_SCANCODE_RETURN] && io.KeyCtrl);
      if (compile && !compiler.IsCompiling() && editor.GetText().size() > 1) {
        // clear compiler output
        compiler_output.SetText("");
        // submit to the RCRL engine
        if (compiler.CompileCode(editor.GetText())) {
          // make the editor code untouchable while compiling
          editor.SetReadOnly(true);
        } else {
          last_compiler_exitcode = 1;
        }
      }
      ImGui::End();
    }

    // if there is a spawned compiler process and it has just finished
    if (compiler.TryGetExitStatusFromCompile(last_compiler_exitcode)) {
      // we can edit the code again
      editor.SetReadOnly(false);

      if (last_compiler_exitcode) {
        // errors occurred - set cursor to the last line of the erroneous code
        editor.SetCursorPosition({editor.GetTotalLines(), 0});
      } else {
        // append to the history and focus last line
        history.SetCursorPosition({history.GetTotalLines(), 1});
        auto history_text = history.GetText();
        // add a new line (if one is missing) to the code that will go to the
        // history for readability
        if (history_text.size() && history_text.back() != '\n')
          history_text += '\n';
        history.SetText(history_text + editor.GetText());

        // load the new plugin
        auto output_from_loading = compiler.CopyAndLoadNewPlugin(true);
        auto old_line_count = program_output.GetTotalLines();
        program_output.SetText(program_output.GetText() + output_from_loading);
        // TODO: used for auto-scroll but the cursor in the editor is removed
        // (unfocused) sometimes from this...
        // program_output.SetCursorPosition({program_output.GetTotalLines(),
        // 0});

        // highlight the new stdout lines
        do_breakpoints_on_output(old_line_count, output_from_loading);

        // clear the editor
        editor.SetText(
            "\r");  // an empty string "" breaks it for some reason...
        editor.SetCursorPosition({0, 0});
      }
    }

    // rendering
    ImGui::Render();
    glViewport(0, 0, window_w, window_h);
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);

    glPushMatrix();
    glLoadIdentity();
    glScalef(0.1f, 0.1f * window_w / window_h, 0.1f);

    for (auto &obj : getObjects()) obj.draw();

    glPopMatrix();

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);

    // do the frame rate limiting
    std::this_thread::sleep_until(nextFrame);
    nextFrame += frames{1};
  }

  // cleanup
  compiler.CleanupPlugins();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
