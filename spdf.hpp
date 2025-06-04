#ifndef SPDF_HPP
#define SPDF_HPP

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace stopwatch {
std::string add_timestamp();
}

namespace uuid {
std::string generate_uuid_v4();
}

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
             std::array<double, 2> pos, std::vector<uint8_t> dat);
};

class SPDF {
public:
  std::string uuid;
  std::string version;
  std::string created;
  std::string updated;
  std::map<std::string, size_t> xref_table;
  std::vector<std::unique_ptr<DataStream>> streams;

  SPDF();
  ~SPDF();

  DataStream &find_stream_by_id(const std::string &id);
  void print();
  void addStream(const std::string &encoding, const std::string &format,
                 const std::string &compression,
                 const std::array<double, 2> &position,
                 const std::vector<uint8_t> &data);
  void removeStream(const std::string &key);

private:
  std::size_t _curr_read_idx = 0;
  void _addStream(std::unique_ptr<DataStream> stream);
};

#endif // SPDF_HPP
