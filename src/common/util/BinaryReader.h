#pragma once

/*!
 * @file BinaryReader.h
 * Read raw data like a stream.
 */

#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include "common/common_types.h"
#include "common/util/Assert.h"

class UnboundedBinaryReader {
 public:
  UnboundedBinaryReader(u8 *data_start) : m_start(data_start){}

  template <typename T>
  T read() {
    T obj;
    memcpy(&obj, m_start + m_seek, sizeof(T));
    m_seek += sizeof(T);
    return obj;
  }

  template <typename T>
  T peek(int offset = 0) {
    T obj;
    memcpy(&obj, m_start + m_seek + offset, sizeof(T));
    return obj;
  }

  void ffwd(int amount) {
    m_seek += amount;
  }

  uint32_t get_seek() const { return m_seek; }
  void set_seek(u32 seek) { m_seek = seek; }
  u8 *getStart() const { return m_start; }

 protected:
  u8 * const m_start; //Start of data stream
  uint32_t m_seek = 0; //Current position within the data stream
};

class BinaryReader {
 public:
  explicit BinaryReader(std::span<const uint8_t> _span) : m_span(_span) {}

  template <typename T>
  T read() {
    ASSERT(m_seek + sizeof(T) <= m_span.size());
    T obj;
    memcpy(&obj, m_span.data() + m_seek, sizeof(T));
    m_seek += sizeof(T);
    return obj;
  }

  void ffwd(int amount) {
    m_seek += amount;
    ASSERT(m_seek <= m_span.size());
  }

  uint32_t bytes_left() const { return m_span.size() - m_seek; }
  const uint8_t* here() { return m_span.data() + m_seek; }
  BinaryReader at(u32 pos) { return BinaryReader(m_span.subspan(pos)); }
  uint32_t get_seek() const { return m_seek; }
  void set_seek(u32 seek) { m_seek = seek; }

 private:
  std::span<const u8> m_span;
  uint32_t m_seek = 0;
};
