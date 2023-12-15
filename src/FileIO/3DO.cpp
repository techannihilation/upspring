//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------
#include "EditorDef.h"
#include "EditorIncl.h"
#include "Model.h"
#include "Util.h"
#include "config.h"

#include "spdlog/spdlog.h"

class CTAPalette {
 public:
  CTAPalette() = default;

  void Init() {
    std::string const fn = (ups::config::get().app_path() / "data" / "palette.pal").string();
    FILE* f = fopen(fn.c_str(), "rb");
    if (f == nullptr) {
      if (!error) {
        spdlog::error("Failed to load data/palette.pal");
      }
      error = true;
    } else {
      for (auto& c : p) {
        for (unsigned char& c2 : c) {
          c2 = fgetc(f);
        }
        c[3] = 255;
      }
      fclose(f);
    }
  }

  int FindIndex(Vector3 color) {
    if (!loaded) {
      Init();
    }
    int const r = color.x * 255;
    int const g = color.y * 255;
    int const b = color.z * 255;
    int best = -1;
    int bestdif = 0;
    for (int a = 0; a < 256; a++) {
      int const dif = abs(r - p[a][0]) + abs(g - p[a][1]) + abs(b - p[a][2]);
      if (best < 0 || bestdif > dif) {
        bestdif = dif;
        best = a;
      }
    }
    return best;
  }

  Vector3 GetColor(int index) {
    if (!loaded) {
      Init();
    }
    if (index < 0 || index >= 256) {
      return Vector3();
    }
    return Vector3(p[index][0] / 255, p[index][1] / 255, p[index][2] / 255);
  }

  inline unsigned char* operator[](int a) { return p[a]; }
  unsigned char p[256][4]{};
  bool loaded{false}, error{false};
};
CTAPalette palette;

const float scaleFactor = 1 / (65536.0F);

#define FROM_TA(c) (float(c) * scaleFactor)
#define TO_TA(c) ((c) / scaleFactor)

// ------------------------------------------------------------------------------------------------
// 3DO support
// ------------------------------------------------------------------------------------------------

using TA_Object = struct {
  int VersionSignature;
  int NumberOfVertexes;
  int NumberOfPrimitives;
  int NoSelectionRect;
  int XFromParent;
  int YFromParent;
  int ZFromParent;
  int OffsetToObjectName;      // 28
  int Always_0;                // 32
  int OffsetToVertexArray;     // 36
  int OffsetToPrimitiveArray;  // 40
  int OffsetToSiblingObject;   // 44
  int OffsetToChildObject;     // 48
};

using TA_Polygon = struct {
  int PaletteIndex;
  int VertNum;
  int Always_0;
  int VertOfs;
  int TexnameOfs;
  int Unknown_1;
  int Unknown_2;
  int Unknown_3;
};

static MdlObject* load_object(int ofs, FILE* f, MdlObject* parent, int r = 0) {
  auto* n = new MdlObject;
  TA_Object obj;
  size_t read_result = 0;
  int const org = ftell(f);

  fseek(f, ofs, SEEK_SET);
  read_result = fread(&obj, sizeof(TA_Object), 1, f);
  if (read_result != static_cast<size_t>(1)) {
    throw std::runtime_error("Couldn't read 3DO header.");
  }

  if (obj.VersionSignature != 1) {
    spdlog::error("Wrong version. Only version 1 is supported");
    return nullptr;
  }

  int ipos[3];
  auto* pm = new PolyMesh;
  n->geometry = pm;
  pm->verts.resize(obj.NumberOfVertexes);

  fseek(f, obj.OffsetToVertexArray, SEEK_SET);
  for (int a = 0; a < obj.NumberOfVertexes; a++) {
    read_result = fread(ipos, sizeof(int), 3, f);
    if (read_result != static_cast<size_t>(3)) {
      throw std::runtime_error("Couldn't read vertexes.");
    }
    for (int b = 0; b < 3; b++) {
      pm->verts[a].pos[b] = FROM_TA(ipos[b]);
    }
  }

  std::vector<TA_Polygon> tapl;
  tapl.resize(obj.NumberOfPrimitives);

  fseek(f, obj.OffsetToPrimitiveArray, SEEK_SET);
  if (obj.NumberOfPrimitives > 0) {
    read_result = fread(tapl.data(), sizeof(TA_Polygon), obj.NumberOfPrimitives, f);
  }
  if (read_result != static_cast<size_t>(obj.NumberOfPrimitives) && obj.NumberOfPrimitives != 0) {
    throw std::runtime_error("Couldn't read primitives.");
  }
  for (int a = 0; a < obj.NumberOfPrimitives; a++) {
    fseek(f, tapl[a].VertOfs, SEEK_SET);

    Poly* p = new Poly;
    p->verts.resize(tapl[a].VertNum);

    for (int b = 0; b < tapl[a].VertNum; b++) {
      short vindex = 0;
      read_result = fread(&vindex, sizeof(short), 1, f);
      if (read_result != static_cast<size_t>(1)) {
        throw std::runtime_error("Couldn't read vertex.");
      }
      p->verts[b] = vindex;
    }

    p->taColor = tapl[a].PaletteIndex;
    p->color = palette.GetColor(tapl[a].PaletteIndex);

    if (tapl[a].TexnameOfs != 0) {
      fseek(f, tapl[a].TexnameOfs, SEEK_SET);
      p->texname = ReadZStr(f);
    }

    pm->poly.push_back(p);
  }

  fseek(f, obj.OffsetToObjectName, SEEK_SET);
  n->name = ReadZStr(f);

  n->position.x = FROM_TA(obj.XFromParent);
  n->position.y = FROM_TA(obj.YFromParent);
  n->position.z = FROM_TA(obj.ZFromParent);

  if (obj.OffsetToSiblingObject != 0) {
    if (parent == nullptr) {
      spdlog::error("Error: Root object can not have sibling nodes.");
    } else {
      MdlObject* sibling = load_object(obj.OffsetToSiblingObject, f, parent);

      if (sibling != nullptr) {
        parent->childs.push_back(sibling);
        sibling->parent = parent;
      }
    }
  }

  if (obj.OffsetToChildObject != 0) {
    MdlObject* ch = load_object(obj.OffsetToChildObject, f, n, r + 1);
    if (ch != nullptr) {
      n->childs.push_back(ch);
      ch->parent = n;
    }
  }

  fseek(f, org, SEEK_SET);
  return n;
}

bool Model::Load3DO(const char* filename, IProgressCtl& /*progctl*/) {
  FILE* f = nullptr;

  f = fopen(filename, "rb");
  if (f == nullptr) {
    return false;
  }

  root = load_object(0, f, nullptr);
  if (root == nullptr) {
    fclose(f);
    return false;
  }

  mapping = MAPPING_3DO;

  fclose(f);

  file_ = filename;

  return true;
}

static void save_object(FILE* f, MdlObject* parent, std::vector<MdlObject*>::iterator cur) {
  TA_Object n;
  MdlObject* obj = *cur++;
  PolyMesh* pm = obj->ToPolyMesh();
  size_t write_result = 0;
  if (pm == nullptr) {
    pm = new PolyMesh;
  }

  memset(&n, 0, sizeof(TA_Object));

  int const header = ftell(f);
  fseek(f, sizeof(TA_Object), SEEK_CUR);

  n.VersionSignature = 1;
  n.NumberOfPrimitives = pm->poly.size();
  n.XFromParent = TO_TA(obj->position.x);
  n.YFromParent = TO_TA(obj->position.y);
  n.ZFromParent = TO_TA(obj->position.z);
  n.NumberOfVertexes = pm->verts.size();

  n.OffsetToObjectName = ftell(f);
  WriteZStr(f, obj->name);

  n.OffsetToVertexArray = ftell(f);
  for (auto& vert : pm->verts) {
    int v[3];
    Vector3& p = vert.pos;
    for (int i = 0; i < 3; i++) {
      v[i] = TO_TA(p[i]);
    }
    write_result = fwrite(v, sizeof(int), 3, f);
    if (write_result != static_cast<size_t>(3)) {
      throw std::runtime_error("Couldn't write vertex.");
    }
  }

  n.OffsetToPrimitiveArray = ftell(f);
  auto* tapl = new TA_Polygon[pm->poly.size()];
  fseek(f, sizeof(TA_Polygon) * pm->poly.size(), SEEK_CUR);
  memset(tapl, 0, sizeof(TA_Polygon) * pm->poly.size());

  for (unsigned int a = 0; a < pm->poly.size(); a++) {
    Poly* pl = pm->poly[a];

    if (pl->taColor >= 0) {
      tapl[a].PaletteIndex = pl->taColor;
    } else {
      tapl[a].PaletteIndex = palette.FindIndex(pl->color);
    }

    tapl[a].TexnameOfs = ftell(f);
    WriteZStr(f, pl->texname);
    tapl[a].VertOfs = ftell(f);
    for (int const vert : pl->verts) {
      unsigned short v = 0;
      v = vert;
      write_result = fwrite(&v, sizeof(short), 1, f);
      if (write_result != static_cast<size_t>(1)) {
        throw std::runtime_error("Couldn't write vertex.");
      }
    }
    tapl[a].VertNum = pl->verts.size();
    tapl[a].Unknown_1 = 0;
    tapl[a].Unknown_2 = 0;
    tapl[a].Unknown_3 = 0;
  }

  int old = ftell(f);
  fseek(f, n.OffsetToPrimitiveArray, SEEK_SET);
  write_result = fwrite(tapl, sizeof(TA_Polygon), pm->poly.size(), f);
  if (write_result != static_cast<size_t>(pm->poly.size())) {
    throw std::runtime_error("Couldn't write polygon.");
  }
  fseek(f, old, SEEK_SET);

  delete pm;

  if (parent != nullptr) {
    if (cur != parent->childs.end()) {
      n.OffsetToSiblingObject = ftell(f);
      save_object(f, parent, cur);
    }
  }

  if (static_cast<unsigned int>(!obj->childs.empty()) != 0U) {
    n.OffsetToChildObject = ftell(f);
    save_object(f, obj, obj->childs.begin());
  }

  old = ftell(f);
  fseek(f, header, SEEK_SET);
  write_result = fwrite(&n, sizeof(TA_Object), 1, f);
  if (write_result != static_cast<size_t>(1)) {
    throw std::runtime_error("Couldn't write 3DO header.");
  }
  fseek(f, old, SEEK_SET);
}

// 3DO only has offsets
static inline void ApplyOrientationAndScaling(MdlObject* o) {
  o->ApplyTransform(true, true, false);
}

bool Model::Save3DO(const char* fn, IProgressCtl& /*progctl*/) const {
  FILE* f = fopen(fn, "wb");

  if (f == nullptr) {
    throw std::runtime_error("Couldn't open 3DO file for writing.");
  }
  return false;

  MdlObject* cl = root->Clone();
  IterateObjects(cl, ApplyOrientationAndScaling);

  std::vector<MdlObject*> tmp;
  tmp.push_back(cl);
  save_object(f, nullptr, tmp.begin());
  fclose(f);

  delete cl;
  return true;
}
