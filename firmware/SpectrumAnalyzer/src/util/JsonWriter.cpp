#include "JsonWriter.h"

#include <cstdio>

void JsonWriter::separate() {
  if (afterKey_) {
    afterKey_ = false;   // value right after its key: no comma
    return;
  }
  if (needComma_) out_ += ',';
}

JsonWriter& JsonWriter::open(char c) {
  separate();
  out_ += c;
  needComma_ = false;
  return *this;
}

JsonWriter& JsonWriter::close(char c) {
  out_ += c;
  needComma_ = true;
  afterKey_ = false;
  return *this;
}

void JsonWriter::appendString(const char* s) {
  out_ += '"';
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p != 0; ++p) {
    switch (*p) {
      case '"': out_ += "\\\""; break;
      case '\\': out_ += "\\\\"; break;
      case '\n': out_ += "\\n"; break;
      case '\r': out_ += "\\r"; break;
      case '\t': out_ += "\\t"; break;
      default:
        if (*p < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", *p);
          out_ += buf;
        } else {
          out_ += static_cast<char>(*p);   // UTF-8 passes through unchanged
        }
    }
  }
  out_ += '"';
}

JsonWriter& JsonWriter::key(const char* k) {
  separate();
  appendString(k);
  out_ += ':';
  afterKey_ = true;
  return *this;
}

JsonWriter& JsonWriter::value(const char* s) {
  separate();
  appendString(s != nullptr ? s : "");
  needComma_ = true;
  return *this;
}

JsonWriter& JsonWriter::value(bool b) {
  separate();
  out_ += b ? "true" : "false";
  needComma_ = true;
  return *this;
}
