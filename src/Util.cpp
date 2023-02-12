//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#include "EditorIncl.h"
#include "EditorDef.h"

#include "spdlog/spdlog.h"

#include "Util.h"

// Printf style std::string formatting
std::string SPrintf(const char* fmt, ...) {
  va_list vl;
  va_start(vl, fmt);
  char buf[256];
  VSNPRINTF(buf, sizeof(buf), fmt, vl);
  va_end(vl);

  return buf;
}

std::string ReadZStr(FILE* f) {
  std::string s;
  int c;
  while ((c = fgetc(f)) != EOF && c) s += c;
  return s;
}

void WriteZStr(FILE* f, const std::string& s) {
  std::size_t c = s.length();
  fwrite(s.data(), c + 1, 1, f);
}

std::string GetFilePath(const std::string& fn) {
  std::string mdlPath = fn;
  std::size_t const pos = mdlPath.rfind('\\');
  std::size_t const pos2 = mdlPath.rfind('/');
  if (pos != std::string::npos) {
    mdlPath.erase(pos + 1, mdlPath.size());
  } else if (pos2 != std::string::npos) {
    mdlPath.erase(pos2 + 1, mdlPath.size());
  }
  return mdlPath;
}

void AddTrailingSlash(std::string& tld) {
  if (tld.rfind('/') != tld.length() - 1 && tld.rfind('\\') != tld.length() - 1) {
    tld.push_back('/');
  }
}

std::string Readstring(int offset, FILE* f) {
  int const oldofs = ftell(f);
  fseek(f, offset, SEEK_SET);
  std::string str = ReadZStr(f);
  fseek(f, oldofs, SEEK_SET);
  return str;
}

// ------------------------------------------------------------------------------------------------
// Globals
// ------------------------------------------------------------------------------------------------

void usDebugBreak() {
#ifdef _DEBUG
  __asm int 3
#else
  fltk::message("An error has occured in the program, so it has to abort now.");
#endif
}

void usAssertFailed(const char* condition, const char* file, int line) {
  d_trace("%s(%d): Assertion failed (Condition \'%s\')\n", file, line, condition);
  usDebugBreak();
}

//-------------------------------------------------------------------------
// InputBuffer - serves as an input for the config value nodes
//-------------------------------------------------------------------------

void InputBuffer::ShowLocation() const { spdlog::debug(Location()); }

std::string InputBuffer::Location() const { return SPrintf("In %s on line %d:", filename, line); }

bool InputBuffer::CompareIdent(const char* str) const {
  int i = 0;
  while ((str[i] != 0) && i + pos < len) {
    if (str[i] != data[pos + i]) {
      return false;
    }

    i++;
  }

  return str[i] == 0;
}

bool InputBuffer::SkipWhitespace() {
  // Skip whitespaces and comments
  for (; !end(); next()) {
    if (get() == ' ' || get() == '\t' || get() == '\r' || get() == '\f' || get() == 'v') {
      continue;
    }
    if (get() == '\n') {
      line++;
      continue;
    }
    if (get(0) == '/' && get(1) == '/') {
      pos += 2;
      while (get() != '\n' && !end()) {
        next();
      }
      ++line;
      continue;
    }
    if (get() == '/' && get(1) == '*') {
      pos += 2;
      while (!end()) {
        if (get(0) == '*' && get(1) == '/') {
          pos += 2;
          break;
        }
        if (get() == '\n') {
          line++;
        }
        next();
      }
      continue;
    }
    break;
  }

  return end();
}

void InputBuffer::SkipKeyword(const char* kw) {
  SkipWhitespace();

  int a = 0;
  while (!end() && (kw[a] != 0)) {
    if (get() != kw[a]) {
      break;
    }
    next(), ++a;
  }

  if (kw[a] != 0) {
    throw content_error(SPrintf("%s: Expecting keyword %s\n", Location().c_str(), kw));
  }
}

void InputBuffer::Expecting(const char* s) const {
  ShowLocation();
  spdlog::debug("Expecting {}", s);
}

std::string InputBuffer::ParseIdent() {
  std::string name;

  if (isalnum(get()) != 0) {
    name += get();
    for (next(); (isalnum(get()) != 0) || get() == '_' || get() == '-'; next()) {
      name += get();
    }

    return name;
  }
  throw content_error(
      SPrintf("%s: Expecting an identifier instead of '%c'\n", Location().c_str(), get()));
}
