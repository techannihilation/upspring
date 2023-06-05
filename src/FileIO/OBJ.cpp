//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#include "EditorDef.h"
#include "EditorIncl.h"
#include "Model.h"
#include "Util.h"

/* Values for wfPart.parttype */
enum {
  WF_FACE = 1,
  WF_LINE = 4,
  WF_VERTEX = 7,
  WF_NORMAL = 8,
  WF_TEXCOORD = 9,
  WF_UNSUPPORTED = 10
};

struct WfFaceVert {
  int vert, tex, norm;
};

struct WfFace {
  std::vector<WfFaceVert> verts;
};

struct WfObject {
  ~WfObject() {
    for (auto& face : faces) {
      delete face;
    }
    faces.clear();
  }
  std::vector<Vector3> vert;
  std::vector<Vector3> norm;
  std::vector<Vector2> texc;
  std::vector<WfFace*> faces;
};

#define whitespace " \t"

/* Types of lines that may be read from object file */
enum {
  VERTEX = 1,
  NORMAL = 2,
  TEXTURE = 3,
  FACE = 4,
  MTLLIB = 5,
  USEMTL = 6,
  MAPLIB = 7,
  USEMAP = 8,
  LINE = 9,
  COMMENT = 99,
  UNSUPPORTED = 999
};

char* wfReadLine(char* buf, int bufsize, FILE* fp) {
  int len = 0;
  if (fgets(buf, bufsize, fp) == nullptr) {
    return nullptr;
  }
  len = strlen(buf) - 1;
  if (len <= 0) {
    return buf;
  }
  while ((len < bufsize) && (buf[len - 1] == '\\')) {
    if (fgets(buf + len - 1, bufsize - len, fp) == nullptr) {
      break;
    }
    len = strlen(buf) - 1;
  }
  return buf;
}

static void get_vertex(WfObject* obj) {
  char* v = nullptr;
  Vector3 pos;
  v = strtok(nullptr, whitespace);
  if (v != nullptr) {
    pos.x = atof(v);
  } else {
    pos.x = 0;
  }
  v = strtok(nullptr, whitespace);
  if (v != nullptr) {
    pos.y = atof(v);
  } else {
    pos.y = 0;
  }
  v = strtok(nullptr, whitespace);
  if (v != nullptr) {
    pos.z = atof(v);
  } else {
    pos.z = 0;
  }
  obj->vert.push_back(pos);
}

/* get_normal - processes a 'vn' line; gets the next vertex normal */
static void get_normal(WfObject* obj) {
  char* v = nullptr;
  Vector3 n;
  v = strtok(nullptr, whitespace);
  if (v != nullptr) {
    n.x = atof(v);
  } else {
    n.x = 0;
  }
  v = strtok(nullptr, whitespace);
  if (v != nullptr) {
    n.y = atof(v);
  } else {
    n.y = 0;
  }
  v = strtok(nullptr, whitespace);
  if (v != nullptr) {
    n.z = atof(v);
  } else {
    n.z = 0;
  }
  obj->norm.push_back(n);
}

/* get_texcoord - processes a 'vt' line; gets the next texture vertex */
static void get_texcoord(WfObject* obj) {
  char* v = nullptr;
  Vector3 tc;
  v = strtok(nullptr, whitespace);
  if (v != nullptr) {
    tc.x = atof(v);
  } else {
    tc.x = 0;
  }
  v = strtok(nullptr, whitespace);
  if (v != nullptr) {
    tc.y = atof(v);
  } else {
    tc.y = 0;
  }
  v = strtok(nullptr, whitespace);
  if (v != nullptr) {
    tc.z = atof(v);
  } else {
    tc.z = 0;
  }
  obj->texc.emplace_back(tc.x, tc.y);
}

static void get_face(char* line, WfObject* obj) {
  int nverts = 0;
  int i = 0;
  int v = 0;
  int vt = 0;
  int vn = 0;
  char* s = nullptr;
  char* r = nullptr;
  /* Count the number of vertices on the line */
  while (strtok(nullptr, whitespace) != nullptr) {
    nverts++;
  }
  /* Create the struct */
  auto* f = new WfFace;
  s = strtok(line, whitespace);

  while ((s = strtok(nullptr, whitespace)) != nullptr) {
    v = atoi(s);

    WfFaceVert fv{};
    if (v >= 0) {
      fv.vert = v;
    } else {
      fv.vert = obj->vert.size() + 1 + v;
    }
    /* Find the vertex texture after the first '/' */
    r = s;
    while (((*r) != 0) && (*r != '/')) {
      r++;
    }
    if (*r != 0) {
      r++;
      vt = atoi(r);
      if (vt >= 0) {
        fv.tex = vt;
      } else {
        fv.tex = obj->texc.size() + 1 + vt;
      }
    } else {
      fv.tex = 0;
    }
    /* Find the vertex normal after the second '/' */
    while (((*r) != 0) && (*r != '/')) {
      r++;
    }
    if (*r != 0) {
      r++;
      vn = atoi(r);
      if (vn >= 0) {
        fv.norm = vn;
      } else {
        fv.tex = obj->norm.size() + 1 + vn;
      }
    } else {
      fv.norm = 0;
    }

    f->verts.push_back(fv);

    i++;
  }
  obj->faces.push_back(f);
}

/* line_type - determines the type of 'command' a line contains. Calls
        strtok() to get the first std::string on the line, and to prepare it
        for parsing by other routines. */
static int line_type(char* line) {
  char* cmd = nullptr;
  cmd = strtok(line, whitespace);
  if (cmd == nullptr) {
    return (COMMENT);
  }
  if (strcmp(cmd, "v") == 0) {
    return (VERTEX);
  }
  if (strcmp(cmd, "vn") == 0) {
    return (NORMAL);
  }
  if (strcmp(cmd, "vt") == 0) {
    return (TEXTURE);
  }
  if (strcmp(cmd, "l") == 0) {
    return (LINE);
  }
  if (strcmp(cmd, "f") == 0) {
    return (FACE);
  }
  if (strcmp(cmd, "fo") == 0) {
    return (FACE);
  }
  if (strcmp(cmd, "mtllib") == 0) {
    return (MTLLIB);
  } if (!strcmp(cmd, "usemtl"))
    return (USEMTL);
  else if (!strcmp(cmd, "maplib"))
    return (MAPLIB);
  else if (!strcmp(cmd, "usemap"))
    return (USEMAP);
  else if (!strcmp(cmd, "#"))
    return (COMMENT);
  return (UNSUPPORTED);
}

/* process_line - Determines what 'command' a line contains and adds
        the info to the object */
static void process_line(char* line, WfObject* obj) {
  int t = 0;
  char linecopy[1024];
  /* Save a copy of the line for get_face et al, before line_type() strtok's it */
  strcpy(linecopy, line);
  t = line_type(line);
  switch (t) {
    case VERTEX:
      get_vertex(obj);
      break;
    case NORMAL:
      get_normal(obj);
      break;
    case TEXTURE:
      get_texcoord(obj);
      break;
    case FACE:
      get_face(linecopy, obj);
      break;
  }
}
/*

static int countFaces(wfObject *obj);
static void computeFaceNormal(wfFace *face,wfObject *obj,wfNormal norm,int clockwise);
static void assignNormalIndices(wfFace *face,int faceNum,int smooth);
static void addNormalToVertNorms(wfFace *face,wfObject *obj,wfNormal norm);

void wfComputeNormals(wfObject *obj,int smooth,int clockwise)
{
 int numFaces = countFaces(obj),i;
 wfNormal *faceNorm;
 wfPart *part;
 if (obj->norm)
        wfFree(obj->norm);
 if (smooth)
        obj->nnorms = obj->nverts;
 else
        obj->nnorms = numFaces;
 obj->norm = (wfNormal *) wfAlloc(obj->nnorms * sizeof(wfNormal));
 faceNorm = (wfNormal *) wfAlloc(numFaces * sizeof(wfNormal));
 if (smooth)
        bzero(obj->norm,obj->nnorms * sizeof(wfNormal));
 for (part = obj->parts, i=0; part; part = part->next)
        if (part->parttype == WF_FACE)
                {
                computeFaceNormal(&part->part.face,obj,faceNorm[i],clockwise);
                assignNormalIndices(&part->part.face,i,smooth);
                if (smooth)
                        addNormalToVertNorms(&part->part.face,obj,faceNorm[i]);
                i++;
                }
 if (smooth)
        for (i=0; i<obj->nnorms; i++)
                normalizeNormal(obj->norm[i]);
 else
        memcpy(obj->norm,faceNorm,numFaces * sizeof(wfNormal));
 free(faceNorm);
}

static int countFaces(wfObject *obj)
{
 wfPart *part;
 int num=0;
 for (part = obj->parts; part; part = part->next)
        if (part->parttype == WF_FACE)
                num++;
 return num;
}

static void computeFaceNormal(wfFace *face,wfObject *obj,wfNormal norm,int clockwise)
{
 float *v0,*v1,*v2;
 float dx1,dy1,dz1,dx2,dy2,dz2,nx,ny,nz,len;
 norm[0] = norm[1] = 0; norm[2] = 1;
 if (face->nverts < 3)
        return;
 v0 = obj->vert[face->vert[0] - 1];
 v1 = obj->vert[face->vert[1] - 1];
 v2 = obj->vert[face->vert[2] - 1];
 if (clockwise)
        {
        dx1 = v0[0] - v1[0];
        dy1 = v0[1] - v1[1];
        dz1 = v0[2] - v1[2];
        dx2 = v2[0] - v1[0];
        dy2 = v2[1] - v1[1];
        dz2 = v2[2] - v1[2];
        }
 else
        {
        dx2 = v0[0] - v1[0];
        dy2 = v0[1] - v1[1];
        dz2 = v0[2] - v1[2];
        dx1 = v2[0] - v1[0];
        dy1 = v2[1] - v1[1];
        dz1 = v2[2] - v1[2];
        }
 nx = dy1*dz2 - dy2*dz1;
 ny = dz1*dx2 - dz2*dx1;
 nz = dx1*dy2 - dx2*dy1;
 len = fsqrt(nx*nx + ny*ny + nz*nz);
 if (len < 1e-20)
        return;
 norm[0] = nx / len;
 norm[1] = ny / len;
 norm[2] = nz / len;
}

static void assignNormalIndices(wfFace *face,int faceNum,int smooth)
{
 int i;
 if (!face->norm)
        face->norm = (int *) wfAlloc(face->nverts * sizeof(int));
 if (smooth)
        {
        for (i=0; i<face->nverts; i++)
                face->norm[i] = face->vert[i];
        }
 else
        {
        for (i=0; i<face->nverts; i++)
                face->norm[i] = faceNum + 1;
        }
}

static void addNormalToVertNorms(wfFace *face,wfObject *obj,wfNormal norm)
{
 int i,v;
 for (i=0; i<face->nverts; i++)
        {
        v = face->vert[i] - 1;
        obj->norm[v][0] += norm[0];
        obj->norm[v][1] += norm[1];
        obj->norm[v][2] += norm[2];
        }
}

*/
WfObject* ReadWFObject(char* fname, IProgressCtl& progctl) {
  FILE* fp = nullptr;
  char line[1024];
  if ((fp = fopen(fname, "r")) == nullptr) {
    return (nullptr);
  }
  int len = 0;
  fseek(fp, 0, SEEK_END);
  len = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  auto* obj = new WfObject;
  while (wfReadLine(line, 1024, fp) != nullptr) {
    line[strlen(line) - 1] = '\0'; /* get rid of newline */
    process_line(line, obj);

    int const pos = ftell(fp);
    progctl.Update(pos / static_cast<float>(len));
  }
  fclose(fp);
  return obj;
}

bool SaveWavefrontObject(const char* fn, MdlObject* src) {
  MdlObject* obj = src->Clone();
  obj->FullMerge();

  FILE* f = fopen(fn, "w");
  if (f == nullptr) {
    delete obj;
    return false;
  }

  // write verts pos
  PolyMesh* pm = obj->GetPolyMesh();
  if (pm == nullptr) {
    return false;
  }

  fprintf(f, "# %zu vertices, %zu polygons\n", pm->verts.size(), pm->poly.size());

  for (auto& vert : pm->verts) {
    fprintf(f, "v %f %f %f\n", vert.pos.x, vert.pos.y, vert.pos.z);
  }

  // write normals
  for (auto& vert : pm->verts) {
    fprintf(f, "vn %f %f %f\n", vert.normal.x, vert.normal.y, vert.normal.z);
  }

  // write tc's
  for (auto& vert : pm->verts) {
    fprintf(f, "vt %f %f 0.0\n", vert.tc[0].x, vert.tc[0].y);
  }

  // write faces
  for (auto* p : pm->poly) {
    fprintf(f, "f");
    for (int const vert : p->verts) {
      int const i = vert + 1;
      fprintf(f, " %d/%d/%d ", i, i, i);
    }
    fprintf(f, "\n");
  }
  fclose(f);

  delete obj;
  return true;
}

MdlObject* LoadWavefrontObject(const char* fn, IProgressCtl& progctl) {
  WfObject* wfobj = ReadWFObject(const_cast<char*>(fn), progctl);

  if (wfobj == nullptr) {
    return nullptr;
  }

  auto* o = new MdlObject;
  auto* pm = new PolyMesh;
  o->geometry = pm;

  for (unsigned int fi = 0; fi < wfobj->faces.size(); fi++) {
    Poly* pl = new Poly;
    WfFace* face = wfobj->faces[fi];
    pl->verts.resize(face->verts.size());

    for (unsigned int a = 0; a < pl->verts.size(); a++) {
      pm->verts.emplace_back();
      Vertex& v = pm->verts.back();

      WfFaceVert const& fv = face->verts[a];

      if (!wfobj->norm.empty() && fv.norm - 1 < static_cast<int>(wfobj->norm.size())) {
        v.normal = wfobj->norm[fv.norm - 1];
      }
      if (!wfobj->texc.empty() && fv.tex - 1 < static_cast<int>(wfobj->texc.size())) {
        v.tc[0] = wfobj->texc[fv.tex - 1];
      }
      if (!wfobj->vert.empty() && fv.vert - 1 < static_cast<int>(wfobj->vert.size())) {
        v.pos = wfobj->vert[fv.vert - 1];
      }

      pl->verts[a] = pm->verts.size() - 1;
    }

    pm->poly.push_back(pl);
  }

  delete wfobj;

  return o;
}
