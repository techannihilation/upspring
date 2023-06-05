//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------
#include "nv_dds.h"

#include "EditorIncl.h"
#include "EditorDef.h"

#include "Util.h"
#include "View.h"
#include "Texture.h"
#include "ModelDrawer.h"
#include "CurvedSurface.h"
#include "config.h"
#include "string_util.h"

#include <GL/glew.h>
#include <GL/gl.h>

#include "spdlog/spdlog.h"

#include "Spline.h"
#include "MeshIterators.h"

static const float ObjCenterSize = 4.0F;

uint LoadVertexProgram(std::string fn);
uint LoadFragmentProgram(std::string fn);

void TestGLError() {
  GLenum const err = glGetError();
  if (err != GL_NO_ERROR) {
    spdlog::error("GL Error: {}", reinterpret_cast<const char*>(gluErrorString(err)));
  }
}

void RenderData::Invalidate() {
  if (drawList != 0) {
    glDeleteLists(drawList, 1);
  }
  drawList = 0;
}

RenderData::~RenderData() { Invalidate(); }

ModelDrawer::ModelDrawer() {
  s3oFP = s3oVP = 0;

  s3oFPunlit = s3oVPunlit = 0;
}

ModelDrawer::~ModelDrawer() {
  if (s3oFP != 0U) {
    glDeleteProgramsARB(1, &s3oFP);
  }
  if (s3oVP != 0U) {
    glDeleteProgramsARB(1, &s3oVP);
  }
  if (s3oFPunlit != 0U) {
    glDeleteProgramsARB(1, &s3oFPunlit);
  }
  if (s3oVPunlit != 0U) {
    glDeleteProgramsARB(1, &s3oVPunlit);
  }

  if (skyboxTexture != 0U) {
    glDeleteTextures(1, &skyboxTexture);
  }

  delete[] buffer;

  if (sphereList != 0U) {
    glDeleteLists(sphereList, 1);
  }
}

void ModelDrawer::SetupGL() {
  glewInitialized = true;

  GLenum err = 0;
  if ((err = glewInit()) != GLEW_OK) {
    spdlog::error("Failed to initialize GLEW: {}",
                  reinterpret_cast<const char*>(glewGetErrorString(err)));
  } else {
    canRenderS3O = 0;
    // see if S3O's can be rendered properly
    if (GLEW_ARB_multitexture && GLEW_ARB_texture_env_combine) {
      canRenderS3O = 1;
    } else {
      spdlog::warn("Basic S3O rendering is not possible with this graphics card");
    }

    if (GLEW_ARB_fragment_program && GLEW_ARB_vertex_program && GLEW_ARB_texture_cube_map) {
      s3oFP = LoadFragmentProgram(
          (ups::config::get().app_path() / "data" / "shaders" / "s3o.fp").string());
      s3oVP = LoadVertexProgram(
          (ups::config::get().app_path() / "data" / "shaders" / "s3o.vp").string());
      s3oFPunlit = LoadFragmentProgram(
          (ups::config::get().app_path() / "data" / "shaders" / "s3o_unlit.fp").string());
      s3oVPunlit = LoadVertexProgram(
          (ups::config::get().app_path() / "data" / "shaders" / "s3o_unlit.vp").string());

      if ((s3oFP != 0U) && (s3oVP != 0U) && (s3oFPunlit != 0U) && (s3oVPunlit != 0U)) {
        canRenderS3O = 2;

        nv_dds::CDDSImage image;
        if (!image.load(
                (ups::config::get().app_path() / "data" / "textures" / "skybox.dds").string())) {
          spdlog::warn("Failed to load textures/skybox.dds");

          canRenderS3O = 1;
        }

        glGenTextures(1, &skyboxTexture);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, skyboxTexture);

        if (!image.upload_textureCubemap()) {
          glDeleteTextures(1, &skyboxTexture);
          skyboxTexture = 0;
          fltk::message("Can't upload cubemap texture skybox.dds");
          canRenderS3O = 1;
        }
      } else {
        canRenderS3O = 1;
      }
    } else {
      spdlog::warn(
          "Full S3O rendering is not possible with this graphics card, \nself-illumination and "
          "reflection won't be visible");
    }
  }

  GLUquadricObj* sphere = gluNewQuadric();
  gluQuadricNormals(sphere, GLU_NONE);

  sphereList = glGenLists(1);
  glNewList(sphereList, GL_COMPILE);
  gluSphere(sphere, 1.0F, 16, 8);
  glEndList();

  gluDeleteQuadric(sphere);

  glGenTextures(1, &whiteTexture);
  glBindTexture(GL_TEXTURE_2D, whiteTexture);
  float pixel = 1.0F;
  glTexImage2D(GL_TEXTURE_2D, 0, 3, 1, 1, 0, GL_LUMINANCE, GL_FLOAT, &pixel);
}

void ModelDrawer::RenderPolygon(MdlObject* o, Poly* pl, IView* v, int mapping, bool allowSelect) {
  PolyMesh* pm = o->GetPolyMesh();  // since there are polygons, we can assume there is a polymesh
  if (allowSelect) {
    pl->selector->mesh = pm;
    o->GetFullTransform(pl->selector->transform);
    v->PushSelector(pl->selector);
  }

  if (mapping == MAPPING_3DO) {
    int texture = 0;

    if (pl->texture && v->GetRenderMode() >= M3D_TEX) {
      if (pl->verts.size() == 3 || pl->verts.size() == 4) {
        texture = pl->texture->glIdent;
      }
    }

    if (!pl->texname.empty()) {
      if (texture != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
      }
    } else {
      if (v->GetRenderMode() >= M3D_SOLID) {
        glColor3fv(reinterpret_cast<float*>(&pl->color));
      }
    }

    glBegin(GL_POLYGON);
    if (pl->verts.size() == 4 || pl->verts.size() == 3) {
      const float tc[] = {0.0F, 1.0F, 1.0F, 1.0F, 1.0F, 0.0F, 0.0F, 0.0F};
      for (std::uint32_t a = 0; a < pl->verts.size(); a++) {
        glTexCoord2f(tc[a * 2], tc[a * 2 + 1]);
        glNormal3fv(reinterpret_cast<float*>(&pm->verts[pl->verts[a]].normal));
        glVertex3fv(reinterpret_cast<float*>(&pm->verts[pl->verts[a]].pos));
      }
    } else {
      for (int const i : pl->verts) {
        glNormal3fv(reinterpret_cast<float*>(&pm->verts[i].normal));
        glVertex3fv(reinterpret_cast<float*>(&pm->verts[i].pos));
      }
    }
    glEnd();

    if (texture != 0) {
      glDisable(GL_TEXTURE_2D);
    }

    glColor3ub(255, 255, 255);
  } else {
    glBegin(GL_POLYGON);
    for (int const i : pl->verts) {
      glTexCoord2fv(reinterpret_cast<float*>(&pm->verts[i].tc[0]));
      glNormal3fv(reinterpret_cast<float*>(&pm->verts[i].normal));
      glVertex3fv(reinterpret_cast<float*>(&pm->verts[i].pos));
    }
    glEnd();
  }

  if (allowSelect) {
    v->PopSelector();
  }
}

void ModelDrawer::RenderObject(MdlObject* o, IView* v, int mapping) {
  // setup selector
  v->PushSelector(o->selector);

  // setup object transformation
  Matrix tmp;
  Matrix mat;
  o->GetTransform(mat);
  mat.transpose(&tmp);
  glPushMatrix();
  glMultMatrixf(reinterpret_cast<float*>(&tmp));

  // render object origin
  if (v->GetConfig(CFG_OBJCENTERS) != 0.0F) {
    glPointSize(ObjCenterSize);
    if (o->isSelected) {
      glColor3ub(255, 0, 0);
    } else {
      glColor3ub(255, 255, 255);
    }
    glBegin(GL_POINTS);
    glVertex3i(0, 0, 0);
    glEnd();
    glPointSize(1);
    glColor3ub(255, 255, 255);
  }

  bool const polySelect = v->GetConfig(CFG_POLYSELECT) != 0;

  //	if(polySelect) {
  // render polygons
  PolyMesh* pm = o->GetPolyMesh();
  if (pm != nullptr) {
    for (auto& a : pm->poly) {
      RenderPolygon(o, a, v, mapping, polySelect);
    }
  } else if (o->geometry != nullptr) {
    o->geometry->Draw(this, model, o);
  }

  /*	}
          else
          {
                  // render geometry using drawing list
                  if (!o->renderData)
                          o->renderData = new RenderData;

                  RenderData* rd = (RenderData*)o->renderData;

                  if (rd->drawList)
                          glCallList(rd->drawList);
                  else {
                          rd->drawList = glGenLists(1);
                          glNewList(rd->drawList, GL_COMPILE_AND_EXECUTE);

                          // render polygons
                          for (std::uint32_t a=0;a<o->poly.size();a++)
                                  RenderPolygon (o, o->poly[a], v,mapping, polySelect);

                          glEndList ();
                  }
          }*/

  for (auto& child : o->childs) {
    RenderObject(child, v, mapping);
  }

  glPopMatrix();
  v->PopSelector();
}

int ModelDrawer::SetupS3OTextureMapping(IView* v, const Vector3& teamColor) {
  int S3ORendering = 0;

  switch (renderMethod) {
    case RM_S3OFULL:
    case RM_S3OBASIC:
      if (renderMethod == RM_S3OFULL && canRenderS3O == 2 && model->HasTex(0) && model->HasTex(1)) {
        S3ORendering = 2;
        SetupS3OAdvDrawing(teamColor, v);
      } else if (canRenderS3O >= 1 && model->HasTex(0)) {
        S3ORendering = 1;
        SetupS3OBasicDrawing(teamColor);
      } else if (model->HasTex(0)) {
        uint const texture = model->texBindings[0].texture->glIdent;
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
      }
      break;
    case RM_TEXTURE0COLOR:
      if (model->HasTex(0)) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, model->texBindings[0].texture->glIdent);
      }
      S3ORendering = 3;
      break;
    case RM_TEXTURE1COLOR:
      if (model->HasTex(1)) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, model->texBindings[1].texture->glIdent);
      }
      S3ORendering = 3;
      break;
  }

  glColor4ub(255, 255, 255, 255);
  return S3ORendering;
}

int ModelDrawer::SetupTextureMapping(IView* v, const Vector3& teamColor) {
  switch (model->mapping) {
    case MAPPING_S3O:
      return SetupS3OTextureMapping(v, teamColor);
    case MAPPING_3DO:
      model->load_3do_textures(v->GetTextureHandler());
  }
  return 0;
}

void ModelDrawer::Render(Model* mdl, IView* v, const Vector3& teamColor) {
  if (!glewInitialized) {
    SetupGL();
  }

  model = mdl;
  if (model->root == nullptr) {
    return;
  }

  int S3ORendering = 0;
  MdlObject* root = model->root;

  if (v->IsSelecting()) {
    glDisable(GL_TEXTURE_2D);
  } else if (v->GetRenderMode() == M3D_TEX) {
    S3ORendering = SetupTextureMapping(v, teamColor);
  }

  glEnable(GL_NORMALIZE);

  Matrix ident;
  ident.identity();
  RenderObject(root, v, model->mapping);

  if (S3ORendering > 0) {
    if (S3ORendering == 2) {
      CleanupS3OAdvDrawing();
    } else if (S3ORendering == 1) {
      CleanupS3OBasicDrawing();
    }
    glDisable(GL_TEXTURE_2D);
  }

  glDisable(GL_LIGHTING);
  RenderHelperGeom(root, v);

  if (!v->IsSelecting()) {
    RenderSelection(v);
  }

  // draw radius
  if (v->GetConfig(CFG_DRAWRADIUS) != 0.0F) {
    glPushMatrix();
    glTranslatef(model->mid.x, model->mid.y, model->mid.z);
    glScalef(model->radius, model->radius, model->radius);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor3ub(255, 255, 255);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glCallList(sphereList);
    glPopMatrix();
  }

  // draw height
  if (v->GetConfig(CFG_DRAWHEIGHT) != 0.0F) {
    glLineWidth(5.0F);

    glColor3ub(255, 255, 0);
    glBegin(GL_LINES);
    glVertex3i(0, 0, 0);
    glVertex3f(0.0F, model->height, 0.0F);
    glEnd();

    glLineWidth(1.0F);
  }
}

void ModelDrawer::RenderPolygonVertexNormals(PolyMesh* o, Poly* pl) {
  glColor3ub(255, 0, 0);
  glDisable(GL_TEXTURE_2D);
  glBegin(GL_LINES);
  for (int const vert : pl->verts) {
    Vertex& v = o->verts[vert];
    glVertex3fv(v.pos.getf());
    glVertex3fv((v.pos + v.normal).getf());
  }
  glEnd();
  glColor3ub(255, 255, 255);
}

static Vector3 ProjectVector(const Vector3& planeNorm, const Vector3& v) {
  float const d = planeNorm | v;

  return v - planeNorm * d;
}

static Vector3 CalcTangentOnEdge(const Vector3& edgeDir, const Vector3& surfNormal,
                                 const Vector3& vertNormal) {
  Vector3 binormal;
  Vector3 projVN;

  binormal = surfNormal ^ edgeDir;

  // project the vertex normal on the plane defined by surfNormal and edgeDir
  // (which is a plane with normal=surfNormal x edgeDir)
  projVN = ProjectVector(binormal, vertNormal);

  Vector3 tg = binormal ^ projVN;
  tg.normalize();
  return tg;
}

void ModelDrawer::RenderSmoothPolygon(PolyMesh* pm, Poly* pl) {
  const int steps = 5;
  const float step = 1.0F / steps;

  Vector3 const surfNormal = pl->CalcPlane(pm->verts).GetVector() * 0.01F;

  if (pl->verts.size() == 4) {
    Vector3 rowStart;
    Vector3 rowEnd;
    Vector3 leftEdge;
    Vector3 rightEdge;
    Vertex* verts[4];
    for (int a = 0; a < 4; a++) {
      verts[a] = &pm->verts[pl->verts[a]];
    }

    leftEdge = verts[3]->pos - verts[0]->pos;
    rightEdge = verts[2]->pos - verts[1]->pos;

    if (bufferSize < steps * steps) {
      bufferSize = steps * steps;
      buffer = new Vector3[bufferSize];
    }
    Vector3* p = buffer;

    for (float y = 0.0F; y < 1.0F; y += step) {
      rowStart = verts[0]->pos + leftEdge * y;
      rowEnd = verts[1]->pos + rightEdge * y;

      Vector3 const rowVec = rowEnd - rowStart;

      for (float x = 0.0F; x < 1.0F; x += step) {
        *(p++) = rowStart + rowVec * x;
      }
    }

    glBegin(GL_POINTS);
    for (int y = 0; y < steps; y++) {
      for (int x = 0; x < steps; x++) {
        glVertex3fv(buffer[y * steps + x].getf());
      }
    }
    glEnd();
  } else if (pl->verts.size() == 3) {
    Vector3 rowStart;
    Vector3 const rowEnd;

    Vertex* verts[3];
    for (int a = 0; a < 3; a++) {
      verts[a] = &pm->verts[pl->verts[a]];
    }

    Vector3 const Xdir = verts[1]->pos - verts[2]->pos;
    Vector3 const Ydir = verts[2]->pos - verts[0]->pos;

    if (bufferSize < steps * steps / 2) {
      bufferSize = steps * steps / 2;
      buffer = new Vector3[bufferSize];
    }
    Vector3* p = buffer;

    glBegin(GL_POINTS);
    for (float y = 0.0F; y < 1.0F; y += step) {
      rowStart = verts[0]->pos + Ydir * y;
      for (float x = 0.0F; x < y; x += step) {
        Vector3 v;
        v = rowStart + Xdir * x;
        v += surfNormal;
        assert(p != buffer + bufferSize);
        *(p++) = v;
        glVertex3fv(v.getf());
      }
    }
    glEnd();
  }

  for (std::uint32_t a = 0; a < pl->verts.size();
       a++) { /*
                     Vertex& next = o->verts[pl->verts[(a+1 >= pl->verts.size()) ? 0 : a+1]];
                     Vertex& prev = o->verts[pl->verts[(a-1 < 0) ? pl->verts.size()-1 : a-1]];
                     Vertex& cur = o->verts[pl->verts[a]];*/

    Vertex& v1 = pm->verts[pl->verts[a]];
    Vertex& v2 = pm->verts[pl->verts[(a + 1) % pl->verts.size()]];

    glBegin(GL_LINES);
    glColor3ub(255, 0, 0);

    Vector3 const edge = v2.pos - v1.pos;
    float const edgeLen = edge.length();

    // Calculate for v1
    Vector3 tgStart = CalcTangentOnEdge(edge, surfNormal, v1.normal);
    tgStart.normalize();
    glVertex3fv(v1.pos.getf());
    glVertex3fv((v1.pos + tgStart).getf());

    glColor3ub(0, 0, 255);
    // Calculate for v2
    Vector3 tgEnd = CalcTangentOnEdge(edge, surfNormal, v2.normal);
    tgEnd.normalize();

    glVertex3fv(v2.pos.getf());
    glVertex3fv((v2.pos + tgEnd).getf());

    glEnd();

    tgStart *= edgeLen;
    tgEnd *= edgeLen;

    const int steps = 20;
    Vector3 prev;
    glBegin(GL_LINE_STRIP);
    for (int b = 0; b < steps; b++) {
      float const t = static_cast<float>(b) / steps;
      float w[4];  // weights
      CubicHermiteSplineWeights(t, w);

      Vector3 pos = v1.pos * w[0] + tgStart * w[1] + v2.pos * w[2] + tgEnd * w[3];
      glVertex3fv(pos.getf());
      glColor3f(1.0F - t, 0.0F, t);
      prev = pos;
    }
    glEnd();
  }
  glColor3ub(255, 255, 255);
}

void ModelDrawer::RenderHelperGeom(MdlObject* o, IView* v) {
  // setup object transformation
  Matrix tmp;
  Matrix mat;
  o->GetTransform(mat);
  mat.transpose(&tmp);
  glPushMatrix();
  glMultMatrixf(reinterpret_cast<float*>(&tmp));

  if (o->csurfobj != nullptr) {
    o->csurfobj->Draw();
  }

  PolyMesh* pm = o->GetPolyMesh();
  if (v->GetConfig(CFG_VRTNORMALS) != 0.0F) {
    if (o->isSelected && (pm != nullptr)) {
      for (std::uint32_t a = 0; a < pm->poly.size(); a++) {
        //	if (o->poly[a]->isSelected)
        RenderPolygonVertexNormals(pm, pm->poly[a]);
      }
    }
  }

  if (v->GetConfig(CFG_MESHSMOOTH) != 0.0F) {
    for (std::uint32_t a = 0; a < pm->poly.size(); a++) {
      if (pm->poly[a]->isSelected) {
        RenderSmoothPolygon(pm, pm->poly[a]);
      }
    }
  }

  for (auto& child : o->childs) {
    RenderHelperGeom(child, v);
  }

  glPopMatrix();
}

void ModelDrawer::RenderSelection_(MdlObject* o, IView* view) {
  // setup object transformation
  Matrix tmp;
  Matrix mat;
  o->GetTransform(mat);
  mat.transpose(&tmp);
  glPushMatrix();
  glMultMatrixf(reinterpret_cast<float*>(&tmp));

  glColor3ub(0, 0, 255);

  bool const psel = view->GetConfig(CFG_POLYSELECT) != 0.0F;
  for (PolyIterator pi(o); !pi.End(); pi.Next()) {
    Poly* pl = *pi;

    if ((o->isSelected && !psel) || (pl->isSelected && psel)) {
      if (pi.verts() == nullptr) {
        continue;
      }

      glBegin(GL_POLYGON);
      for (int const vert : pl->verts) {
        glVertex3fv(reinterpret_cast<float*>(&(*pi.verts())[vert].pos));
      }
      glEnd();
    }
  }
  for (auto& child : o->childs) {
    RenderSelection_(child, view);
  }

  glPopMatrix();
}

void ModelDrawer::RenderSelection(IView* view) {
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(0.0F, -10.0F);
  glDepthMask(GL_FALSE);

  unsigned long pattern[32];
  unsigned long* i = pattern;
  unsigned long val = 0xCCCCCCCC;
  for (; i < pattern + 32;) {
    *(i++) = val;
    *(i++) = val;
    val = ~val;
  }
  glPolygonStipple(reinterpret_cast<GLubyte*>(&pattern[0]));
  glEnable(GL_POLYGON_STIPPLE);
  glDisable(GL_LIGHTING);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glDisable(GL_CULL_FACE);

  if (model->root != nullptr) {
    RenderSelection_(model->root, view);
  }

  glDisable(GL_POLYGON_STIPPLE);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glDepthMask(GL_TRUE);
  glPolygonOffset(0.0F, 0.0F);
}

void ModelDrawer::SetupS3OBasicDrawing(const Vector3& teamcol) {
  glColor3f(1, 1, 1);

  // RGB = Texture * Alpha + Teamcolor * (1-Alpha)
  glActiveTextureARB(GL_TEXTURE0_ARB);
  glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_INTERPOLATE_ARB);
  glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
  glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_CONSTANT_ARB);
  glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_TEXTURE);
  glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_ONE_MINUS_SRC_ALPHA);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
  glEnable(GL_TEXTURE_2D);

  // set team color
  float tc[4] = {teamcol.x, teamcol.y, teamcol.z, 1.0F};
  glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, tc);

  // bind texture 0
  glBindTexture(GL_TEXTURE_2D, model->texBindings[0].texture->glIdent);

  // RGB = Primary Color * Previous
  glActiveTextureARB(GL_TEXTURE1_ARB);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
  glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
  glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR_ARB);
  glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, whiteTexture);

  glActiveTextureARB(GL_TEXTURE0_ARB);

  glDisable(GL_ALPHA_TEST);
  glDisable(GL_BLEND);

  TestGLError();
}

void ModelDrawer::CleanupS3OBasicDrawing() {
  glActiveTextureARB(GL_TEXTURE1_ARB);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glDisable(GL_TEXTURE_2D);

  glActiveTextureARB(GL_TEXTURE0_ARB);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_ALPHA);
  glDisable(GL_TEXTURE_2D);
}

void ModelDrawer::SetupS3OAdvDrawing(const Vector3& teamcol, IView* /*v*/) {
  glEnable(GL_VERTEX_PROGRAM_ARB);
  glEnable(GL_FRAGMENT_PROGRAM_ARB);
  if (glIsEnabled(GL_LIGHTING) != 0U) {
    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, s3oVP);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, s3oFP);
  } else {
    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, s3oVPunlit);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, s3oFPunlit);
  }

  glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, teamcol.x, teamcol.y, teamcol.z, 1.0F);

  glActiveTextureARB(GL_TEXTURE0_ARB);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, model->TextureID(0));

  glActiveTextureARB(GL_TEXTURE1_ARB);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, model->TextureID(1));

  glActiveTextureARB(GL_TEXTURE2_ARB);
  glEnable(GL_TEXTURE_CUBE_MAP_ARB);
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, skyboxTexture);

  glActiveTextureARB(GL_TEXTURE0_ARB);
}

void ModelDrawer::CleanupS3OAdvDrawing() {
  glDisable(GL_VERTEX_PROGRAM_ARB);
  glDisable(GL_FRAGMENT_PROGRAM_ARB);

  glActiveTextureARB(GL_TEXTURE2_ARB);
  glDisable(GL_TEXTURE_CUBE_MAP_ARB);

  glActiveTextureARB(GL_TEXTURE1_ARB);
  glDisable(GL_TEXTURE_2D);

  glActiveTextureARB(GL_TEXTURE0_ARB);
  glDisable(GL_TEXTURE_2D);
}

char* LoadTextFile(std::string fn, int& l) {
  FILE* f = fopen(fn.c_str(), "rb");
  if (f == nullptr) {
    fltk::message("Failed to open %s", fn.c_str());
    return nullptr;
  }
  fseek(f, 0, SEEK_END);
  l = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* buf = new char[l];
  if (fread(buf, l, 1, f) != 0U) {
  }
  fclose(f);
  return buf;
}

uint LoadVertexProgram(std::string fn) {
  int l = 0;
  char* buf = LoadTextFile(fn, l);
  if (buf == nullptr) {
    return 0;
  }

  uint ret = 0;
  glGenProgramsARB(1, &ret);
  glBindProgramARB(GL_VERTEX_PROGRAM_ARB, ret);

  glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, l, buf);
  delete[] buf;

  if (GL_INVALID_OPERATION == glGetError()) {
    // Find the error position
    GLint errPos = 0;
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errPos);
    // Print implementation-dependent program
    // errors and warnings string.
    const GLubyte* errString = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
    fltk::message("Error at position %d when loading vertex program file %s:\n%s", errPos,
                  fn.c_str(), errString);
    return 0;
  }
  return ret;
}

uint LoadFragmentProgram(std::string fn) {
  int len = 0;
  char* buf = LoadTextFile(fn, len);
  if (buf == nullptr) {
    return 0;
  }

  uint ret = 0;
  glGenProgramsARB(1, &ret);
  glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ret);

  glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, len, buf);

  if (GL_INVALID_OPERATION == glGetError()) {
    // Find the error position
    GLint errPos = 0;
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errPos);
    // Print implementation-dependent program
    // errors and warnings string.
    const GLubyte* errString = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
    fltk::message("Error at position %d when loading fragment program file %s:\n%s", errPos,
                  fn.c_str(), errString);
    return 0;
  }
  return ret;
}
