#include "EditorDef.h"
#include "EditorIncl.h"
#include "Model.h"
#include "Util.h"

// ------------------------------------------------------------------------------------------------
// Polygon
// ------------------------------------------------------------------------------------------------

float Poly::Selector::Score(Vector3& pos, float /*camdis*/) {
  assert(mesh);
  const std::vector<Vertex>& v = mesh->verts;
  Plane plane;

  Vector3 vrt[3];
  for (std::uint32_t a = 0; a < 3; a++) {
    transform.apply(&v[poly->verts[a]].pos, &vrt[a]);
  }

  plane.MakePlane(vrt[0], vrt[1], vrt[2]);
  float const dis = plane.Dis(&pos);
  return fabs(dis);
}
void Poly::Selector::Toggle(Vector3& /*pos*/, bool bSel) { poly->isSelected = bSel; }
bool Poly::Selector::IsSelected() { return poly->isSelected; }

Poly::Poly() : selector(new Selector(this)) {
  texture = nullptr;
  color.set(1, 1, 1);
}

Poly::~Poly() { SAFE_DELETE(selector); }

Poly* Poly::Clone() const {
  Poly* pl = new Poly;
  pl->verts = verts;
  pl->texname = texname;
  pl->color = color;
  pl->taColor = taColor;
  pl->texture = texture;
  pl->isCurved = isCurved;
  return pl;
}

void Poly::Flip() {
  std::vector<int> nv;
  nv.resize(verts.size());
  for (std::uint32_t a = 0; a < verts.size(); a++) {
    nv[verts.size() - a - 1] = verts[(a + 2) % verts.size()];
  }
  verts = nv;
}

Plane Poly::CalcPlane(const std::vector<Vertex>& vrt) {
  Plane plane;
  plane.MakePlane(vrt[verts[0]].pos, vrt[verts[1]].pos, vrt[verts[2]].pos);
  return plane;
}

void Poly::RotateVerts() {
  std::vector<int> n(verts.size());
  for (std::uint32_t a = 0; a < verts.size(); a++) {
    n[a] = verts[(a + 1) % n.size()];
  }
  verts = n;
}

// ------------------------------------------------------------------------------------------------
// PolyMesh
// ------------------------------------------------------------------------------------------------

PolyMesh::~PolyMesh() {
  for (auto& a : poly) {
    delete a;
  }
  poly.clear();
}

// Special case... polymesh drawing is done in the ModelDrawer
void PolyMesh::Draw(ModelDrawer* /*drawer*/, Model* /*mdl*/, MdlObject* /*o*/) {}

PolyMesh* PolyMesh::ToPolyMesh() { return dynamic_cast<PolyMesh*>(Clone()); }

Geometry* PolyMesh::Clone() {
  auto* cp = new PolyMesh;

  cp->verts = verts;

  for (auto& a : poly) {
    cp->poly.push_back(a->Clone());
  }

  return cp;
}

void PolyMesh::Transform(const Matrix& transform) {
  Matrix normalTransform;
  Matrix invTransform;
  transform.inverse(invTransform);
  invTransform.transpose(&normalTransform);

  // transform and add the child vertices to the parent vertices list
  for (auto& v : verts) {
    Vector3 tpos;
    Vector3 tnormal;

    transform.apply(&v.pos, &tpos);
    v.pos = tpos;
    normalTransform.apply(&v.normal, &tnormal);
    v.normal = tnormal;
  }
}

bool PolyMesh::IsEqualVertexTC(Vertex& a, Vertex& b) {
  return a.pos.epsilon_compare(&b.pos, 0.001F) && a.tc[0].x == b.tc[0].x && a.tc[0].y == b.tc[0].y;
}

bool PolyMesh::IsEqualVertexTCNormal(Vertex& a, Vertex& b) {
  return a.pos.epsilon_compare(&b.pos, 0.001F) && a.tc[0].x == b.tc[0].x &&
         a.tc[0].y == b.tc[0].y && a.normal.epsilon_compare(&b.normal, 0.01F);
}

void PolyMesh::OptimizeVertices(PolyMesh::IsEqualVertexCB cb) {
  std::vector<int> old2new;
  std::vector<int> usage;
  std::vector<Vertex> nv;

  if (verts.empty()) {
    spdlog::error("OptimizeVertices: found a PolyMesh with no verts");
    return;
  }

  old2new.resize(verts.size());
  usage.resize(verts.size());
  fill(usage.begin(), usage.end(), 0);

  for (auto* pl : poly) {
    for (int const vert : pl->verts) {
      usage[vert]++;
    }
  }

  for (std::uint32_t a = 0; a < verts.size(); a++) {
    bool matched = false;

    if (usage[a] == 0) {
      continue;
    }

    for (uint b = 0; b < nv.size(); b++) {
      Vertex* va = &verts[a];
      Vertex* vb = &nv[b];

      if (cb(*va, *vb)) {
        matched = true;
        old2new[a] = b;
        break;
      }
    }

    if (!matched) {
      old2new[a] = nv.size();
      nv.push_back(verts[a]);
    }
  }

  verts = nv;

  // map the poly vertex-indices to the new set of vertices
  for (auto* pl : poly) {
    for (int& vert : pl->verts) {
      vert = old2new[vert];
    }
  }
}

void PolyMesh::Optimize(PolyMesh::IsEqualVertexCB cb) {
  OptimizeVertices(cb);

  // remove double linked vertices
  std::vector<Poly*> npl;
  for (auto* pl : poly) {
    bool finished = false;
    do {
      finished = true;
      for (uint i = 0, j = static_cast<int>(pl->verts.size()) - 1; i < pl->verts.size(); j = i++) {
        if (pl->verts[i] == pl->verts[j]) {
          pl->verts.erase(pl->verts.begin() + i);
          finished = false;
          break;
        }
      }
    } while (!finished);

    if (pl->verts.size() >= 3) {
      npl.push_back(pl);
    } else {
      delete pl;
    }
  }
  poly = npl;
}

void PolyMesh::CalculateRadius(float& radius, const Matrix& tr, const Vector3& mid) {
  for (auto& vert : verts) {
    Vector3 tpos;
    tr.apply(&vert.pos, &tpos);
    float const r = (tpos - mid).length();
    if (radius < r) {
      radius = r;
    }
  }
}

std::vector<Triangle> PolyMesh::MakeTris() {
  std::vector<Triangle> tris;

  for (auto* p : poly) {
    for (uint b = 2; b < p->verts.size(); b++) {
      Triangle t;

      t.vrt[0] = p->verts[0];
      t.vrt[1] = p->verts[b - 1];
      t.vrt[2] = p->verts[b];

      tris.push_back(t);
    }
  }
  return tris;
}

void GenerateUniqueVectors(const std::vector<Vertex>& verts, std::vector<Vector3>& vertPos,
                           std::vector<int>& old2new) {
  old2new.resize(verts.size());

  for (std::uint32_t a = 0; a < verts.size(); a++) {
    bool matched = false;

    for (uint b = 0; b < vertPos.size(); b++) {
      if (vertPos[b] == verts[a].pos) {
        old2new[a] = b;
        matched = true;
        break;
      }
    }
    if (!matched) {
      old2new[a] = vertPos.size();
      vertPos.push_back(verts[a].pos);
    }
  }
}

struct FaceVert {
  std::vector<int> adjacentFaces;
};

void PolyMesh::CalculateNormals2(float maxSmoothAngle) {
  float const ang_c = cosf(M_PI * maxSmoothAngle / 180.0F);
  std::vector<Vector3> vertPos;
  std::vector<int> old2new;
  GenerateUniqueVectors(verts, vertPos, old2new);

  std::vector<std::vector<int>> new2old;
  new2old.resize(vertPos.size());
  for (std::uint32_t a = 0; a < old2new.size(); a++) {
    new2old[old2new[a]].push_back(a);
  }

  // Calculate planes
  std::vector<Plane> polyPlanes;
  polyPlanes.resize(poly.size());
  for (std::uint32_t a = 0; a < poly.size(); a++) {
    polyPlanes[a] = poly[a]->CalcPlane(verts);
  }

  // Determine which faces are using which unique vertex
  std::vector<FaceVert> faceVerts;  // one per unique vertex
  faceVerts.resize(old2new.size());

  for (std::uint32_t a = 0; a < poly.size(); a++) {
    Poly* pl = poly[a];
    for (int const vert : pl->verts) {
      faceVerts[old2new[vert]].adjacentFaces.push_back(a);
    }
  }

  // Calculate normals
  int cnorm = 0;
  for (auto& a : poly) {
    cnorm += a->verts.size();
  }
  std::vector<Vector3> normals;
  normals.resize(cnorm);

  cnorm = 0;
  for (std::uint32_t a = 0; a < poly.size(); a++) {
    Poly* pl = poly[a];
    for (int const vert : pl->verts) {
      FaceVert const& fv = faceVerts[old2new[vert]];
      std::vector<Vector3> vnormals;
      vnormals.push_back(polyPlanes[a].GetVector());
      for (int const adjacentFace : fv.adjacentFaces) {
        // Same poly?
        if (adjacentFace == static_cast<int>(a)) {
          continue;
        }

        Plane adjPlane = polyPlanes[adjacentFace];

        // Spring 3DO style smoothing
        float const dot = adjPlane.GetVector().dot(polyPlanes[a].GetVector());
        //	logger.Print("Dot: %f\n",dot);
        if (dot < ang_c) {
          continue;
        }

        // see if the normal is unique for this vertex
        bool isUnique = true;
        for (auto& vnormal : vnormals) {
          if (vnormal == adjPlane.GetVector()) {
            isUnique = false;
            break;
          }
        }

        if (isUnique) {
          vnormals.push_back(adjPlane.GetVector());
        }
      }
      Vector3 normal;
      for (auto& vnormal : vnormals) {
        normal += vnormal;
      }

      if (normal.length() > 0.0F) {
        normal.normalize();
      }
      normals[cnorm++] = normal;
    }
  }

  // Create a new set of vertices with the calculated normals
  std::vector<Vertex> newVertices;
  newVertices.reserve(poly.size() * 4);  // approximate
  cnorm = 0;
  for (auto* pl : poly) {
    for (int const vert : pl->verts) {
      Vertex nv = verts[vert];
      nv.normal = normals[cnorm++];
      newVertices.push_back(nv);
    }
  }

  // Optimize
  verts = newVertices;
  Optimize(&PolyMesh::IsEqualVertexTCNormal);
}

// In short, the reason for the complexity of this function is:
//  - creates a list of vertices where every vertex has a unique position (UV ignored)
//  - doesn't allow the same poly normal to be added to the same vertex twice
void PolyMesh::CalculateNormals() {
  std::vector<Vector3> vertPos;
  std::vector<int> old2new;
  GenerateUniqueVectors(verts, vertPos, old2new);

  std::vector<std::vector<int>> new2old;
  new2old.resize(vertPos.size());
  for (std::uint32_t a = 0; a < old2new.size(); a++) {
    new2old[old2new[a]].push_back(a);
  }

  std::vector<std::vector<Vector3>> normals;
  normals.resize(vertPos.size());

  for (auto* pl : poly) {
    Plane plane;

    plane.MakePlane(vertPos[old2new[pl->verts[0]]], vertPos[old2new[pl->verts[1]]],
                    vertPos[old2new[pl->verts[2]]]);

    Vector3 const plnorm = plane.GetVector();
    for (int const vert : pl->verts) {
      std::vector<Vector3>& norms = normals[old2new[vert]];
      uint c = 0;
      for (c = 0; c < norms.size(); c++) {
        if (norms[c] == plnorm) {
          break;
        }
      }

      if (c == norms.size()) {
        norms.push_back(plnorm);
      }
    }
  }

  for (std::uint32_t a = 0; a < normals.size(); a++) {
    Vector3 sum;
    std::vector<Vector3> const& vn = normals[a];
    for (const auto& b : vn) {
      sum += b;
    }

    if (sum.length() > 0.0F) {
      sum.normalize();
    }

    std::vector<int> const& vlist = new2old[a];
    for (int const b : vlist) {
      verts[b].normal = sum;
    }
  }
}

void PolyMesh::FlipPolygons() {
  for (auto& a : poly) {
    a->Flip();
  }
}

void PolyMesh::MoveGeometry(PolyMesh* dst) {
  // offset the vertex indices and move polygons
  for (auto* pl : poly) {
    for (int& vert : pl->verts) {
      vert += static_cast<int>(dst->verts.size());
    }
  }
  dst->poly.insert(dst->poly.end(), poly.begin(), poly.end());
  poly.clear();

  // insert the child vertices
  dst->verts.insert(dst->verts.end(), verts.begin(), verts.end());
  verts.clear();

  InvalidateRenderData();
}
