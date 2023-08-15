%module upspring

#pragma SWIG nowarn=509

%{
#undef SWIG_fail_arg
#define SWIG_fail_arg(func_name,argnum,type) \
  {lua_Debug ar;\
  lua_getstack(L, 1, &ar);\
  lua_getinfo(L, "nSl", &ar);\
  lua_pushfstring(L,"Error (%s:%d) in %s (arg %d), expected '%s' got '%s'",\
  ar.source,ar.currentline,func_name,argnum,type,SWIG_Lua_typename(L,argnum));\
  goto fail;}
%}


%{
#include "../EditorIncl.h"
#include "../EditorDef.h"
#include "ScriptInterface.h"
#include "../Model.h"
#include "../DebugTrace.h"
#include "../Texture.h"
#include "../Util.h"
#include "spdlog/spdlog.h"
#include "../Image.h"

#include <iostream>
%}

%ignore CR_DECLARE;
%ignore CR_DECLARE_STRUCT;
%ignore NO_SCRIPT;

%include "std_vector.i"
%rename(cppstring) string;
%include "std_string.i"
%include "list.i"

%extend std::vector { int __len(void*) { return self->size(); } }

namespace std {
	%template(vectori) vector<int>; 
	%template(vectorf) vector<float>;
	%template(vectorc) vector<char>;
	%template(vectors) vector<short>;
}

namespace std {
	%template(PolyRefArray) vector<Poly*>;
	%template(VertexArray) vector<Vertex>;
	%template(TriArray) vector<Triangle>;
	%template(ObjectRefArray) vector<MdlObject*>;
	%template(AnimationInfoRefArray) vector<AnimationInfo*>;
	%template(AnimationInfoList) list<AnimationInfo>;
	%template(AnimInfoListIt)  list_iterator<AnimationInfo>;
	%template(AnimInfoListRevIt)  list_reverse_iterator<AnimationInfo>;
	%template(AnimPropertyList) list<AnimProperty>;
	%template(AnimPropListIt) list_iterator<AnimProperty>;
	%template(AnimPropListRevIt) list_reverse_iterator<AnimProperty>;
	%template(AnimPropertyRefArray) vector<AnimProperty*>;
}

%include "../DebugTrace.h"
%include "../math/Mathlib.h"
%include "../Model.h"
%include "../Animation.h"
%include "../IEditor.h"
%include "ScriptInterface.h"
%include "../Image.h"
%include "../Texture.h"
%include "../Atlas/atlas.hpp"
//%include "../Fltk.h"

%feature("immutable") MdlObject::parent;
%feature("immutable") MdlObject::childs;
%feature("immutable") MdlObject::poly;

%newobject MdlObject::Clone();
%newobject Poly::Clone();

namespace fltk{
void message(const char *fmt, ...);
const char* input(const char *label, const char *def);
}

// ---------------------------------------------------------------
// Script util functions
// ---------------------------------------------------------------

%extend Model {
	void SetRoot(MdlObject *o) {
		self->root = o;
	}
}

%extend MdlObject {
	void NewPolyMesh() {
		delete self->geometry;
		self->geometry = new PolyMesh;
	}
}

%inline %{
Model* upsGetModel() { return upsGetEditor()->GetMdl(); }
MdlObject* upsGetRootObj() { return upsGetEditor()->GetMdl()->root; }
void upsUpdateViews() { upsGetEditor()->Update(); }
bool _upsFileSaveDlg (const char *msg, const char *pattern, std::string& fn) { return FileSaveDlg(msg, pattern, fn); }
bool _upsFileOpenDlg (const char *msg, const char *pattern, std::string& fn) { return FileOpenDlg(msg, pattern, fn); }
%}

inline %{
#include "../Texture.h"
#include "../Model.h"
#include "../Atlas/atlas.hpp"

namespace UpsScript {
	auto textureHandler = std::make_shared<TextureHandler>();

	std::shared_ptr<TextureHandler> get_texture_handler() {
		return textureHandler;
	}

	void load_archive(const std::string &pArchive) {
		std::cout << "Loading 3DO textures from archive: " << pArchive << std::endl;
		textureHandler->Load3DO(pArchive);
	}

	void load_archives() {
		ArchiveList archives;
		archives.Load();

		for (auto it = archives.archives.begin(); it !=archives.archives.end(); ++it) {
			load_archive(*it);
		}
	};

	void textures_to_model(Model *pModel) {
		pModel->load_3do_textures(textureHandler);
	}

	void make_archive_atlas(const std::string &archive_par, const std::string &par_savepath, bool par_power_of_two) {
		std::cout << "Making an atlas from the archive: " << archive_par << std::endl;
		auto a = atlas::make_from_archive(archive_par, par_savepath, par_power_of_two);
		a.save(par_savepath);
	}
}
%}

namespace UpsScript {
	std::shared_ptr<TextureHandler> get_texture_handler();
	void load_archives();
	void load_archive(const std::string &pArchive);
	void textures_to_model(Model *pModel);
	void make_archive_atlas(const std::string &archive_par, const std::string &par_savepath, bool par_power_of_two);
}