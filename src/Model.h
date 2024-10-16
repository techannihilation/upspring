//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#pragma once

#include "Animation.h"
#include "IView.h"
#include "Referenced.h"
#include "Texture.h"
#include "VertexBuffer.h"
#include "math/Mathlib.h"
#include "Atlas/atlas.hpp"

#include <memory>

#include "spdlog/spdlog.h"

#define MAPPING_S3O 0
#define MAPPING_3DO 1

namespace csurf {
class Face;
class Object;
};  // namespace csurf

class Texture;

struct Poly;
struct Vertex;
struct MdlObject;
struct Model;
struct IKinfo;
class PolyMesh;

uint Vector3ToRGB(Vector3 v);

struct Triangle {
  Triangle() { vrt[0] = vrt[1] = vrt[2] = 0; }
  int vrt[3];
};

struct Vertex {
  Vector3 pos, normal;
  Vector2 tc[1];
};

struct Poly {
  Poly();
  ~Poly();

  Plane CalcPlane(const std::vector<Vertex>& vrt);
  void Flip();
  Poly* Clone() const;
  void RotateVerts();

  std::vector<int> verts;
  std::string texname;
  Vector3 color;
  int taColor{-1};  // TA indexed color
  std::shared_ptr<Texture> texture;

  bool isCurved{false};  // this polygon should get a curved surface at the next csurf update
  bool isSelected{false};

#ifndef SWIG
  struct Selector : ViewSelector {
    Selector(Poly* poly) : poly(poly), mesh(0) {}
    float Score(Vector3& pos, float camdis);
    void Toggle(Vector3& pos, bool bSel);
    bool IsSelected();
    Poly* poly;
    Matrix transform;
    PolyMesh* mesh;
  };

  Selector* selector;
#endif
};

// Inverse Kinematics joint types - these use the same naming as ODE
enum IKJointType {
  IKJT_Fixed = 0,
  IKJT_Hinge = 1,      // rotation around an axis
  IKJT_Universal = 2,  // 2 axis
};

struct BaseJoint {
  virtual ~BaseJoint() {}
};

struct HingeJoint : public BaseJoint {
  Vector3 axis;
};

struct UniversalJoint : public BaseJoint {
  Vector3 axis[2];
};

struct IKinfo {
  IKinfo();
  ~IKinfo();

  IKJointType jointType;
  BaseJoint* joint{};
};

struct IRenderData {
  virtual ~IRenderData() {}
  virtual void Invalidate() = 0;
};

class Rotator {
 public:
  Rotator();
  void AddEulerAbsolute(const Vector3& rot);
  void AddEulerRelative(const Vector3& rot);
  Vector3 GetEuler() const;
  void SetEuler(Vector3 euler);
  void ToMatrix(Matrix& o) const;
  void FromMatrix(const Matrix& r);
  Quaternion GetQuat() const;
  void SetQuat(Quaternion q);

  Vector3 euler;
  //	Quaternion q;
  bool eulerInterp{};  // Use euler angles to interpolate
};

class PolyMesh;
class ModelDrawer;

// class Geometry {
//  public:
//   virtual ~Geometry() {}

//   virtual void Draw(ModelDrawer* drawer, Model* mdl, MdlObject* o) = 0;
//   virtual Geometry* Clone() = 0;
//   virtual void Transform(const Matrix& transform) = 0;
//   virtual PolyMesh* ToPolyMesh() = 0;
//   virtual void InvalidateRenderData() {}

//   virtual void CalculateRadius(float& radius, const Matrix& tr, const Vector3& mid) = 0;
// };

class PolyMesh {
 public:
  ~PolyMesh();

  std::vector<Vertex> verts;
  std::vector<Poly*> poly;

  void Draw(ModelDrawer* drawer, Model* mdl, MdlObject* o);
  PolyMesh* Clone();
  void Transform(const Matrix& transform);
  PolyMesh* ToPolyMesh();  // clones
  std::vector<Triangle> MakeTris();

  static bool IsEqualVertexTC(Vertex& a, Vertex& b);
  static bool IsEqualVertexTCNormal(Vertex& a, Vertex& b);
  typedef bool (*IsEqualVertexCB)(Vertex& a, Vertex& b);

  void OptimizeVertices(IsEqualVertexCB cb);
  void Optimize(IsEqualVertexCB cb);

  void InvalidateRenderData() {}

  void MoveGeometry(PolyMesh* dst);
  void FlipPolygons();  // flip polygons of object and child objects
  void CalculateRadius(float& radius, const Matrix& tr, const Vector3& mid);
  void CalculateNormals();
  void CalculateNormals2(float maxSmoothAngle);
};

struct MdlObject {
 public:
  MdlObject();
  virtual ~MdlObject();

  bool IsEmpty();
  void MergeChild(MdlObject* ch);
  void FullMerge();                         // merge all childs and their subchilds
  void GetTransform(Matrix& mat) const;     // calculates the object space -> parent space transform
  void GetFullTransform(Matrix& tr) const;  // object space -> world space
  std::vector<MdlObject*> GetChildObjects();  // returns all child objects (recursively)

  void UnlinkFromParent();
  void LinkToParent(MdlObject* p);

  template <typename Fn>
  void ApplyPolyMeshOperationR(Fn f) {
    PolyMesh* pm = GetPolyMesh();
    if (pm) (pm->*f)();
    for (std::uint32_t a = 0; a < childs.size(); a++) childs[a]->ApplyPolyMeshOperationR(f);
  }

  void FlipPolygons();

  void load_3do_textures(std::shared_ptr<TextureHandler> par_texhandler);

  bool HasSelectedParent() const;

  void Rotate180();
  void ApplyTransform(bool rotation, bool scaling, bool position);
  void ApplyParentSpaceTransform(const Matrix& transform);
  void TransformVertices(const Matrix& transform) const;
  void ApproximateOffset();
  void SetPropertiesFromMatrix(Matrix& transform);
  // Apply transform to contents of object,
  // does not touch the properties such as position/scale/rotation
  void Transform(const Matrix& transform);

  void InvalidateRenderData() const;
  void NormalizeNormals();

  void MoveOrigin(Vector3 move);

  // Simple script util functions, useful for other code as well
  void AddChild(MdlObject* o);
  void RemoveChild(MdlObject* o);

  PolyMesh* GetPolyMesh() const;
  PolyMesh* ToPolyMesh() {
    return geometry ? geometry->ToPolyMesh() : 0;
  }  // returns a new PolyMesh
  PolyMesh* GetOrCreatePolyMesh();

  virtual void InitAnimationInfo();
  void UpdateAnimation(float time);

  MdlObject* Clone();

  // Orientation
  Vector3 position;
  Rotator rotation;
  Vector3 scale;

  PolyMesh* geometry{};

  AnimationInfo animInfo;

  std::string name;
  bool isSelected{};
  bool isOpen{};  // childs visible in object browser
  IKinfo ikInfo;

  MdlObject* parent{};
  std::vector<MdlObject*> childs;

#ifndef SWIG
  csurf::Object* csurfobj{};

  struct Selector : ViewSelector {
    Selector(MdlObject* obj) : obj(obj) {}
    // Is pos contained by this object?
    float Score(Vector3& pos, float camdis);
    void Toggle(Vector3& pos, bool bSel);
    bool IsSelected();
    MdlObject* obj;
  };
  Selector* selector;
  bool bTexturesLoaded{};
#endif
};

static inline void IterateObjects(MdlObject* obj, void (*fn)(MdlObject* obj)) {
  fn(obj);
  for (unsigned int a = 0; a < obj->childs.size(); a++) IterateObjects(obj->childs[a], fn);
}

// allows a GUI component to plug in and show the progress
struct IProgressCtl {
  IProgressCtl() {
    data = 0;
    cb = 0;
  }
  virtual void Update(float v) {
    if (cb) cb(v, data);
  }
  void (*cb)(float part, void* data);
  void* data;
};

struct TextureBinding {
  std::string name;
  std::shared_ptr<Texture> texture;

  void SetTexture(std::shared_ptr<Texture> par_tex) { texture = par_tex; }
  std::shared_ptr<Texture>& GetTexture() { return texture; }
};

// NOTE: g++ disallows references to temporary objects, so...
static IProgressCtl defprogctl;

struct Model {
  std::string file_;

  Model();
  ~Model();

  const std::string& file() {
    return file_;
  }

  void PostLoad();

  bool Load3DO(const char* filename, IProgressCtl& progctl = defprogctl);
  bool Save3DO(const char* fn, IProgressCtl& progctl = defprogctl) const;

  bool LoadS3O(const char* filename, IProgressCtl& progctl = defprogctl);
  bool SaveS3O(const char* filename, IProgressCtl& progctl = defprogctl);

  static Model* Load(const std::string& fn, bool Optimize = true,
                     IProgressCtl& progctl = defprogctl);
  static bool Save(Model* mdl, const std::string& fn, IProgressCtl& progctl = defprogctl);

  // exports merged version of the model
  bool ExportUVMesh(const char* fn) const;

  // copies the UV coords of the single piece exported by ExportUVMesh back to the model
  bool ImportUVMesh(const char* fn, IProgressCtl& progctl = defprogctl) const;
  bool ImportUVCoords(Model* other, IProgressCtl& progctl = defprogctl) const;

  static void InsertModel(MdlObject* obj, Model* sub);
  std::vector<MdlObject*> GetSelectedObjects() const;
  std::vector<MdlObject*> GetObjectList() const;  // returns all objects
  std::vector<PolyMesh*> GetPolyMeshList() const;
  void DeleteObject(MdlObject* obj);
  void ReplaceObject(MdlObject* oldObj, MdlObject* _new);
  void EstimateMidPosition();
  void CalculateRadius();
  void SwapObjects(MdlObject* a, MdlObject* b);
  Model* Clone() const;

  unsigned long ObjectSelectionHash() const;

  void SetTextureName(uint index, const char* name);
  void SetTexture(uint index, std::shared_ptr<Texture> par_tex);

  bool ConvertToS3O(std::string textureName, int texw, int texh);

  bool add_textures_to_atlas(atlas& par_atlas) const;
  bool convert_to_atlas_s3o(const atlas& par_atlas);

  void Remove3DOBase();
  void Triangleize();
  void Cleanup() const;
  void FlipUVs() const;
  void MirrorUVs() const;
  void AllLowerCaseNames() const;

  float radius{};  // radius of collision sphere
  float height;    // height of whole object
  Vector3 mid;  // these give the offset from origin(which is supposed to lay in the ground plane)
                // to the middle of the unit collision sphere

  bool HasTex(uint i) { return i < texBindings.size() && texBindings[i].texture; }
  uint TextureID(uint i) { return texBindings[i].texture->glIdent; }
  std::string& TextureName(uint i) { return texBindings[i].name; }

  std::vector<TextureBinding> texBindings;
  int mapping{};

  MdlObject* root{};

  Model(const Model& rhs) = delete;
  void operator=(const Model& rhs) = delete;

  void load_3do_textures(std::shared_ptr<TextureHandler> par_texture_handler) {
    texture_handler_ = par_texture_handler;

    root->load_3do_textures(par_texture_handler);
  }

 private:
  std::shared_ptr<TextureHandler> texture_handler_;
};

MdlObject* Load3DSObject(const char* fn, IProgressCtl& progctl);
bool Save3DSObject(const char* fn, MdlObject* obj, IProgressCtl& progctl);
MdlObject* LoadWavefrontObject(const char* fn, IProgressCtl& progctl);
bool SaveWavefrontObject(const char* fn, MdlObject* src);

void GenerateUniqueVectors(const std::vector<Vertex>& verts, std::vector<Vector3>& vertPos,
                           std::vector<int>& old2new);
