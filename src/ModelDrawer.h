//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#ifndef JC_MODEL_DRAWER_H
#define JC_MODEL_DRAWER_H

#include "Model.h"
#include "VertexBuffer.h"

enum RenderMethod { RM_S3OFULL, RM_S3OBASIC, RM_TEXTURE0COLOR, RM_TEXTURE1COLOR };

// Cached rendering data for objects
struct RenderData : IRenderData {
  RenderData() { drawList = 0; }
  ~RenderData();

  unsigned int drawList;

  void Invalidate();
};

class ModelDrawer {
 public:
  ModelDrawer();
  ~ModelDrawer();

  void SetRenderMethod(RenderMethod rm) { renderMethod = rm; }
  void Render(Model* mdl, IView* view, const Vector3& teamcolor);
  void RenderObject(MdlObject* o, IView* view, int mapping);
  static void RenderPolygon(MdlObject* o, Poly* pl, IView* v, int mapping, bool allowSelect);

 protected:
  void RenderSelection(IView* view);
  static void RenderPolygonVertexNormals(PolyMesh* o, Poly* pl);
  void RenderHelperGeom(MdlObject* o, IView* v);

  void RenderSmoothPolygon(PolyMesh* pm, Poly* pl);

  void RenderSelection_(MdlObject* o, IView* view);
  void SetupS3OAdvDrawing(const Vector3& teamcol, IView* v);
  void SetupS3OBasicDrawing(const Vector3& teamcol);
  static void CleanupS3OBasicDrawing();
  static void CleanupS3OAdvDrawing();
  int SetupTextureMapping(IView* view, const Vector3& teamcolor);
  int SetupS3OTextureMapping(IView* view, const Vector3& teamcolor);
  void SetupGL();

  bool glewInitialized{false};
  int canRenderS3O{false};  // 0=no S3O style rendering, 1=only texture 0, 2=both textures
  RenderMethod renderMethod{RM_S3OFULL};
  uint whiteTexture{};

  uint s3oFP;
  uint s3oVP;
  uint s3oFPunlit;
  uint s3oVPunlit;
  uint skyboxTexture{0};

  int bufferSize{0};
  Vector3* buffer{0};

  Model* model{0};  // valid while drawing

  uint sphereList{0};  // display list for rendering a sphere
};

#endif  // JC_MODEL_DRAWER_H
