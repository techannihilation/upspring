//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#include "EditorIncl.h"
#include "EditorDef.h"

#include "Util.h"
#include "Model.h"
#include "View.h"
#include "Texture.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

#include "string_util.h"

#include "math/hash.h"

#include "CurvedSurface.h"

#include "MeshIterators.h"

#include "spdlog/spdlog.h"

#include <unordered_map>

// ------------------------------------------------------------------------------------------------
// Model
// ------------------------------------------------------------------------------------------------

Model::Model() : height(radius = 0.0F) {}

Model::~Model() { delete root; }

void Model::SetTextureName(uint index, const char* name) {
  if (texBindings.size() <= index) {
    texBindings.resize(index + 1);
  }

  texBindings[index].name = name;
}

void Model::SetTexture(uint index, std::shared_ptr<Texture> par_tex) {
  if (texBindings.size() <= index) {
    texBindings.resize(index + 1);
  }

  texBindings[index].texture = par_tex;
  texBindings[index].name = par_tex ? par_tex->name : "";
}

void Model::InsertModel(MdlObject* obj, Model* sub) {
  if (sub->root == nullptr) {
    return;
  }

  obj->childs.push_back(sub->root);
  sub->root->parent = obj;
  sub->root = nullptr;
}

// TODO(unknown): Abstract file formats
Model* Model::Load(const std::string& _fn, bool /*Optimize*/, IProgressCtl& progctl) {
  const char* fn = _fn.c_str();
  const char* ext = fltk::filename_ext(fn);
  Model* mdl = nullptr;

  try {
    bool r = false;
    mdl = new Model;

    if (!STRCASECMP(ext, ".3do")) {
      r = mdl->Load3DO(fn, progctl);
    } else if (!STRCASECMP(ext, ".s3o")) {
      r = mdl->LoadS3O(fn, progctl);
    } else if (!STRCASECMP(ext, ".3ds")) {
      r = (mdl->root = Load3DSObject(fn, progctl)) != nullptr;
    } else if (!STRCASECMP(ext, ".obj")) {
      r = (mdl->root = LoadWavefrontObject(fn, progctl)) != nullptr;
    } else {
      fltk::message("Unknown extension %s\n", fltk::filename_ext(fn));
      delete mdl;
      return nullptr;
    }
    if (!r) {
      delete mdl;
      mdl = nullptr;
    }
  } catch (std::runtime_error err) {
    fltk::message(err.what());
    return nullptr;
  }
  if (mdl != nullptr) {
    return mdl;
  }
  fltk::message("Failed to read file %s:\n", fn);
  return nullptr;
}

bool Model::Save(Model* mdl, const std::string& _fn, IProgressCtl& progctl) {
  bool r = false;
  const char* fn = _fn.c_str();
  const char* ext = fltk::filename_ext(fn);

  if (mdl->root == nullptr) {
    fltk::message("No objects");
    return false;
  }

  assert(fn && fn[0]);
  if (!STRCASECMP(ext, ".3do")) {
    r = mdl->Save3DO(fn, progctl);
  } else if (!STRCASECMP(ext, ".s3o")) {
    r = mdl->SaveS3O(fn, progctl);
  } else if (!STRCASECMP(ext, ".3ds")) {
    r = Save3DSObject(fn, mdl->root, progctl);
  } else if (!STRCASECMP(ext, ".obj")) {
    r = SaveWavefrontObject(fn, mdl->root);
  } else {
    fltk::message("Unknown extension %s\n", fltk::filename_ext(fn));
  }
  if (!r) {
    fltk::message("Failed to save file %s\n", fn);
  }
  return r;
}

static void GetSelectedObjectsHelper(MdlObject* obj, std::vector<MdlObject*>& sel) {
  if (obj->isSelected) {
    sel.push_back(obj);
  }

  for (auto& child : obj->childs) {
    GetSelectedObjectsHelper(child, sel);
  }
}

std::vector<MdlObject*> Model::GetSelectedObjects() const {
  std::vector<MdlObject*> sel;
  if (root != nullptr) {
    GetSelectedObjectsHelper(root, sel);
  }
  return sel;
}

void Model::DeleteObject(MdlObject* obj) {
  if (obj->parent != nullptr) {
    auto const i = find(obj->parent->childs.begin(), obj->parent->childs.end(), obj);
    if (i != obj->parent->childs.end()) {
      obj->parent->childs.erase(i);
    }
  } else {
    assert(obj == root);
    root = nullptr;
  }

  delete obj;
}

void Model::SwapObjects(MdlObject* a, MdlObject* b) {
  MdlObject* ap = a->parent;
  MdlObject* bp = b->parent;

  // Unlink both objects from their parents
  // (this will also fix problems if b is a child of a or vice versa)
  a->UnlinkFromParent();
  b->UnlinkFromParent();

  // Swap childs
  swap(a->childs, b->childs);
  // assign parents again on the swapped childs
  for (auto ci = a->childs.begin(); ci != a->childs.end(); ci++) {
    (*ci)->parent = a;
  }
  for (auto ci = b->childs.begin(); ci != b->childs.end(); ci++) {
    (*ci)->parent = b;
  }

  if (b == ap) {  // was b originally the parent of a?
    b->LinkToParent(a);
    if (bp != nullptr) {
      a->LinkToParent(bp);
    }
  } else if (a == bp) {  // or was a originally the parent of b
    a->LinkToParent(b);
    if (ap != nullptr) {
      b->LinkToParent(ap);
    }
  } else {  // no parents of eachother at all
    if (bp != nullptr) {
      a->LinkToParent(bp);
    }
    if (ap != nullptr) {
      b->LinkToParent(ap);
    }
  }

  if (a == root) {
    root = b;
  } else if (b == root) {
    root = a;
  }
}

void Model::ReplaceObject(MdlObject* old, MdlObject* _new) {
  // Insert the old childs into the new object
  _new->childs.insert(_new->childs.end(), old->childs.begin(), old->childs.end());
  old->childs.clear();

  // Delete the old object
  MdlObject* parent = old->parent;
  DeleteObject(old);

  // Insert the new object
  if (parent != nullptr) {
    parent->childs.push_back(_new);
  } else {
    root = _new;
  }
}

static void AddPositions(MdlObject* o, Vector3& p, int& count) {
  Matrix transform;
  o->GetTransform(transform);

  for (VertexIterator v(o); !v.End(); v.Next()) {
    Vector3 temp;
    transform.apply(&v->pos, &temp);
    p += temp;
    count++;
  }
  for (auto& child : o->childs) {
    AddPositions(child, p, count);
  }
}

void Model::EstimateMidPosition() {
  Vector3 total;
  int count = 0;
  if (root != nullptr) {
    AddPositions(root, total, count);
  }

  if (count != 0) {
    mid = total / count;
  } else {
    mid = Vector3();
  }
}

void Model::CalculateRadius() {
  std::vector<MdlObject*> const objs = GetObjectList();
  radius = 0.0F;
  for (auto* obj : objs) {
    /*PolyMesh *pm = obj->GetPolyMesh();*/
    Matrix objTransform;
    obj->GetFullTransform(objTransform);
    if (obj->geometry != nullptr) {
      obj->geometry->CalculateRadius(radius, objTransform, mid);
    }
  }
}

bool Model::ExportUVMesh(const char* fn) const {
  // create a cloned model
  MdlObject* cloned = root->Clone();
  Model mdl;
  mdl.root = cloned;
  mdl.texBindings = texBindings;

  // merge all objects in the clone
  while (!mdl.root->childs.empty()) {
    std::vector<MdlObject*> const childs = mdl.root->childs;

    for (const auto& child : childs) {
      mdl.root->MergeChild(child);
    }
  }

  return Model::Save(&mdl, fn);
}

bool Model::ImportUVMesh(const char* fn, IProgressCtl& progctl) const {
  Model* mdl = nullptr;
  mdl = Model::Load(fn, false);  // an unoptimized mesh so the vertices are not merged
  return false;

  if (!ImportUVCoords(mdl, progctl)) {
    delete mdl;
    return false;
  }
  delete mdl;
  return true;
}

int MatchPolygon(MdlObject* root, std::vector<Vector3>& pverts, int& startVertex) {
  int index = 0;
  for (PolyIterator pi(root); !pi.End(); pi.Next(), index++) {
    Poly* pl = *pi;

    if (pl->verts.size() != pverts.size() || (pi.verts() == nullptr)) {
      continue;
    }

    // An early out plane comparision, will also make sure that "double-sided" polgyon pairs
    // are handled correctly
    Plane const plane = pl->CalcPlane(*pi.verts());
    Plane tplane;
    tplane.MakePlane(pverts[0], pverts[1], pverts[2]);

    if (!plane.EpsilonCompare(tplane, EPSILON)) {
      continue;
    }

    // in case the polygon vertices have been reordered,
    // this takes care of finding "the first" vertex again
    uint startv = 0;
    for (; startv < pverts.size(); startv++) {
      if (((*pi.verts())[pl->verts[0]].pos - pverts[startv]).length() < EPSILON) {
        break;
      }
    }
    // no start vertex has been found
    if (startv == pverts.size()) {
      continue;
    }

    // compare the polygon vertices with eachother...
    uint v = 0;
    for (; v < pverts.size(); v++) {
      if (v == pverts.size()) {
        startVertex = static_cast<int>(startv);
        return index;
      }
    }
  }
  return -1;
}

bool Model::ImportUVCoords(Model* other, IProgressCtl& progctl) const {
  std::vector<MdlObject*> const objects = GetObjectList();
  MdlObject* srcobj = other->root;

  std::vector<Vector3> pverts;

  int numPl = 0;
  int curPl = 0;
  for (const auto& object : objects) {
    PolyMesh* pm = object->GetPolyMesh();
    if (pm != nullptr) {
      numPl += pm->poly.size();
    }
  }

  for (auto* obj : objects) {
    Matrix objTransform;
    obj->GetFullTransform(objTransform);
    PolyMesh* pm = obj->GetPolyMesh();

    // give each polygon an independent set of vertices, this will be optimized back to normal later
    std::vector<Vertex> nverts;
    for (PolyIterator pi(obj); !pi.End(); pi.Next()) {
      for (int& vert : pi->verts) {
        nverts.push_back(pm->verts[vert]);
        vert = nverts.size() - 1;
      }
    }
    pm->verts = nverts;

    // match our polygons with the ones of the other model
    for (PolyIterator pi(obj); !pi.End(); pi.Next()) {
      pverts.clear();
      for (int const vert : pi->verts) {
        Vector3 tpos;
        objTransform.apply(&pm->verts[vert].pos, &tpos);
        pverts.push_back(tpos);
      }

      int startVertex = 0;
      int const bestpl = MatchPolygon(srcobj, pverts, startVertex);
      PolyMesh* srcpm = srcobj->GetPolyMesh();
      if (bestpl >= 0) {
        // copy texture coordinates from rt->poly[bestpl] to pl
        Poly* src = srcpm->poly[bestpl];
        for (uint v = 0; v < src->verts.size(); v++) {
          Vertex& dstvrt = pm->verts[pi->verts[(v + startVertex) % pi->verts.size()]];
          dstvrt.tc[0] = srcpm->verts[src->verts[v]].tc[0];
        }
      }

      progctl.Update(static_cast<float>(curPl++) / numPl);
    }
    obj->InvalidateRenderData();
  }

  return true;
}

static void GetObjectListHelper(MdlObject* obj, std::vector<MdlObject*>& list) {
  list.push_back(obj);
  for (auto& child : obj->childs) {
    GetObjectListHelper(child, list);
  }
}

std::vector<MdlObject*> Model::GetObjectList() const {
  std::vector<MdlObject*> objlist;
  if (root != nullptr) {
    GetObjectListHelper(root, objlist);
  }
  return objlist;
}

std::vector<PolyMesh*> Model::GetPolyMeshList() const {
  std::vector<MdlObject*> const objlist = GetObjectList();
  std::vector<PolyMesh*> pmlist;
  for (const auto& a : objlist) {
    if (a->GetPolyMesh() != nullptr) {
      pmlist.push_back(a->GetPolyMesh());
    }
  }
  return pmlist;
}

// loads textures after a creg read serialization
void Model::PostLoad() {
  for (auto& texBinding : texBindings) {
    if (texBinding.name.empty()) {
      continue;
    }

    texBinding.texture = std::make_shared<Texture>(texBinding.name);
    if (texBinding.texture->HasError()) {
      texBinding.texture = nullptr;
    }
  }
}

Model* Model::Clone() const {
  auto* cpy = new Model();

  cpy->texBindings = texBindings;
  cpy->radius = radius;
  cpy->height = height;
  cpy->mid = mid;
  cpy->mapping = mapping;

  if (root != nullptr) {
    cpy->root = root->Clone();
  }

  return cpy;
}

unsigned long Model::ObjectSelectionHash() const {
  unsigned long ch = 0;
  unsigned long a = 63689;
  std::vector<MdlObject*> const sel = GetSelectedObjects();

  for (const auto& x : sel) {
    ch = ch * a + (intptr_t)x;
    a *= 378551;
  }

  return ch;
}

uint Vector3ToRGB(Vector3 v) {
  return (static_cast<uint>(v.x * 255.0F) << 16) | (static_cast<uint>(v.y * 255.0F) << 8) |
         (static_cast<uint>(v.z * 255.0F) << 0);
}

bool Model::ConvertToS3O(std::string textureName, int texw, int texh) {
  std::unordered_map<std::string, std::shared_ptr<Texture>> textures;

  std::vector<PolyMesh*> pmlist = GetPolyMeshList();
  for (auto& polymesh : pmlist) {
    for (auto& poly : polymesh->poly) {
      if (poly->texture) {
        if (textures.find(poly->texname) != textures.end()) {
          continue;
        }
        textures.insert({poly->texname, poly->texture});

      } else if (poly->color.x != 0.0F) {  // create a new color texture
        const std::string color_name(SPrintf("Color%d", Vector3ToRGB(poly->color)));

        auto ci_ti = textures.find(color_name);
        if (ci_ti != textures.end()) {
          poly->texture = ci_ti->second;
          continue;
        }

        auto color_image = std::make_shared<Image>();
        color_image->name(color_name);
        if (!color_image->create(1, 1, poly->color.x, poly->color.y, poly->color.z)) {
          spdlog::error("Failed to create a color image, error was: {}", color_image->error());
          continue;
        }

        auto color_tex = std::make_shared<Texture>(color_image, color_name);
        textures.insert({color_name, color_tex});

      } else {
        continue;
      }
    }
  }

  TextureBinTree tree(texw, texh);
  std::map<std::string, TextureBinTree::Node*> texToNode;

  for (auto& texmap_entry : textures) {
    auto img = texmap_entry.second->image;

    // On teamcolor we copy the red channel to green, on normal textures we add a opaque alpha.
    if (texture_handler_->has_team_color(img->name())) {
      if (!img->threedo_to_s3o()) {
        spdlog::error("image->threedo_to_s3o failed, error was: {}", img->error());
        continue;
      }
    } else if (!img->add_opaque_alpha()) {
      spdlog::error("image->add_opaque_alpha failed, error was: {}", img->error());
      continue;
    }

    TextureBinTree::Node* node = tree.AddNode(img);

    if (node == nullptr) {
      spdlog::error("Not enough texture space for all 3DO textures");
      return false;
    }

    texToNode.insert({texmap_entry.first, node});
  }

  auto img = tree.GetResult();
  auto ext = std::filesystem::path(textureName).extension();

  if (!img->flip()) {
    spdlog::error("Failed to flip the atlas, error was: {}", img->error());
    return false;
  }

  if (!img->save(textureName)) {
    spdlog::error("Failed to save the atlas");
    return false;
  }

  // if (!img->flip()) {
  //   spdlog::error("Failed to flip the atlas, error was: {}", img->error());
  //   return false;
  // }

  auto tex1 = std::make_shared<Texture>();
  tex1->SetImage(img);
  tex1->name = std::filesystem::path(textureName).filename().string();
  SetTexture(0, tex1);

  // now set new texture coordinates.
  // Vertices might need to be split, so vertices are calculated per frame and then optimized again

  for (auto& polymesh : pmlist) {
    std::vector<Vertex> vertices;

    for (auto& poly : polymesh->poly) {
      auto* tnode = texToNode[poly->texname];

      if (poly->verts.size() <= 4) {
        const float tc[] = {0.0F, 1.0F, 1.0F, 1.0F, 1.0F, 0.0F, 0.0F, 0.0F};

        for (uint v = 0; v < poly->verts.size(); v++) {
          vertices.push_back(polymesh->verts[poly->verts[v]]);
          Vertex& vrt = vertices.back();
          // convert to texturebintree UV coords:
          if (tnode != nullptr) {
            vrt.tc[0].x = tree.GetU(tnode, tc[v * 2 + 0]);
            vrt.tc[0].y = tree.GetV(tnode, tc[v * 2 + 1]);
          }

          poly->verts[v] = vertices.size() - 1;
        }
      } else {
        for (int& vert : poly->verts) {
          vertices.push_back(polymesh->verts[vert]);
          Vertex& vrt = vertices.back();
          vrt.tc[0].x = vrt.tc[0].y = 0.0F;
          vert = vertices.size() - 1;
        }
      }
    }

    polymesh->verts = vertices;
    polymesh->Optimize(&PolyMesh::IsEqualVertexTC);
  }

  mapping = MAPPING_S3O;

  return true;
}

void removeDuplicatedChildPolys(MdlObject* pObj,
                                std::unordered_map<std::size_t, MdlObject**>& knownHashes) {
  for (auto& obj : pObj->childs) {
    if (obj->GetPolyMesh() == nullptr) {
      std::cout << "-- Found object without Mesh" << std::endl;
      continue;
    }

    auto* pm = obj->GetPolyMesh();
    if (pm->verts.empty()) {
      std::cout << "-- Found empty Mesh" << std::endl;
      continue;
    }

    std::size_t hash = HASH_SEED;
    // Add the position to the hash
    hash_combine<float>(hash, obj->position.x, obj->position.y, obj->position.z);

    for (auto const& vert : pm->verts) {
      // Add all vertexes with theier normals to the hash.
      hash_combine<float>(hash, vert.pos.x, vert.pos.y, vert.pos.z, vert.normal.x, vert.normal.y,
                          vert.normal.z);
    }

    const auto& phi = knownHashes.find(hash);
    if (phi != knownHashes.end()) {
      *phi->second = nullptr;

      knownHashes.erase(phi);
    }

    knownHashes.insert({hash, &obj});

    // Child of childs
    removeDuplicatedChildPolys(obj, knownHashes);
  }
}

void removeInvalidChilds(MdlObject* obj) {
  auto oit = obj->childs.begin();
  while (oit != obj->childs.end()) {
    auto& oChild = *oit;

    if (oChild == nullptr) {
      oit = obj->childs.erase(oit);
      continue;
    }

    auto* pm = oChild->GetPolyMesh();
    if (pm == nullptr) {
      oit = obj->childs.erase(oit);
      continue;
    }
    if (pm->verts.size() < 3) {
      oit = obj->childs.erase(oit);
      continue;
    }

    removeInvalidChilds(oChild);
    ++oit;
  }
}

void Model::Remove3DOBase() {
  if (root == nullptr) {
    std::cout << "Cleanup3DO() can only be run on the root object." << std::endl;
    return;
  }

  // Remove base.
  root = root->childs[0];
}

void Model::Cleanup() const {
  if (root == nullptr) {
    std::cout << "Cleanup() can only be run on the root object." << std::endl;
    return;
  }

  // The actual removeall.
  std::unordered_map<std::size_t, MdlObject**> knownHashes;
  removeDuplicatedChildPolys(root, knownHashes);
  knownHashes.clear();

  // Cleanup nullptr from the removeall
  removeInvalidChilds(root);
}

void Model::FlipUVs() const {
  auto objs = GetObjectList();

  for (auto& obj : objs) {
    for (VertexIterator vi(obj); !vi.End(); vi.Next()) {
      vi->tc[0].y = 1.0F - vi->tc[0].y;
      // vi->tc[1].y = 1.0F - vi->tc[1].y;
    }
  }
}

void Model::MirrorUVs() const {
  auto objs = GetObjectList();

  for (auto& obj : objs) {
    for (VertexIterator vi(obj); !vi.End(); vi.Next()) {
      vi->tc[0].x = 1.0F - vi->tc[0].x;
      // vi->tc[1].x = 1.0F - vi->tc[1].x;
    }
  }
}

void Model::AllLowerCaseNames() const {
  // convert to lowecase names
  auto objlist = GetObjectList();
  for (auto& obj : objlist) {
    obj->name = to_lower(obj->name);
  }
}