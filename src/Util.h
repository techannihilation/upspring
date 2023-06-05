//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------
#pragma once

#include <string>

#include "DebugTrace.h"

struct InputBuffer {
  InputBuffer() : pos(0), len(0), line(1), data(0), filename(0) {}
  char operator*() const { return data[pos]; }
  bool end() const { return pos == len; }
  InputBuffer& operator++() {
    next();
    return *this;
  }
  InputBuffer operator++(int) {
    InputBuffer t = *this;
    next();
    return t;
  }
  char operator[](int i) const { return data[pos + i]; }
  void ShowLocation() const;
  bool CompareIdent(const char* str) const;
  char get() const { return data[pos]; }
  char get(int i) const { return data[pos + i]; }
  void next() {
    if (pos < len) pos++;
  }
  bool SkipWhitespace();
  std::string Location() const;
  void SkipKeyword(const char* kw);
  void Expecting(const char* s) const;  // show an error message saying that 's' was expected
  std::string ParseIdent();

  int pos;
  int len;
  int line;
  char* data;
  const char* filename;
};

std::string Readstring(int offset, FILE* f);
std::string ReadZStr(FILE* f);
void WriteZStr(FILE* f, const std::string& s);
std::string GetFilePath(const std::string& fn);
void AddTrailingSlash(std::string& tld);

std::string SPrintf(const char* fmt, ...);