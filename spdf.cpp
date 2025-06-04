#include <array>
#include <chrono>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

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

// good enough for now
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

class DataStream {
public:
  std::string type;
  std::string uuid;
  std::string version;
  std::string created;
  std::size_t offset;
  std::string encoding;
  std::string format;
  std::string compression;
  std::size_t reading_index;
  std::array<double, 2> position;  
  std::vector<std::uint8_t> data;

  DataStream(std::string enc, std::string fmt, std::string comp,
             std::array<double, 2> pos, std::vector<uint8_t> dat)
      : encoding(std::move(enc)), format(std::move(fmt)),
        compression(std::move(comp)), position(pos), data(std::move(dat)) {

    type = "Data";
    version = "0.1.0";
    uuid = uuid::generate_uuid_v4();
    created = stopwatch::add_timestamp();
  }
};

class SPDF {
public:
  std::string uuid;
  std::string version;
  std::string created;
  std::string updated;  
  std::map<std::string, size_t> xref_table;
  std::vector<std::unique_ptr<DataStream>> streams;

  SPDF() {
    version = SPDF_VERSION;
    uuid = uuid::generate_uuid_v4();
    created = stopwatch::add_timestamp();
    updated = created;
  };

  ~SPDF() = default;

  DataStream &find_stream_by_id(const std::string &id) {
    for (auto &s : streams)
      if (s->uuid == id)
        return *s;

    throw std::runtime_error("Stream not found");
  }

  void print() {
    std::cout << std::dec << std::endl
              << SPDF_HEADER << std::endl
              << std::endl // 6
              << STREAM_HEADER << std::endl
              << "  Type         "
              << "Metadata" << std::endl                                  // 0
              << "  Version      " << version << std::endl                // 6
              << "  XRef Offset  " << cross_reference_offset() << std::endl // 8
              << "  Data Streams " << streams.size() << std::endl         // 4
              << "  ID           " << 0 << std::endl
              << "  Created      " << created            // 4
              << "  Last Update  " << updated            // 4
              << "  DOCID        " << uuid << std::endl; // 35
                                                         // total 67
    for (const auto &streamPtr : streams) {
      std::cout << std::dec << std::endl
                << STREAM_HEADER << std::endl
                << "  Type          " << streamPtr->type << std::endl     // 1
                << "  Version       " << streamPtr->version << std::endl  // 6
                << "  ID            " << streamPtr->uuid << std::endl     // 35
                << "  Created       " << streamPtr->created               // 4
                << "  Offset        " << streamPtr->offset << std::endl   // 8
                << "  Encoding      " << streamPtr->encoding << std::endl // 1
                << "  Format        " << streamPtr->format << std::endl   // 1
                << "  Compression   " << streamPtr->compression
                << std::endl                                           // 1
                << "  Position      " << streamPtr->position[0] << " " // 8
                << streamPtr->position[1] << std::endl                 // 8
                << "  Reading Index " << streamPtr->reading_index
                << std::endl // 8
                << "  Data Size     " << streamPtr->data.size()
                << std::endl // 8                
                << "  Bytes         ";
      // total 89
      for (int i = 0; i < 5; i++) {
        std::cout << std::hex << static_cast<int>(streamPtr->data[i]) << " ";
      }
      std::cout << std::dec << "...[" << streamPtr->data.size() - 10
                << " bytes omitted]... ";

      for (int i = 5; i > 0; i--) {
        std::cout << std::hex
                  << static_cast<int>(
                         streamPtr->data[streamPtr->data.size() - i])
                  << " ";
      }

      std::cout << std::endl;
    }

    std::cout << std::endl
              << "...[" << streams.size() << " data streams total]..." << std::endl
              << std::endl;

    std::cout << STREAM_HEADER << std::endl
              << "  Type          "
              << "XRef" << std::endl // 1
              << "  Version       "
              << "0.1.0" << std::endl // 6
              << "  ID            " << 1 << std::endl
              << "  Cross Reference Table" << std::endl;

    for (const auto &k : xref_table) {
      auto s = find_stream_by_id(k.first);
      std::cout << std::dec << "    " << s.reading_index << ": " << k.first
                << " " << s.offset << std::endl;
    }
    std::cout << "    ...[" << xref_table.size() << " index values total]..." << std::endl;

    std::cout << "  Cross Reference Offset " << cross_reference_offset() << std::endl; // 8

    std::cout << std::endl << SPDF_FOOTER << std::endl; // 5
  }

  void addStream(const std::string &encoding, const std::string &format,
                 const std::string &compression,
                 const std::array<double, 2> &position,
                 const std::vector<uint8_t> &data) {

    _addStream(std::make_unique<DataStream>(encoding, format, compression,
                                            position, data));
  }

  void removeStream(const std::string &key) {
    if (xref_table.find(key) != xref_table.end()) {
      size_t index = xref_table[key];
      if (index < streams.size()) {
        streams.erase(streams.begin() + index);
        xref_table.erase(key);

        for (auto &pair : xref_table) {
          if (pair.second > index) {
            --pair.second;
          }
        }
      }
    }
  }

private:
  static constexpr std::size_t HEADER_SIZE = 67;
  static constexpr std::size_t STREAM_META_SIZE = 89;
  std::size_t _curr_read_idx = 0;

  std::size_t cross_reference_offset() const {
    if (streams.empty())
      return HEADER_SIZE;
    const auto &last = streams.back();
    return last->offset + STREAM_META_SIZE + last->data.size();
  }

  void _addStream(std::unique_ptr<DataStream> stream) {
    if (_curr_read_idx == 0) {
      stream->offset = HEADER_SIZE;
    } else {
      const auto &prev = streams.back();
      stream->offset = prev->offset + STREAM_META_SIZE + prev->data.size();
    }

    stream->reading_index = _curr_read_idx++;
    updated = stopwatch::add_timestamp();
    xref_table[stream->uuid] = stream->offset;
    streams.push_back(std::move(stream));
  }
};

int main() {
  SPDF document;

  document.addStream(
      "UTF-8", "text/plain", "None", {0.0, 0.0},
      {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'});

  document.addStream("Base64", "application/octet-stream", "None", {0.0, 0.0},
                     {'S', 'G', 'V', 's', 'b', 'G', '8', 'g', 'V', '2', '9',
                      'y', 'b', 'G', 'Q', 'h'});

  document.print();

  return 0;
}
