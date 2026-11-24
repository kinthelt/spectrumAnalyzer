// Minimal JSON writer: objects, arrays, strings (escaped), numbers, booleans.
// Commas are handled for you. Plain C++ so it is unit-tested on a PC.
//
//   JsonWriter j;
//   j.beginObject().key("name").value("Movie \"night\"").key("on").value(true).endObject();
//   j.str()  ->  {"name":"Movie \"night\"","on":true}
#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

class JsonWriter {
 public:
  JsonWriter& beginObject() { return open('{'); }
  JsonWriter& endObject() { return close('}'); }
  JsonWriter& beginArray() { return open('['); }
  JsonWriter& endArray() { return close(']'); }

  JsonWriter& key(const char* k);
  JsonWriter& value(const char* s);
  JsonWriter& value(const std::string& s) { return value(s.c_str()); }
  JsonWriter& value(bool b);
  // Any integer type (int, int32_t, uint32_t, ...: they differ between the PC
  // and the ESP32, so one template rather than overloads that may collide).
  template <typename T,
            typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value,
                                    int>::type = 0>
  JsonWriter& value(T n) {
    separate();
    out_ += std::to_string(n);
    needComma_ = true;
    return *this;
  }

  const std::string& str() const { return out_; }

 private:
  JsonWriter& open(char c);
  JsonWriter& close(char c);
  void separate();          // a comma before a value, unless first in its container
  void appendString(const char* s);

  std::string out_;
  bool needComma_ = false;
  bool afterKey_ = false;
};
