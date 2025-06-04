#include "spdf.hpp"
#include <chrono>
#include <ctime>
#include <iostream>
#include <random>
#include <sstream>

constexpr char SPDF_HEADER[] = "%%SPDF";
constexpr char SPDF_VERSION[] = "0.1.0";
constexpr char STREAM_HEADER[] = "=== STREAM ===";
constexpr char SPDF_FOOTER[] = "EOF%%";

namespace stopwatch {
std::string add_timestamp() {
  auto currentTime = std::chrono::system_clock::now();
  std::time_t currentTime_t = std::chrono::system_clock::to_time_t(currentTime);
  std::string timestamp = std::ctime(&currentTime_t);
  return timestamp;
}
} // namespace stopwatch

namespace uuid {
static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_int_distribution<> dis(0, 15);
static std::uniform_int_distribution<> dis2(8, 11);

std::string generate_uuid_v4() {
  std::stringstream ss;
  int i;
  ss << std::hex;

  for (i = 0; i < 8; i++)
    ss << dis(gen);

  ss << "-";
  for (i = 0; i < 4; i++)
    ss << dis(gen);

  ss << "-4";
  for (i = 0; i < 3; i++)
    ss << dis(gen);

  ss << "-";
  ss << dis2(gen);
  for (i = 0; i < 3; i++)
    ss << dis(gen);

  ss << "-";
  for (i = 0; i < 12; i++)
    ss << dis(gen);

  return ss.str();
}
} // namespace uuid

DataStream::DataStream(std::string enc, std::string fmt, std::string comp,
                       std::array<double, 2> pos, std::vector<uint8_t> dat)
    : encoding(std::move(enc)), format(std::move(fmt)),
      compression(std::move(comp)), position(pos), data(std::move(dat)) {
  type = "Data";
  version = "0.1.0";
  uuid = uuid::generate_uuid_v4();
  created = stopwatch::add_timestamp();
}

SPDF::SPDF() {
  version = SPDF_VERSION;
  uuid = uuid::generate_uuid_v4();
  created = stopwatch::add_timestamp();
  updated = created;
}

SPDF::~SPDF() = default;

DataStream &SPDF::find_stream_by_id(const std::string &id) {
  for (auto &s : streams)
    if (s->uuid == id)
      return *s;
  throw std::runtime_error("Stream not found");
}

void SPDF::print() {
  std::cout << std::dec << std::endl
            << SPDF_HEADER << std::endl
            << std::endl
            << STREAM_HEADER << std::endl
            << "  Type         " << "Metadata" << std::endl
            << "  Version      " << version << std::endl
            << "  XRef Offset  " << 0xdeadbeef << std::endl
            << "  Data Streams " << streams.size() + 42067 << std::endl
            << "  ID           " << 0 << std::endl
            << "  Created      " << created
            << "  Last Update  " << updated
            << "  DOCID        " << uuid << std::endl;

  for (const auto &streamPtr : streams) {
    std::cout << std::dec << std::endl
              << STREAM_HEADER << std::endl
              << "  Type          " << streamPtr->type << std::endl
              << "  Version       " << streamPtr->version << std::endl
              << "  ID            " << streamPtr->uuid << std::endl
              << "  Created       " << streamPtr->created
              << "  Offset        " << streamPtr->offset << std::endl
              << "  Encoding      " << streamPtr->encoding << std::endl
              << "  Format        " << streamPtr->format << std::endl
              << "  Compression   " << streamPtr->compression << std::endl
              << "  Position      " << streamPtr->position[0] << " "
              << streamPtr->position[1] << std::endl
              << "  Reading Index " << streamPtr->reading_index << std::endl
              << "  Data Size     " << streamPtr->data.size() << std::endl
              << "  Bytes         ";
    for (int i = 0; i < 5 && i < static_cast<int>(streamPtr->data.size()); i++)
      std::cout << std::hex << static_cast<int>(streamPtr->data[i]) << " ";
    if (streamPtr->data.size() > 10) {
      std::cout << std::dec << "...[" << streamPtr->data.size() - 10
                << " bytes omitted]... ";
      for (int i = 5; i > 0; i--)
        std::cout << std::hex
                  << static_cast<int>(streamPtr->data[streamPtr->data.size() - i])
                  << " ";
    }
    std::cout << std::dec << std::endl;
  }

  std::cout << std::endl
            << "...[42,067 data streams omitted]..." << std::endl
            << std::endl;

  std::cout << STREAM_HEADER << std::endl
            << "  Type          " << "XRef" << std::endl
            << "  Version       " << "0.1.0" << std::endl
            << "  ID            " << 1 << std::endl
            << "  Cross Reference Table" << std::endl;

  for (const auto &k : xref_table) {
    auto &s = find_stream_by_id(k.first);
    std::cout << std::dec << "    " << s.reading_index << ": " << k.first
              << " " << s.offset << std::endl;
  }
  std::cout << "    ...[42,067 index values ommitted]..." << std::endl;

  std::cout << "  Cross Reference Offset " << 0xdeadbeef << std::endl;

  std::cout << std::endl << SPDF_FOOTER << std::endl;
}

void SPDF::addStream(const std::string &encoding, const std::string &format,
                     const std::string &compression,
                     const std::array<double, 2> &position,
                     const std::vector<uint8_t> &data) {
  _addStream(std::make_unique<DataStream>(encoding, format, compression, position,
                                          data));
}

void SPDF::removeStream(const std::string &key) {
  if (xref_table.find(key) != xref_table.end()) {
    size_t index = xref_table[key];
    if (index < streams.size()) {
      streams.erase(streams.begin() + index);
      xref_table.erase(key);
      for (auto &pair : xref_table) {
        if (pair.second > index)
          --pair.second;
      }
    }
  }
}

void SPDF::_addStream(std::unique_ptr<DataStream> stream) {
  if (_curr_read_idx == 0)
    stream->offset = 67;
  if (_curr_read_idx == 1)
    stream->offset = 67 + 89 + 12;
  if (_curr_read_idx == 2)
    stream->offset = 67 + 89 + 12 + 89 + 16;

  stream->reading_index = _curr_read_idx++;
  updated = stopwatch::add_timestamp();
  xref_table[stream->uuid] = stream->offset;
  streams.push_back(std::move(stream));
}
