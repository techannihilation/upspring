
#include "EditorIncl.h"
#include "EditorDef.h"

#include "Model.h"
#include "MeshIterators.h"
#include "CurvedSurface.h"
#include "Util.h"
// ------------------------------------------------------------------------------------------------
// Rotator
// ------------------------------------------------------------------------------------------------

Rotator::Rotator() = default;

Vector3 Rotator::GetEuler() const { /*
                                     Matrix m;
                                     q.makematrix(&m);
                                     return m.calcEulerYXZ();*/
  return euler;
}

void Rotator::SetEuler(Vector3 euler) {
  /*	Matrix m;
          m.eulerYXZ(euler);
          m.makequat(&q);
          q.normalize();*/
  this->euler = euler;
}

void Rotator::SetQuat(Quaternion q) {
  Matrix m;
  q.makematrix(&m);
  euler = m.calcEulerYXZ();
}

Quaternion Rotator::GetQuat() const {
  Matrix m;
  m.eulerYXZ(euler);
  Quaternion q;
  m.makequat(&q);
  return q;
}

void Rotator::AddEulerAbsolute(const Vector3& rot) {
  /*
  Matrix m;
  m.eulerYXZ(rot);

  Quaternion tq;
  m.makequat(&tq);

  q *= tq;
  q.normalize();*/

  euler += rot;
}

void Rotator::AddEulerRelative(const Vector3& rot) {
  /*	Matrix m;
          m.eulerYXZ(rot);

          Quaternion tq;
          m.makequat(&tq);

          q = tq * q;
          q.normalize();*/

  euler += rot;
}

void Rotator::ToMatrix(Matrix& o) const {
  //	q.makematrix(&o);
  o.eulerYXZ(euler);
}

void Rotator::FromMatrix(const Matrix& r) {
  euler = r.calcEulerYXZ();
  //	r.makequat(&q);
  //	q.normalize();
}

// ------------------------------------------------------------------------------------------------
// MdlObject
// ------------------------------------------------------------------------------------------------

// Is pos contained by this object?
float MdlObject::Selector::Score(Vector3& pos, float camdis) {
  // it it close to the center?
  Vector3 const tmp;
  Vector3 center;
  Matrix transform;
  obj->GetFullTransform(transform);
  Vector3 const empty = Vector3();
  transform.apply(&empty, &center);
  float best = (pos - center).length();
  // it it close to a polygon?
  for (PolyIterator pi(obj); !pi.End(); pi.Next()) {
    pi->selector->mesh = pi.Mesh();
    float const polyscore = pi->selector->Score(pos, camdis);
    if (polyscore < best) {
      best = polyscore;
    }
  }
  return best;
}
void MdlObject::Selector::Toggle(Vector3& /*pos*/, bool bSel) { obj->isSelected = bSel; }
bool MdlObject::Selector::IsSelected() { return obj->isSelected; }

MdlObject::MdlObject() : selector(new Selector(this)) {
  scale.set(1, 1, 1);

  InitAnimationInfo();
}

MdlObject::~MdlObject() {
  delete geometry;

  for (auto& child : childs) {
    delete child;
  }
  childs.clear();

  delete selector;
  delete csurfobj;
}

PolyMesh* MdlObject::GetPolyMesh() const { return geometry; }

PolyMesh* MdlObject::GetOrCreatePolyMesh() {
  if (geometry == nullptr) {
    delete geometry;
    geometry = new PolyMesh;
  }
  return geometry;
}

void MdlObject::InvalidateRenderData() const {
  if (geometry != nullptr) {
    geometry->InvalidateRenderData();
  }
}

void MdlObject::RemoveChild(MdlObject* o) {
  childs.erase(find(childs.begin(), childs.end(), o));
  o->parent = nullptr;
}

void MdlObject::AddChild(MdlObject* o) {
  if (o->parent != nullptr) {
    o->parent->RemoveChild(o);
  }

  childs.push_back(o);
  o->parent = this;
}

bool MdlObject::IsEmpty() {
  for (auto& child : childs) {
    if (!child->IsEmpty()) {
      return false;
    }
  }

  PolyMesh* pm = GetPolyMesh();
  return pm != nullptr ? pm->poly.empty() : true;
}

void MdlObject::GetTransform(Matrix& mat) const {
  Matrix scaling;
  scaling.scale(scale);

  Matrix rotationMatrix;
  rotation.ToMatrix(rotationMatrix);

  mat = scaling * rotationMatrix;
  mat.t(0) = position.x;
  mat.t(1) = position.y;
  mat.t(2) = position.z;
}

void MdlObject::GetFullTransform(Matrix& tr) const {
  GetTransform(tr);

  if (parent != nullptr) {
    Matrix parentTransform;
    parent->GetFullTransform(parentTransform);

    tr = parentTransform * tr;
  }
}

void MdlObject::SetPropertiesFromMatrix(Matrix& transform) {
  position.x = transform.t(0);
  position.y = transform.t(1);
  position.z = transform.t(2);

  // extract scale and create a rotation matrix
  Vector3 cx;
  Vector3 cy;
  Vector3 cz;  // columns
  transform.getcx(cx);
  transform.getcy(cy);
  transform.getcz(cz);

  scale.x = cx.length();
  scale.y = cy.length();
  scale.z = cz.length();

  Matrix rotationMatrix;
  rotationMatrix.identity();
  rotationMatrix.setcx(cx / scale.x);
  rotationMatrix.setcy(cy / scale.y);
  rotationMatrix.setcz(cz / scale.z);
  rotation.FromMatrix(rotationMatrix);
}

void MdlObject::load_3do_textures(std::shared_ptr<TextureHandler> par_texhandler) {
  if (!bTexturesLoaded) {
    for (PolyIterator p(this); !p.End(); p.Next()) {
      if (!p->texture && !p->texname.empty()) {
        p->texture = par_texhandler->texture(p->texname);
      }
    }
    bTexturesLoaded = true;
  }

  for (auto& child : childs) {
    child->load_3do_textures(par_texhandler);
  }
}

void MdlObject::FlipPolygons() { ApplyPolyMeshOperationR(&PolyMesh::FlipPolygons); }

// apply parent-space transform without modifying any transformation properties
void MdlObject::ApplyParentSpaceTransform(const Matrix& psTransform) {
  /*
  A = object transformation matrix to parent space
  T = given parent-space transform matrix
  p = original position
  p' = new position

  the new position, when transformed,
  needs to be equal to the transformed old position with scale/mirror applied to it:

  A*p' = SAp
  p' = (A^-1)SAp
  */

  Matrix transform;
  Matrix inv;

  GetTransform(transform);
  transform.inverse(inv);

  Matrix result = inv * psTransform;
  result *= transform;

  TransformVertices(result);

  // transform childs objects
  for (auto& child : childs) {
    child->ApplyParentSpaceTransform(result);
  }
}

void MdlObject::Rotate180() {
  Vector3 rot;
  rot[1] = 180 * (3.141593f / 180.0f);
  rotation.AddEulerAbsolute(rot);

  ApplyTransform(true, false, false);
}

void MdlObject::ApplyTransform(bool removeRotation, bool removeScaling, bool removePosition) {
  Matrix mat;
  mat.identity();
  if (removeScaling) {
    // child vertices have to be transformed to do mirroring properly
    if (scale.x < 0.0F || scale.y < 0.0F || scale.z < 0.0F) {
      Vector3 mirror;
      bool flip = false;
      for (int a = 0; a < 3; a++) {
        if (scale[a] < 0.0F) {
          mirror[a] = -1.0F;
          scale[a] = -scale[a];
          flip = !flip;
        } else {
          mirror[a] = 1.0F;
        }
      }
      Matrix mirrorMatrix;
      mirrorMatrix.scale(mirror);

      TransformVertices(mirrorMatrix);
      for (auto& child : childs) {
        child->ApplyParentSpaceTransform(mirrorMatrix);
      }

      if (flip) {
        FlipPolygons();
      }
    }

    Matrix scaling;
    scaling.scale(scale);
    scale.set(1, 1, 1);
    mat = scaling;
  }
  if (removeRotation) {
    Matrix rotationMatrix;
    rotation.ToMatrix(rotationMatrix);
    mat *= rotationMatrix;
    rotation = Rotator();
  }

  if (removePosition) {
    mat.t(0) = position.x;
    mat.t(1) = position.y;
    mat.t(2) = position.z;
    position = Vector3();
  }
  Transform(mat);
}

void MdlObject::NormalizeNormals() {
  for (VertexIterator v(this); !v.End(); v.Next()) {
    v->normal.normalize();
  }

  for (auto& child : childs) {
    child->NormalizeNormals();
  }

  InvalidateRenderData();
}

void MdlObject::TransformVertices(const Matrix& transform) const {
  if (geometry != nullptr) {
    geometry->Transform(transform);
  }
  InvalidateRenderData();
}

void MdlObject::Transform(const Matrix& transform) {
  TransformVertices(transform);

  for (auto& child : childs) {
    Matrix subObjTr;
    child->GetTransform(subObjTr);
    subObjTr *= transform;
    child->SetPropertiesFromMatrix(subObjTr);
  }
}

MdlObject* MdlObject::Clone() {
  auto* cp = new MdlObject;

  if (geometry != nullptr) {
    cp->geometry = geometry->Clone();
  }

  for (auto& child : childs) {
    MdlObject* ch = child->Clone();
    cp->childs.push_back(ch);
    ch->parent = cp;
  }

  cp->position = position;
  cp->rotation = rotation;
  cp->scale = scale;
  cp->name = name;
  cp->isSelected = isSelected;

  // clone animInfo
  animInfo.CopyTo(cp->animInfo);

  return cp;
}

bool MdlObject::HasSelectedParent() const {
  MdlObject* c = parent;
  while (c != nullptr) {
    if (c->isSelected) {
      return true;
    }
    c = c->parent;
  }
  return false;
}

void MdlObject::ApproximateOffset() {
  Vector3 mid;
  int c = 0;
  for (VertexIterator v(this); !v.End(); v.Next()) {
    mid += v->pos;
    c++;
  }
  if (c != 0) {
    mid /= static_cast<float>(c);
  }

  position += mid;
  for (VertexIterator v(this); !v.End(); v.Next()) {
    v->pos -= mid;
  }
}

void MdlObject::UnlinkFromParent() {
  if (parent != nullptr) {
    parent->childs.erase(find(parent->childs.begin(), parent->childs.end(), this));
    parent = nullptr;
  }
}

void MdlObject::LinkToParent(MdlObject* p) {
  if (parent != nullptr) {
    UnlinkFromParent();
  }
  p->childs.push_back(this);
  parent = p;
}

std::vector<MdlObject*> MdlObject::GetChildObjects() {
  std::vector<MdlObject*> objects;

  for (auto& child : childs) {
    if (!child->childs.empty()) {
      std::vector<MdlObject*> sub = child->GetChildObjects();
      objects.insert(objects.end(), sub.begin(), sub.end());
    }
    objects.push_back(child);
  }

  return objects;
}

void MdlObject::FullMerge() {
  std::vector<MdlObject*> ch = childs;
  for (auto& a : ch) {
    a->FullMerge();
    MergeChild(a);
  }
}

void MdlObject::MergeChild(MdlObject* ch) {
  ch->ApplyTransform(true, true, true);
  PolyMesh* pm = ch->GetPolyMesh();
  if (pm != nullptr) {
    pm->MoveGeometry(GetOrCreatePolyMesh());
  }

  // move the childs
  for (auto& child : ch->childs) {
    child->parent = this;
  }
  childs.insert(childs.end(), ch->childs.begin(), ch->childs.end());
  ch->childs.clear();

  // delete the child
  childs.erase(find(childs.begin(), childs.end(), ch));
  delete ch;
}

void MdlObject::MoveOrigin(Vector3 move) {
  // Calculate inverse
  Matrix tr;
  GetTransform(tr);

  Matrix inv;
  if (tr.inverse(inv))  // Origin-move only works for objects with inverse transform (scale can't
                        // contain zero)
  {
    // move the object
    position += move;

    Matrix mat;
    mat.translation(-move);
    mat = tr * mat;
    mat *= inv;

    Transform(mat);
  }
}

void MdlObject::UpdateAnimation(float time) {
  animInfo.Evaluate(this, time);

  for (auto& child : childs) {
    child->UpdateAnimation(time);
  }
}

void MdlObject::InitAnimationInfo() {
  /*
  animInfo.AddProperty(
          AnimController::GetQuaternionController(),
          "rotation",
          (long int) &((MdlObject*) this)->rotation.q);
  */
  /*
  animInfo.AddProperty(
          AnimController::GetStructController(
                  AnimController::GetEulerAngleController(),
                  Vector3::StaticClass()
          ),
          "rotation",
          (long int) &((MdlObject*) this)->rotation
  );
  */
}