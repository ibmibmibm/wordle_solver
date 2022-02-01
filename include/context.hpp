// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Shen-Ta Hsieh

#pragma once

#include <algorithm>
#include <cstdint>
#include <execution>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

template <std::size_t kSize> class Context {
public:
  bool is_finished() const noexcept { return possible_answer.size() <= 1; }

  void reorder_all_word_set() noexcept {
    std::vector<std::array<char32_t, kSize>> union_(possible_answer.size() +
                                                    valid_input.size());
    const auto last = std::set_union(
        std::execution::par_unseq, valid_input.begin(), valid_input.end(),
        possible_answer.begin(), possible_answer.end(), union_.begin());
    union_.erase(last, union_.end());
    union_.shrink_to_fit();
    std::swap(valid_input, union_);
  }

  inline std::tuple<double, std::span<const char32_t, kSize>>
  find_best_candidate() const noexcept;

  inline std::tuple<uint64_t, uint64_t>
  input_candidate(std::span<const char32_t, kSize> candidate,
                  std::span<const char8_t, kSize> result,
                  bool hard_mode) noexcept;

  static inline constexpr std::array<char8_t, kSize>
  calculate(std::span<const char32_t, kSize> word,
            std::span<const char32_t, kSize> problem) noexcept;

  static inline constexpr const auto size = kSize;
  std::vector<std::array<char32_t, kSize>> possible_answer;
  std::vector<std::array<char32_t, kSize>> valid_input;
};

template <std::size_t kSize>
std::tuple<double, std::span<const char32_t, kSize>>
Context<kSize>::find_best_candidate() const noexcept {
  if (possible_answer.size() == 1) {
    return {1, possible_answer.front()};
  }
  auto find_excluded = [this](const std::array<char32_t, kSize> &word) noexcept
      -> std::tuple<uint64_t, std::span<const char32_t, kSize>> {
    auto power = [](uint64_t base, uint64_t factor) constexpr noexcept {
      uint64_t result = 1;
      for (uint64_t index = 0; index < factor; ++index) {
        result *= base;
      }
      return result;
    };
    auto serialize =
        [](const std::array<char8_t, kSize> &result) constexpr noexcept {
      return std::accumulate(
          result.begin(), result.end(), UINT64_C(0),
          [](uint64_t a, uint64_t b) constexpr noexcept {
            return a * UINT64_C(3) + b;
          });
    };

    std::array<uint64_t, power(3, kSize)> buckets = {};
    for (const auto &answer : possible_answer) {
      auto result = calculate(word, answer);
      ++buckets[serialize(result)];
    }
    uint64_t excluded = 0;
    for (const auto &count : buckets) {
      excluded += count * (possible_answer.size() - count);
    }
    // exact bias
    if (buckets.back()) {
      ++excluded;
    }
    return {excluded, word};
  };

  std::array<const char32_t, kSize> empty{};
  std::vector<std::tuple<uint64_t, std::span<const char32_t, kSize>>>
      all_excluded(valid_input.size(), {0, empty});
  std::transform(std::execution::par_unseq, valid_input.begin(),
                 valid_input.end(), all_excluded.begin(), find_excluded);
  const auto [excluded, candidate] = *std::max_element(
      std::execution::par, all_excluded.begin(), all_excluded.end(),
      [](const auto &a, const auto &b) constexpr noexcept {
        return std::get<0>(a) < std::get<0>(b);
      });
  return {static_cast<double>(excluded) /
              static_cast<double>(possible_answer.size()),
          candidate};
}

template <std::size_t kSize>
std::tuple<uint64_t, uint64_t>
Context<kSize>::input_candidate(std::span<const char32_t, kSize> candidate,
                                std::span<const char8_t, kSize> result,
                                bool hard_mode) noexcept {
  auto check_remove =
      [&candidate, &result ](const auto &word) constexpr noexcept {
    const auto compared = calculate(candidate, word);
    return !std::equal(std::execution::unseq, compared.begin(), compared.end(),
                       result.begin(), result.end());
  };

  uint64_t possible_answer_removed;
  uint64_t possible_answer_remained;
  {
    const auto erase_begin =
        std::remove_if(std::execution::par_unseq, possible_answer.begin(),
                       possible_answer.end(), check_remove);
    possible_answer_removed = std::distance(erase_begin, possible_answer.end());
    possible_answer.erase(erase_begin, possible_answer.end());
    possible_answer.shrink_to_fit();
    possible_answer_remained = possible_answer.size();
  }

  if (hard_mode) {
    const auto erase_begin =
        std::remove_if(std::execution::par_unseq, valid_input.begin(),
                       valid_input.end(), check_remove);
    valid_input.erase(erase_begin, valid_input.end());
    valid_input.shrink_to_fit();
  }

  return {possible_answer_removed, possible_answer_remained};
}

template <std::size_t kSize>
constexpr std::array<char8_t, kSize>
Context<kSize>::calculate(std::span<const char32_t, kSize> word,
                          std::span<const char32_t, kSize> problem) noexcept {
  std::array<std::tuple<char32_t, uint8_t>, kSize> alphabits;
  size_t alphabit_size = 0;

  auto increase =
      [&alphabits, &alphabit_size ](char32_t char_) constexpr noexcept {
    for (size_t index = 0; index < alphabit_size; ++index) {
      auto &[key, count] = alphabits[index];
      if (key == char_) {
        ++count;
        return;
      }
    }
    alphabits[alphabit_size++] = {char_, 1};
  };
  auto decrease =
      [&alphabits, &alphabit_size ](char32_t char_) constexpr noexcept {
    for (size_t index = 0; index < alphabit_size; ++index) {
      auto &[key, count] = alphabits[index];
      if (key == char_) {
        if (count > 0) {
          --count;
          return true;
        }
        return false;
      }
    }
    return false;
  };

  for (size_t index = 0; index < word.size(); ++index) {
    if (word[index] != problem[index]) {
      increase(problem[index]);
    }
  }

  std::array<char8_t, kSize> result;
  for (size_t index = 0; index < word.size(); ++index) {
    if (word[index] == problem[index]) {
      result[index] = 2;
      continue;
    }
    if (decrease(word[index])) {
      result[index] = 1;
      continue;
    }
    result[index] = 0;
  }

  return result;
}
