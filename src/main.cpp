#include <GLFW/glfw3.h>
#include <third_party/ImGuiColorTextEdit/TextEditor.h>
#include <third_party/imgui/examples/opengl2_example/imgui_impl_glfw_gl2.h>

#include <chrono>
#include <iostream>
#include <list>
#include <thread>

#include "host_app.h"
#include "rcrl/debug.hpp"
#include "rcrl/rcrl.h"

using namespace std;

#define RCRL_LIVE_DEMO 0

bool g_console_visible = !RCRL_LIVE_DEMO;

// my own callback - need to add the new line symbols to make ImGuiColorTextEdit
// work when 'enter' is pressed
void My_ImGui_ImplGlfwGL2_KeyCallback(GLFWwindow* w, int key, int scancode,
                                      int action, int mods) {
  // calling the callback from the imgui/glfw integration only if not a dash
  // because when writing an underscore (with shift down) ImGuiColorTextEdit
  // does a paste - see this for more info:
  // https://github.com/BalazsJako/ImGuiColorTextEdit/issues/18
  if (key != '-') ImGui_ImplGlfwGL2_KeyCallback(w, key, scancode, action, mods);

  // add the '\n' char when 'enter' is pressed - for ImGuiColorTextEdit
  ImGuiIO& io = ImGui::GetIO();
  if (key == GLFW_KEY_ENTER && !io.KeyCtrl &&
      (action == GLFW_PRESS || action == GLFW_REPEAT))
    io.AddInputCharacter((unsigned short)'\n');

  // console toggle
  if (!io.WantCaptureKeyboard && !io.WantTextInput &&
      key == GLFW_KEY_GRAVE_ACCENT &&
      (action == GLFW_PRESS || action == GLFW_REPEAT))
    g_console_visible = !g_console_visible;
}

int main() {
  // Compiler
  char flags[255] = "";
  std::vector<string> args = {"-std=c++17", "-O0",    "-Wall",
                              "-Wextra",    "-ggdb3", "-lm"};
  rcrl::Plugin compiler("plugin", args);
  // Setup window
  glfwSetErrorCallback([](int error, const char* description) {
    fprintf(stderr, "%d %s", error, description);
  });
  if (!glfwInit()) return 1;

  GLFWwindow* window = glfwCreateWindow(
      1280, 1024, "Read-Compile-Run-Loop - REPL for C++", nullptr, nullptr);
  glfwMakeContextCurrent(window);

  // Setup ImGui binding
  ImGui::CreateContext();
  ImGui_ImplGlfwGL2_Init(window, true);

  // remove rounding of the console window
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 0.f;

  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->AddFontFromFileTTF(
      CMAKE_SOURCE_DIR "/src/third_party/imgui/misc/fonts/Cousine-Regular.ttf",
      17.f);

  // overwrite with my own callback
  glfwSetKeyCallback(window, My_ImGui_ImplGlfwGL2_KeyCallback);

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

#if !RCRL_LIVE_DEMO
  // set some initial code
  editor.SetText(R"raw(// global
// global
#include <iostream>

#include "host_app.h"
using namespace std;
// once
cout << "Hello world!" << endl;
// global
#include <vector>
auto getVec() { return vector<int>({1, 2, 3}); }
// vars
auto vec = getVec();
// once
cout << vec.size() << endl;
)raw");
#endif  // RCRL_LIVE_DEMO

  // holds the exit code from the last compilation - there was an error when not
  // 0
  int last_compiler_exitcode = 0;

  // limiting to 50 fps because on some systems the whole machine started
  // lagging when the demo was turned on
  using frames = chrono::duration<int64_t, ratio<1, 60>>;
  auto nextFrame = chrono::system_clock::now() + frames{0};

  // add objects in scene
  for (int i = 0; i < 4; ++i) {
    for (int k = 0; k < 4; ++k) {
      auto& obj = addObject(-7.5f + k * 5, -4.5f + i * 3);
      obj.colorize(float(i % 2), float(k % 2), 0);
    }
  }

  // main loop
  while (!glfwWindowShouldClose(window)) {
    // poll for events
    glfwPollEvents();
    ImGui_ImplGlfwGL2_NewFrame();

    // handle window stretching
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    int window_w, window_h;
    glfwGetWindowSize(window, &window_w, &window_h);

    // console should be always fixed
    ImGui::SetNextWindowSize({(float)window_w, -1.f}, ImGuiCond_Always);
    ImGui::SetNextWindowPos({0.f, 0.f}, ImGuiCond_Always);

    // sets breakpoints on the program_output instance of the text editor widget
    // - used to highlight new output
    auto do_breakpoints_on_output = [&](int old_line_count,
                                        const std::string& new_output) {
      TextEditor::Breakpoints bps;
      if (old_line_count == program_output.GetTotalLines() && new_output.size())
        bps.insert(old_line_count);
      for (auto curr_line = old_line_count;
           curr_line < program_output.GetTotalLines(); ++curr_line)
        bps.insert(curr_line);
      program_output.SetBreakpoints(bps);
    };

    if (g_console_visible &&
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
            error_markers.insert(make_pair(line, ""));
            if (!first_error_marker_line) first_error_marker_line = line;
          }
          if (new_curr_pos_2 < new_curr_pos_1) {
            line++;
          }
          curr_pos = min(new_curr_pos_1, new_curr_pos_2);
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
      // ImGui::Text("Compiler flags:");
      // ImGui::SameLine();
      ImGui::PushItemWidth(ImGui::GetTextLineHeight() * 10.0);
      ImGui::InputText("Compiler flags:", flags, 255);
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
#if !RCRL_LIVE_DEMO
      ImGui::Text("Use Ctrl+Enter to submit code");
#endif  // RCRL_LIVE_DEMO

      // if the user has submitted code for compilation
#if RCRL_LIVE_DEMO
      extern std::list<const char*> fragments;
      static bool fragment_popped = false;
      if (!compiler.IsCompiling() && !fragment_popped && fragments.size() &&
          io.KeysDown[GLFW_KEY_ENTER] && io.KeyCtrl) {
        editor.SetText(fragments.front());
        fragments.pop_front();
        fragment_popped = true;
      }
#else   // RCRL_LIVE_DEMO
      compile |= (io.KeysDown[GLFW_KEY_ENTER] && io.KeyCtrl);
#endif  // RCRL_LIVE_DEMO
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
#if RCRL_LIVE_DEMO
        fragment_popped = false;
#endif  // RCRL_LIVE_DEMO
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
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);

    glPushMatrix();
    glLoadIdentity();
    glScalef(0.1f, 0.1f * display_w / display_h, 0.1f);

    for (auto& obj : getObjects()) obj.draw();

    glPopMatrix();

    // finalize rendering
    ImGui::Render();
    ImGui_ImplGlfwGL2_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);

    // do the frame rate limiting
    this_thread::sleep_until(nextFrame);
    nextFrame += frames{1};
  }

  // cleanup
  compiler.CleanupPlugins();
  ImGui_ImplGlfwGL2_Shutdown();
  ImGui::DestroyContext();
  glfwTerminate();

  return 0;
}

#if RCRL_LIVE_DEMO
std::list<const char*> fragments = {
    R"raw(// global
#include "host_app.h"
)raw", R"raw(// vars
auto& obj = addObject(0, -2);
)raw", R"raw(// once
obj.colorize(1, 1, 1);
)raw",
    R"raw(// once
obj.translate(0, -4);
obj.set_speed(7);
)raw",   R"raw(// once
for(auto& curr : getObjects())
    curr.set_speed(0.1);
)raw", R"raw(// global
struct MyType {
    MyType() { cout << "hello" << endl; }
    ~MyType() { cout << "bye" << endl; }
};

// vars
MyType asd;
)raw"};
#endif  // RCRL_LIVE_DEMO
