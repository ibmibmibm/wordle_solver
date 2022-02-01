// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Shen-Ta Hsieh

#include "context.hpp"
#include "dataset.hpp"
#include <SDL.h>
#include <SDL_opengles2.h>
#include <cinttypes>
#include <cstdio>
#include <future>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <tbb/task_arena.h>
#include <variant>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui_internal.h>

namespace ImGui {

bool ResultBox(const char *label, uint8_t *value, const char *text) noexcept {
  ImGuiWindow *window = GetCurrentWindow();
  if (window->SkipItems)
    return false;

  ImGuiContext &g = *GImGui;
  const ImGuiStyle &style = g.Style;
  const ImGuiID id = window->GetID(label);
  const ImVec2 label_size = CalcTextSize(label, NULL, true);
  const ImVec2 text_size = CalcTextSize(text, NULL, true);

  const float square_sz = GetFrameHeight();
  const ImVec2 pos = window->DC.CursorPos;
  const ImRect total_bb(
      pos,
      pos + ImVec2(square_sz + (label_size.x > 0.0f
                                    ? style.ItemInnerSpacing.x + label_size.x
                                    : 0.0f),
                   label_size.y + style.FramePadding.y * 2.0f));
  ItemSize(total_bb, style.FramePadding.y);
  if (!ItemAdd(total_bb, id)) {
    IMGUI_TEST_ENGINE_ITEM_INFO(id, label,
                                g.LastItemData.StatusFlags |
                                    ImGuiItemStatusFlags_Checkable |
                                    (*v ? ImGuiItemStatusFlags_Checked : 0));
    return false;
  }

  bool hovered, held;
  bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
  if (pressed) {
    switch (*value) {
    case 0:
      *value = 1;
      break;
    case 1:
      *value = 2;
      break;
    default:
      *value = 0;
    }
    MarkItemEdited(id);
  }

  const ImRect check_bb(pos, pos + ImVec2(square_sz, square_sz));
  RenderNavHighlight(total_bb, id);

  ImU32 box_col = *value == 0   ? ImColor(0x1.49494ap-1f, 0x1.5d5d5ep-1f,
                                          0x1.89898ap-1f, g.Style.Alpha)
                  : *value == 1 ? ImColor(0x1.e7e7e8p-1f, 0x1.858586p-1f,
                                          0x1.b9b9bap-3f, g.Style.Alpha)
                                : ImColor(0x1.e5e5e6p-2f, 0x1.717172p-1f,
                                          0x1.454546p-2f, g.Style.Alpha);
  RenderFrame(check_bb.Min, check_bb.Max, box_col, true, style.FrameRounding);

  /*
  ImVec2 text_pos = ImVec2((check_bb.Max.x - check_bb.Min.x - text_size.x)
  / 2.0f, (check_bb.Max.y - check_bb.Min.y - text_size.y) / 2.0f);
  RenderText(text_pos, text);
  */
  RenderTextClipped(check_bb.Min + style.FramePadding,
                    check_bb.Max - style.FramePadding, text, nullptr,
                    &text_size, style.ButtonTextAlign, &check_bb);

  ImVec2 label_pos = ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x,
                            check_bb.Min.y + style.FramePadding.y);
  if (g.LogEnabled) {
    LogRenderedText(&label_pos, *value == 0   ? "[0]"
                                : *value == 1 ? "[1]"
                                              : "[2]");
  }
  if (label_size.x > 0.0f) {
    RenderText(label_pos, label);
  }

  IMGUI_TEST_ENGINE_ITEM_INFO(id, label,
                              g.LastItemData.StatusFlags |
                                  ImGuiItemStatusFlags_Checkable |
                                  (*v ? ImGuiItemStatusFlags_Checked : 0));
  return pressed;
}

} // namespace ImGui

namespace {

SDL_Window *g_Window = nullptr;
SDL_GLContext g_GLContext = nullptr;
#ifndef __EMSCRIPTEN__
bool g_Done = false;
#endif

static std::variant<Context<4>, Context<5>, Context<6>, Context<7>, Context<8>,
                    Context<9>, Context<10>, Context<11>>
    g_Context(std::in_place_type<Context<5>>);

template <size_t kSize>
std::string load(const std::filesystem::path &directory,
                 bool hard_mode) noexcept {
  auto &context = g_Context.emplace<Context<kSize>>();
  if (auto set = Dataset::read<kSize>(Dataset::kData / directory /
                                      Dataset::kPossible)) {
    context.possible_answer = std::move(*set);
  } else {
    return "load possible failed";
  }

  if (auto set =
          Dataset::read<kSize>(Dataset::kData / directory / Dataset::kValid)) {
    context.valid_input = std::move(*set);
  } else {
    return "load valid failed";
  }

  context.reorder_all_word_set();
  return {};
}

inline std::u32string from_utf8(std::string_view string) noexcept {
  std::wstring_convert<std::codecvt_utf8_utf16<char32_t>, char32_t> converter;
  return converter.from_bytes(string.begin(), string.end());
}

template <std::size_t kSize>
inline std::string to_utf8(std::span<const char32_t, kSize> word) noexcept {
  std::wstring_convert<std::codecvt_utf8_utf16<char32_t>, char32_t> converter;
  return converter.to_bytes(word.data(), word.data() + word.size());
}

inline std::string to_utf8(std::u32string_view word) noexcept {
  std::wstring_convert<std::codecvt_utf8_utf16<char32_t>, char32_t> converter;
  return converter.to_bytes(word.data(), word.data() + word.size());
}

void main_loop(void) noexcept {
  ImGuiIO &io = ImGui::GetIO();

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL2_ProcessEvent(&event);
#ifndef __EMSCRIPTEN__
    if (event.type == SDL_QUIT) {
      g_Done = true;
    }
    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_CLOSE &&
        event.window.windowID == SDL_GetWindowID(g_Window)) {
      g_Done = true;
    }
#endif
  }

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  {
#ifdef IMGUI_HAS_VIEWPORT
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetWorkPos());
    ImGui::SetNextWindowSize(viewport->GetWorkSize());
    ImGui::SetNextWindowViewport(viewport->ID);
#else
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
#endif
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("Wordle Solver", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

    static bool hard_mode = false;
    static std::array<char, 11 * 4 + 1> candidate_utf8{};
    static std::array<char32_t, 11 + 1> candidate_utf32{};
    static std::array<uint8_t, 11> result_value{};
    static ImGuiTextBuffer message;

    static std::future<std::string> load_result;
    static std::future<std::tuple<double, std::u32string>>
        best_candidate_result;
    static std::future<std::tuple<uint64_t, uint64_t>> input_candidate_result;

    ImGui::BeginDisabled(load_result.valid() || best_candidate_result.valid() ||
                         input_candidate_result.valid());
    if (ImGui::BeginTable("Dataset", 2, ImGuiTableFlags_BordersInnerV)) {
      static int dataset = 4;
      static const std::array<const char *, 7> dataset_name{
          "bopomofo", "japanese",   "nerdlegame", "nerdlegame_mini",
          "wordle",   "wordlegame", "zidou",
      };
      static int word_size_minus_4 = 5 - 4;
      static const std::array<const char *, 8> word_size_name{
          "4", "5", "6", "7", "8", "9", "10", "11",
      };

      ImGui::TableNextColumn();
      ImGui::TextUnformatted("Dataset");
      ImGui::TableNextColumn();
      if (ImGui::BeginCombo("##Dataset", dataset_name[dataset])) {
        for (size_t i = 0; i < dataset_name.size(); ++i) {
          ImGui::PushID(i);
          bool selected = (dataset == i);
          if (ImGui::Selectable(dataset_name[i], selected)) {
            dataset = i;
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
          ImGui::PopID();
        }
        ImGui::EndCombo();
      }

      ImGui::TableNextColumn();
      ImGui::TextUnformatted("Word size");
      ImGui::TableNextColumn();
      if (ImGui::BeginCombo("##Word size", word_size_name[word_size_minus_4])) {
        for (size_t i = 0; i < word_size_name.size(); ++i) {
          ImGui::PushID(i);
          bool selected = (word_size_minus_4 == i);
          if (ImGui::Selectable(word_size_name[i], selected)) {
            word_size_minus_4 = i;
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
          ImGui::PopID();
        }
        ImGui::EndCombo();
      }

      ImGui::TableNextColumn();
      ImGui::TextUnformatted("Hard mode");
      ImGui::TableNextColumn();
      ImGui::Checkbox("##Hard mode", &hard_mode);

      ImGui::TableNextColumn();
      if (ImGui::Button("Load dataset")) {
        auto promise = std::make_shared<std::promise<std::string>>();
        load_result = promise->get_future();
        tbb::this_task_arena::enqueue(
            [promise = std::move(promise), word_size = word_size_minus_4 + 4,
             dataset = dataset_name[dataset]]() noexcept {
              std::string result;
              switch (word_size) {
              case 4:
                result = load<4>(dataset, hard_mode);
                break;
              case 5:
                result = load<5>(dataset, hard_mode);
                break;
              case 6:
                result = load<6>(dataset, hard_mode);
                break;
              case 7:
                result = load<7>(dataset, hard_mode);
                break;
              case 8:
                result = load<8>(dataset, hard_mode);
                break;
              case 9:
                result = load<9>(dataset, hard_mode);
                break;
              case 10:
                result = load<10>(dataset, hard_mode);
                break;
              case 11:
                result = load<11>(dataset, hard_mode);
                break;
              }
              promise->set_value(std::move(result));
            });
      }

      ImGui::EndTable();
    }

    ImGui::Separator();

    std::visit(
        [](auto &context) noexcept {
          ImGui::Text("Possible answers count: %zu",
                      load_result.valid() ? 0 : context.possible_answer.size());
          if (ImGui::Button("Find best candidate")) {
            auto promise = std::make_shared<
                std::promise<std::tuple<double, std::u32string>>>();
            best_candidate_result = promise->get_future();
            tbb::this_task_arena::enqueue([promise = std::move(promise),
                                           &context]() noexcept {
              const auto [excluded, candidate] = context.find_best_candidate();
              promise->set_value({excluded, std::u32string(candidate.begin(),
                                                           candidate.end())});
            });
          }
        },
        g_Context);

    ImGui::Separator();

    if (ImGui::BeginTable("Input", 2, ImGuiTableFlags_BordersInnerV)) {
      ImGui::TableNextColumn();
      ImGui::TextUnformatted("Input");
      ImGui::TableNextColumn();
      if (ImGui::InputText("##Input", candidate_utf8.data(),
                           candidate_utf8.size(),
                           ImGuiInputTextFlags_CharsNoBlank)) {
        const auto candidate = from_utf8(candidate_utf8.data());
        const auto last = std::copy(candidate.begin(), candidate.end(),
                                    candidate_utf32.begin());
        *last = 0;
      }

      if (best_candidate_result.valid()) {
        if (best_candidate_result.wait_for(std::chrono::seconds(0)) ==
            std::future_status::ready) {
          const auto [best_excluded, best_candidate_utf32] =
              best_candidate_result.get();
          message.clear();
          std::string best_candidate_utf8 = to_utf8(best_candidate_utf32);
          message.appendf("Best candidate: `%.*s` excluded %lf candidates",
                          static_cast<int>(best_candidate_utf8.size()),
                          best_candidate_utf8.data(), best_excluded);
          std::copy(std::execution::unseq, best_candidate_utf32.begin(),
                    best_candidate_utf32.end() + 1, candidate_utf32.begin());
          std::copy(std::execution::unseq, best_candidate_utf8.begin(),
                    best_candidate_utf8.end() + 1, candidate_utf8.begin());
        }
      }

      ImGui::TableNextColumn();
      ImGui::TextUnformatted("Result");
      ImGui::TableNextColumn();
      {
        const auto kSize =
            load_result.valid()
                ? 4
                : std::visit(
                      [](auto &context) noexcept {
                        return std::decay_t<decltype(context)>::size;
                      },
                      g_Context);
        const auto candidate_view =
            std::u32string_view(candidate_utf32.data(), kSize);
        ImGui::PushID(0);
        ImGui::ResultBox("##Character", &result_value[0],
                         to_utf8(candidate_view.substr(0, 1)).c_str());
        ImGui::PopID();
        for (size_t i = 1; i < kSize; ++i) {
          ImGui::SameLine();
          ImGui::PushID(i);
          ImGui::ResultBox("##Character", &result_value[i],
                           to_utf8(candidate_view.substr(i, 1)).c_str());
          ImGui::PopID();
        }
      }

      ImGui::TableNextColumn();
      if (ImGui::Button("Enter input and result")) {
        std::visit(
            [](auto &context) noexcept {
              constexpr const auto kSize =
                  std::decay_t<decltype(context)>::size;
              do {
                const auto last = std::find(candidate_utf32.begin(),
                                            candidate_utf32.end(), 0);
                if (std::distance(candidate_utf32.begin(), last) != kSize) {
                  message.clear();
                  message.append("Wrong candidate length!");
                  break;
                }
                std::array<char32_t, kSize> candidate;
                std::copy_n(std::execution::unseq, candidate_utf32.begin(),
                            kSize, candidate.begin());
                candidate_utf8[0] = '\0';
                std::fill_n(candidate_utf32.begin(), kSize, 0);
                std::array<char8_t, kSize> parsed_result{};
                std::copy_n(std::execution::unseq, result_value.begin(), kSize,
                            parsed_result.begin());
                auto promise = std::make_shared<
                    std::promise<std::tuple<uint64_t, uint64_t>>>();
                input_candidate_result = promise->get_future();
                tbb::this_task_arena::enqueue([promise = std::move(promise),
                                               candidate, parsed_result,
                                               &context]() noexcept {
                  promise->set_value(context.input_candidate(
                      candidate, parsed_result, hard_mode));
                });
              } while (false);
            },
            g_Context);
      }
      ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("First 16 Candidates");
    if (ImGui::BeginListBox("##Possible", ImVec2(-FLT_MIN, 0))) {
      if (!load_result.valid() && !input_candidate_result.valid()) {
        std::visit(
            [](auto &context) noexcept {
              constexpr const auto kSize =
                  std::decay_t<decltype(context)>::size;
              for (size_t i = 0;
                   i < std::min<size_t>(context.possible_answer.size(), 16);
                   ++i) {
                const auto word = to_utf8<kSize>(context.possible_answer[i]);
                ImGui::Selectable(word.c_str(), false);
              }
            },
            g_Context);
      }
      ImGui::EndListBox();
    }

    ImGui::EndDisabled();
    ImGui::TextUnformatted(message.begin(), message.end());
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                1000.0f / io.Framerate, io.Framerate);
    ImGui::End();
    ImGui::PopStyleVar(1);

    if (load_result.valid()) {
      if (load_result.wait_for(std::chrono::seconds(0)) ==
          std::future_status::ready) {
        const auto m = load_result.get();
        message.clear();
        message.append(m.data(), m.data() + m.size());
      }
    }
    if (input_candidate_result.valid()) {
      if (input_candidate_result.wait_for(std::chrono::seconds(0)) ==
          std::future_status::ready) {
        const auto [possible_answer_removed, possible_answer_remained] =
            input_candidate_result.get();
        message.clear();
        message.appendf("removed %" PRIu64 " candidates.",
                        possible_answer_removed);

        const auto kSize = std::visit(
            [](auto &context) noexcept {
              return std::decay_t<decltype(context)>::size;
            },
            g_Context);
        std::fill_n(std::execution::unseq, result_value.begin(), kSize, 0);
      }
    }
  }

  // Rendering
  ImGui::Render();
  SDL_GL_MakeCurrent(g_Window, g_GLContext);
  /*
  glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
  glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
               clear_color.z * clear_color.w, clear_color.w);
  glClear(GL_COLOR_BUFFER_BIT);
  */
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(g_Window);
}
} // namespace

int main(int, char **) {
  // Setup SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) !=
      0) {
    std::fprintf(stderr, "SDL Error: %s\n", SDL_GetError());
    return -1;
  }

  const char *glsl_version = "#version 100";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_DisplayMode current;
  SDL_GetCurrentDisplayMode(0, &current);
  const auto window_flags = static_cast<SDL_WindowFlags>(
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  g_Window = SDL_CreateWindow("Wordle Solver", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
  g_GLContext = SDL_GL_CreateContext(g_Window);
#ifdef __EMSCRIPTEN__
  if (!g_GLContext) {
    std::fprintf(stderr, "Failed to initialize WebGL context!\n");
    return -1;
  }
#endif
  SDL_GL_SetSwapInterval(1); // Enable vsync

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

#ifdef __EMSCRIPTEN__
  // For an Emscripten build we are disabling file-system access, so let's not
  // attempt to do a fopen() of the imgui.ini file. You may manually call
  // LoadIniSettingsFromMemory() to load settings from your own storage.
  io.IniFilename = nullptr;
#endif

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsClassic();

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForOpenGL(g_Window, g_GLContext);
  ImGui_ImplOpenGL3_Init(glsl_version);

  {
    io.Fonts->AddFontFromFileTTF("fonts/NotoSans-Regular.ttf", 20.0f, nullptr,
                                 io.Fonts->GetGlyphRangesDefault());
    ImFontConfig config;
    config.MergeMode = true;
    ImVector<ImWchar> jp_ranges;
    ImVector<ImWchar> tc_ranges;
    {
      ImFontGlyphRangesBuilder builder;
      builder.AddText(
          "\u301c\u3042\u3044\u3046\u3048\u304a\u304b\u304c\u304d\u304e\u304f"
          "\u3050\u3051\u3052\u3053\u3054\u3055\u3056\u3057\u3058\u3059\u305a"
          "\u305b\u305c\u305d\u305e\u305f\u3060\u3061\u3062\u3064\u3065\u3066"
          "\u3067\u3068\u3069\u306a\u306b\u306c\u306d\u306e\u306f\u3070\u3071"
          "\u3072\u3073\u3074\u3075\u3076\u3077\u3078\u3079\u307a\u307b\u307c"
          "\u307d\u307e\u307f\u3080\u3081\u3082\u3084\u3086\u3088\u3089\u308a"
          "\u308b\u308c\u308d\u308f\u3092\u3093\u3094\u30fc");
      builder.BuildRanges(&jp_ranges);
    }
    {
      ImFontGlyphRangesBuilder builder;
      builder.AddText(
          "\u3105\u3106\u3107\u3108\u3109\u310a\u310b\u310c\u310d\u310e\u310f"
          "\u3110\u3111\u3112\u3113\u3114\u3115\u3116\u3117\u3118\u3119\u311a"
          "\u311b\u311c\u311d\u311e\u311f\u3120\u3121\u3122\u3123\u3124\u3125"
          "\u3126\u3127\u3128\u3129");
      builder.BuildRanges(&tc_ranges);
    }
    io.Fonts->AddFontFromFileTTF("fonts/NotoSansJP-Regular.otf", 20.0f, &config,
                                 jp_ranges.Data);
    io.Fonts->AddFontFromFileTTF("fonts/NotoSansTC-Regular.otf", 20.0f, &config,
                                 tc_ranges.Data);
    io.Fonts->Build();
  }

#ifdef __EMSCRIPTEN__
  // This function call won't return, and will engage in an infinite loop,
  // processing events from the browser, and dispatching them.
  emscripten_set_main_loop(&main_loop, 0, true);
#else
  while (!g_Done) {
    main_loop();
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  SDL_GL_DeleteContext(g_GLContext);
  SDL_DestroyWindow(g_Window);
  SDL_Quit();
#endif

  return 0;
}
