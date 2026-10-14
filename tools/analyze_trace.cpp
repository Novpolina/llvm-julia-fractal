#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using Key = std::array<uint16_t, 5>;
struct Hash {
  size_t operator()(const Key &key) const {
    size_t hash = 14695981039346656037ull;
    for (uint16_t token : key)
      hash = (hash ^ token) * 1099511628211ull;
    return hash;
  }
};

static void analyze(const std::string &directory, bool edges,
                    const std::unordered_map<unsigned, std::string> &names) {
  const std::string kind = edges ? "uses" : "instructions";
  gzFile input = gzopen((directory + "/" + kind + ".bin.gz").c_str(), "rb");
  if (!input)
    throw std::runtime_error("cannot open " + kind + " trace");
  std::array<std::unordered_map<Key, uint64_t, Hash>, 5> counts;
  Key history{};
  uint64_t total = 0;
  int pending = -1;
  std::array<unsigned char, 65536> buffer{};
  int length;
  while ((length = gzread(input, buffer.data(), buffer.size())) > 0) {
    for (int i = 0; i < length; ++i) {
      unsigned token = buffer[i];
      if (edges) {
        if (pending < 0) {
          pending = token;
          continue;
        }
        token |= static_cast<unsigned>(pending) << 8;
        pending = -1;
      }
      if (!names.count(edges ? token >> 8 : token) ||
          (edges && !names.count(token & 255)))
        throw std::runtime_error("unknown opcode in trace");
      if (names.at(edges ? token >> 8 : token) == "phi" ||
          (edges && names.at(token & 255) == "phi"))
        throw std::runtime_error("PHI was not skipped/expanded");
      for (unsigned j = 0; j < 4; ++j)
        history[j] = history[j + 1];
      history[4] = static_cast<uint16_t>(token);
      ++total;
      for (unsigned window = 1; window <= 5 && window <= total; ++window) {
        Key key{};
        std::copy(history.end() - window, history.end(), key.begin());
        ++counts[window - 1][key];
      }
    }
  }
  int error_code = Z_OK;
  std::string error = gzerror(input, &error_code);
  int close_status = gzclose(input);
  if (length < 0 || (error_code != Z_OK && error_code != Z_STREAM_END) ||
      close_status != Z_OK || pending >= 0)
    throw std::runtime_error("invalid/truncated trace: " + error);

  std::ofstream summary(directory + "/" + kind + "_summary.json");
  summary << "{\n  \"events\": " << total << ",\n  \"windows\": [\n";
  for (unsigned window = 1; window <= 5; ++window) {
    auto &map = counts[window - 1];
    std::vector<std::pair<Key, uint64_t>> sorted(map.begin(), map.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
      return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    const uint64_t windows = total >= window ? total - window + 1 : 0;
    std::ofstream output(directory + "/" + kind + "_patterns_" +
                         std::to_string(window) + ".tsv");
    output << "count\tpercent\tpattern\n";
    uint64_t sum = 0;
    for (const auto &entry : sorted) {
      sum += entry.second;
      output << entry.second << '\t' << std::fixed << std::setprecision(6)
             << (windows ? 100.0 * entry.second / windows : 0) << '\t';
      for (unsigned i = 0; i < window; ++i) {
        if (i)
          output << " ; ";
        unsigned token = entry.first[i];
        if (edges)
          output << names.at(token >> 8) << " <- " << names.at(token & 255);
        else
          output << names.at(token);
      }
      output << '\n';
    }
    if (sum != windows)
      throw std::runtime_error("window count invariant failed");
    if (!output)
      throw std::runtime_error("cannot write pattern table");
    summary << "    {\"length\": " << window << ", \"total\": " << windows
            << ", \"unique\": " << sorted.size() << "}"
            << (window == 5 ? "\n" : ",\n");
  }
  summary << "  ]\n}\n";
  if (!summary)
    throw std::runtime_error("cannot write summary");
  std::cout << directory << ' ' << kind << ": " << total << " runtime events\n";
}

int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("usage: analyze-trace TRACE_DIRECTORY");
    std::ifstream dictionary(std::string(argv[1]) + "/opcodes.json");
    std::string contents((std::istreambuf_iterator<char>(dictionary)), {});
    std::unordered_map<unsigned, std::string> names;
    std::regex entry("\"([0-9]+)\"[[:space:]]*:[[:space:]]*\"([a-z0-9_]+)\"");
    for (auto it = std::sregex_iterator(contents.begin(), contents.end(), entry);
         it != std::sregex_iterator(); ++it)
      names[std::stoul((*it)[1])] = (*it)[2];
    if (names.empty())
      throw std::runtime_error("empty opcode dictionary");
    analyze(argv[1], false, names);
    analyze(argv[1], true, names);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
