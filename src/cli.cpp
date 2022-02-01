// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Shen-Ta Hsieh

#include "context.hpp"
#include "dataset.hpp"
#include <cstdlib>
#include <iostream>
#include <spdlog/spdlog.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

using std::literals::string_view_literals::operator""sv;

namespace {

std::string read_input(std::string_view message) noexcept {
  spdlog::info("{}"sv, message);
  std::string buffer;
  std::cin >> buffer;
  return buffer;
}

template <std::size_t kSize>
inline std::string to_utf8(std::span<const char32_t, kSize> word) noexcept {
  std::wstring_convert<std::codecvt_utf8_utf16<char32_t>, char32_t> converter;
  return converter.to_bytes(word.data(), word.data() + word.size());
}

inline constexpr bool invalid_character(char char_) noexcept {
  switch (char_) {
  case '0':
  case '1':
  case '2':
    return false;
  default:
    return true;
  }
}

template <std::size_t kSize> std::array<char8_t, kSize> ask_result() noexcept {
  std::array<char8_t, kSize> parsed_result{};
  do {
    std::string result =
        read_input("please enter result(0:grey, 1:yello, 2:green):"sv);
    if (result.size() != kSize) {
      spdlog::error("result size not match:{} != {}"sv, result.size(), kSize);
      continue;
    }

    if (auto pos = std::find_if(std::execution::par_unseq, result.begin(),
                                result.end(), invalid_character);
        pos != result.end()) {
      spdlog::error("result contained invalid character `{}` at position {}"sv,
                    *pos, std::distance(result.begin(), pos));
      continue;
    }
    for (size_t index = 0; index < kSize; ++index) {
      switch (result[index]) {
      case '0':
        parsed_result[index] = 0;
        break;
      case '1':
        parsed_result[index] = 1;
        break;
      case '2':
        parsed_result[index] = 2;
        break;
      }
    }
    break;
  } while (true);
  return parsed_result;
}

template <std::size_t kSize>
bool run(const std::filesystem::path &directory, bool hard_mode) noexcept {
  Context<kSize> context;
  if (auto set = Dataset::read<kSize>(Dataset::kData / directory /
                                      Dataset::kPossible)) {
    context.possible_answer = std::move(*set);
    spdlog::info("read problem words set, {} words"sv,
                 context.possible_answer.size());
  } else {
    return false;
  }

  if (auto set =
          Dataset::read<kSize>(Dataset::kData / directory / Dataset::kValid)) {
    context.valid_input = std::move(*set);
    spdlog::info("read all words set, {} words"sv, context.valid_input.size());
  } else {
    return false;
  }

  context.reorder_all_word_set();

  while (!context.is_finished()) {
    const auto [excluded, candidate] = context.find_best_candidate();
    spdlog::info("`{}` exclude {} words"sv, to_utf8(candidate), excluded);

    const auto result = ask_result<kSize>();
    const auto [possible_answer_removed, possible_answer_remained] =
        context.input_candidate(candidate, result, hard_mode);
    spdlog::info("removed {}, remained {} candidates"sv,
                 possible_answer_removed, possible_answer_remained);

    if (possible_answer_remained <= 16) {
      std::string wordlist;
      for (const auto &word : context.possible_answer) {
        wordlist += ' ';
        wordlist += to_utf8<kSize>(word);
      }
      spdlog::info("candidates:{}"sv, wordlist);
    }
  }

  if (context.possible_answer.size() == 1) {
    spdlog::info("final answer: `{}`"sv,
                 to_utf8<kSize>(context.possible_answer.front()));
  } else {
    spdlog::info("no answer founded!"sv);
  }

  return true;
}

bool run(std::size_t word_size, const std::filesystem::path &directory,
         bool hard_mode) noexcept {
  switch (word_size) {
  default:
    spdlog::error("unsupported size `{}`! need 4~11"sv, word_size);
    return false;
  case 4:
    return run<4>(directory, hard_mode);
  case 5:
    return run<5>(directory, hard_mode);
  case 6:
    return run<6>(directory, hard_mode);
  case 7:
    return run<7>(directory, hard_mode);
  case 8:
    return run<8>(directory, hard_mode);
  case 9:
    return run<9>(directory, hard_mode);
  case 10:
    return run<10>(directory, hard_mode);
  case 11:
    return run<11>(directory, hard_mode);
  }
}

} // namespace

int main() noexcept {
  size_t word_size;
  do {
    try {
      word_size = std::stoul(read_input("please enter word size(4~11)"sv));
      if (4 <= word_size && word_size <= 11) {
        break;
      }
    } catch (std::invalid_argument &msg) {
      spdlog::error("invalid argument:{}"sv, msg.what());
    } catch (std::out_of_range &msg) {
      spdlog::error("out of range:{}"sv, msg.what());
    }
  } while (true);

  std::string directory;
  do {
    try {
      switch (std::stoul(read_input("please enter dataset id:\n"
                                    "1.bopomofo\n"
                                    "2.japanese\n"
                                    "3.nerdlegame\n"
                                    "4.nerdlegame_mini\n"
                                    "5.wordle\n"
                                    "6.wordlegame\n"
                                    "7.zidou"sv))) {
      case 1:
        directory = "bopomofo"sv;
        break;
      case 2:
        directory = "japanese"sv;
        break;
      case 3:
        directory = "nerdlegame"sv;
        break;
      case 4:
        directory = "nerdlegame_mini"sv;
        break;
      case 5:
        directory = "wordle"sv;
        break;
      case 6:
        directory = "wordlegame"sv;
        break;
      case 7:
        directory = "zidou"sv;
        break;
      }
      break;
    } catch (std::invalid_argument &msg) {
      spdlog::error("invalid argument:{}"sv, msg.what());
    } catch (std::out_of_range &msg) {
      spdlog::error("out of range:{}"sv, msg.what());
    }
  } while (true);

  bool hard_mode;
  do {
    try {
      switch (std::stoul(read_input("please enter hard mode:\n"
                                    "1.false\n"
                                    "2.true"sv))) {
      case 1:
        hard_mode = false;
        break;
      case 2:
        hard_mode = true;
        break;
      }
      break;
    } catch (std::invalid_argument &msg) {
      spdlog::error("invalid argument:{}"sv, msg.what());
    } catch (std::out_of_range &msg) {
      spdlog::error("out of range:{}"sv, msg.what());
    }
  } while (true);
  if (run(word_size, directory, hard_mode)) {
    return EXIT_SUCCESS;
  } else {
    return EXIT_FAILURE;
  }
}
