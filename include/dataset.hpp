// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Shen-Ta Hsieh

#pragma once
#include <array>
#include <codecvt>
#include <execution>
#include <filesystem>
#include <fstream>
#include <locale>
#include <optional>
#include <vector>

namespace Dataset {

using std::literals::string_view_literals::operator""sv;
static inline constexpr const auto kPossible = "possible.txt"sv;
static inline constexpr const auto kValid = "valid.txt"sv;
static inline constexpr const auto kData = "data"sv;

template <std::size_t kSize>
static inline std::optional<std::vector<std::array<char32_t, kSize>>>
read(const std::filesystem::path &filename) noexcept {
  std::ifstream file(filename);
  if (!file) {
    return std::nullopt;
  }

  std::wstring_convert<std::codecvt_utf8_utf16<char32_t>, char32_t> converter;
  std::vector<std::array<char32_t, kSize>> list;
  std::string line;
  while (std::getline(file, line)) {
    const auto line_utf32 = converter.from_bytes(line);
    if (line_utf32.size() != kSize) {
      // ignore wrong words
      continue;
    }
    std::array<char32_t, kSize> word;
    std::copy(std::execution::par_unseq, line_utf32.begin(), line_utf32.end(),
              word.begin());
    list.push_back(word);
  }

  std::sort(std::execution::par_unseq, list.begin(), list.end());
  list.erase(std::unique(std::execution::par_unseq, list.begin(), list.end()),
             list.end());
  list.shrink_to_fit();
  return list;
}

} // namespace Dataset
