//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------
#include "EditorIncl.h"
#include "EditorDef.h"
#include "CfgParser.h"
#include "Util.h"

// this keeps a list of implemented value types
std::vector<CfgValueClass*> CfgValue::classes;

//-------------------------------------------------------------------------
// CfgWriter - "outputdevice" for the config value nodes
//-------------------------------------------------------------------------

CfgWriter::CfgWriter(const char* name) : indentLevel(0), out(fopen(name, "w")) {}

CfgWriter::~CfgWriter() {
  if (out != nullptr) {
    fclose(out);
    out = nullptr;
  }
}

bool CfgWriter::IsFailed() { return out == nullptr || (ferror(out) != 0); }

void CfgWriter::DecIndent() { indentLevel--; }
void CfgWriter::IncIndent() { indentLevel++; }

CfgWriter& CfgWriter::operator<<(char c) {
  fputc(c, out);
  MakeIndent(c);
  return *this;
}

CfgWriter& CfgWriter::operator<<(const std::string& s) {
  fputs(s.c_str(), out);
  if (!s.empty()) {
    MakeIndent(s.at(s.size() - 1));
  }
  return *this;
}

CfgWriter& CfgWriter::operator<<(const char* str) {
  fputs(str, out);
  int const l = strlen(str);
  if (l != 0) {
    MakeIndent(str[l - 1]);
  }
  return *this;
}

void CfgWriter::MakeIndent(char c) {
  if (c != 0x0D && c != 0x0A) {
    return;
  }

  for (int a = 0; a < indentLevel; a++) {
    fputs("  ", out);
  }
}

//-------------------------------------------------------------------------
// CfgValue - base config parsing class
//-------------------------------------------------------------------------

void CfgValue::AddValueClass(CfgValueClass* vc) {
  if (find(classes.begin(), classes.end(), vc) == classes.end()) {
    classes.push_back(vc);
  }
}

CfgValue* CfgValue::ParseValue(InputBuffer& buf) {
  CfgValue* v = nullptr;

  if (buf.SkipWhitespace()) {
    buf.Expecting("Value");
    return nullptr;
  }

  for (auto& classe : classes) {
    if (classe->Identify(buf)) {
      v = classe->Create();

      if (!v->Parse(buf)) {
        delete v;
        return nullptr;
      }

      return v;
    }
  }

  // parse standard value types:
  char const r = Lookahead(buf);

  if (buf.CompareIdent("file")) {
    // load a nested config file
    return LoadNestedFile(buf);
  }
  if (isalpha(*buf) != 0) {
    v = new CfgLiteral;
    (dynamic_cast<CfgLiteral*>(v))->ident = true;
  } else if ((isdigit(r) != 0) || *buf == '.' || *buf == '-') {
    v = new CfgNumeric;
  } else if (*buf == '"') {
    v = new CfgLiteral;
  } else if (*buf == '{') {
    v = new CfgList;
  }

  if ((v != nullptr) && !v->Parse(buf)) {
    delete v;
    return nullptr;
  }

  return v;
}

void CfgValue::Write(CfgWriter& /*w*/) {}

CfgList* CfgValue::LoadNestedFile(InputBuffer& buf) {
  std::string s;
  buf.SkipKeyword("file");

  buf.SkipWhitespace();

  CfgLiteral l;
  if (!l.Parse(buf)) {
    return nullptr;
  }
  s = l.value;

  // insert the path of the current file in the string
  int i = strlen(buf.filename) - 1;
  while (i > 0) {
    if (buf.filename[i] == '\\' || buf.filename[i] == '/') {
      break;
    }
    i--;
  }

  s.insert(s.begin(), buf.filename, buf.filename + i + 1);
  return LoadFile(s.c_str());
}

CfgList* CfgValue::LoadFile(const char* name) {
  InputBuffer buf;

  FILE* f = fopen(name, "rb");
  if (f == nullptr) {
    return nullptr;
  }

  fseek(f, 0, SEEK_END);
  buf.len = ftell(f);
  buf.data = new char[buf.len];
  fseek(f, 0, SEEK_SET);
  if (fread(buf.data, buf.len, 1, f) == 0U) {
    fclose(f);
    delete[] buf.data;
    return nullptr;
  }
  buf.filename = name;

  fclose(f);

  auto* nlist = new CfgList;
  if (!nlist->Parse(buf, true)) {
    delete nlist;
    delete[] buf.data;
    return nullptr;
  }

  delete[] buf.data;
  return nlist;
}

char CfgValue::Lookahead(InputBuffer& buf) {
  InputBuffer cp = buf;

  if (!cp.SkipWhitespace()) {
    return *cp;
  }

  return 0;
}

//-------------------------------------------------------------------------
// CfgNumeric - parses int/float/double
//-------------------------------------------------------------------------

bool CfgNumeric::Parse(InputBuffer& buf) {
  bool dot = *buf == '.';
  std::string str;
  str += *buf;
  ++buf;
  while (1) {
    if (*buf == '.') {
      if (dot) {
        break;
      }
      dot = true;
    }
    if (!(isdigit(*buf) != 0) && *buf != '.') {
      break;
    }
    str += *buf;
    ++buf;
  }
  if (dot) {
    value = atof(str.c_str());
  } else {
    value = atoi(str.c_str());
  }
  return true;
}

void CfgNumeric::Write(CfgWriter& w) {
  CfgValue::Write(w);

  char tmp[20];
  SNPRINTF(tmp, 20, "%g", value);
  w << tmp;
}

//-------------------------------------------------------------------------
// CfgLiteral - parses std::string constants
//-------------------------------------------------------------------------
bool CfgLiteral::Parse(InputBuffer& buf) {
  if (ident) {
    value = buf.ParseIdent();
    return true;
  }

  ++buf;
  while (*buf != '\n') {
    if (*buf == '\\') {
      if (buf[1] == '"') {
        value += buf[1];
        buf.pos += 2;
        continue;
      }
    }

    if (*buf == '"') {
      break;
    }

    value += *buf;
    ++buf;
  }
  ++buf;
  return true;
}

void CfgLiteral::Write(CfgWriter& w) {
  if (ident) {
    w << value;
  } else {
    w << '"' << value << '"';
  }
}

//-------------------------------------------------------------------------
// CfgList & CfgListElem - parses a list of values enclosed with { }
//-------------------------------------------------------------------------

bool CfgListElem::Parse(InputBuffer& buf) {
  if (buf.SkipWhitespace()) {
    return false;
  }

  // parse name
  name = buf.ParseIdent();
  buf.SkipWhitespace();

  if (*buf == '=') {
    ++buf;

    value = ParseValue(buf);

    return value != nullptr;
  }
  return true;
}

void CfgListElem::Write(CfgWriter& w) {
  w << name;

  if (value != nullptr) {
    w << " = ";
    value->Write(w);
  }
  w << "\n";
}

bool CfgList::Parse(InputBuffer& buf, bool root) {
  if (!root) {
    buf.SkipWhitespace();

    if (*buf != '{') {
      buf.Expecting("{");
      return false;
    }

    ++buf;
  }

  while (!buf.SkipWhitespace()) {
    if (*buf == '}') {
      ++buf;
      return true;
    }

    childs.emplace_back();
    if (!childs.back().Parse(buf)) {
      return false;
    }
  }

  return root || !buf.end();
}

void CfgList::Write(CfgWriter& w, bool root) {
  if (!root) {
    w << '{';
    w.IncIndent();
    w << "\n";
  }
  for (auto& child : childs) {
    child.Write(w);
  }
  if (!root) {
    w << '}';
    w.DecIndent();
    w << "\n";
  }
}

CfgValue* CfgList::GetValue(const char* name) {
  for (auto& child : childs) {
    if (!STRCASECMP(child.name.c_str(), name)) {
      return child.value;
    }
  }
  return nullptr;
}

double CfgList::GetNumeric(const char* name, double def) {
  CfgValue* v = GetValue(name);
  if (v == nullptr) {
    return def;
  }

  auto* n = dynamic_cast<CfgNumeric*>(v);
  if (n == nullptr) {
    return def;
  }

  return n->value;
}

const char* CfgList::GetLiteral(const char* name, const char* def) {
  CfgValue* v = GetValue(name);
  if (v == nullptr) {
    return def;
  }

  auto* n = dynamic_cast<CfgLiteral*>(v);
  if (n == nullptr) {
    return def;
  }

  return n->value.c_str();
}

void CfgList::AddLiteral(const char* name, const char* val) {
  auto* l = new CfgLiteral;
  l->value = val;
  childs.emplace_back();
  childs.back().value = l;
  childs.back().name = name;
}

void CfgList::AddNumeric(const char* name, double val) {
  auto* n = new CfgNumeric;
  n->value = val;
  childs.emplace_back();
  childs.back().value = n;
  childs.back().name = name;
}

void CfgList::AddValue(const char* name, CfgValue* val) {
  childs.emplace_back();
  childs.back().value = val;
  childs.back().name = name;
}
