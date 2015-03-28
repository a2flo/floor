/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#if !defined(__APPLE__)

#include <floor/core/gl_support.hpp>
#include <floor/core/logger.hpp>

#if defined(__WINDOWS__) || defined(WIN_UNIXENV)
#define glGetProcAddress(x) wglGetProcAddress(x)
#define ProcType LPCSTR
#else
#define glGetProcAddress(x) glXGetProcAddressARB(x)
#define ProcType GLubyte*
#endif

OGL_API PFNGLISRENDERBUFFERPROC _glIsRenderbuffer_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLBINDRENDERBUFFERPROC _glBindRenderbuffer_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLDELETERENDERBUFFERSPROC _glDeleteRenderbuffers_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLGENRENDERBUFFERSPROC _glGenRenderbuffers_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLRENDERBUFFERSTORAGEPROC _glRenderbufferStorage_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLGETRENDERBUFFERPARAMETERIVPROC _glGetRenderbufferParameteriv_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLISFRAMEBUFFERPROC _glIsFramebuffer_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLBINDFRAMEBUFFERPROC _glBindFramebuffer_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLDELETEFRAMEBUFFERSPROC _glDeleteFramebuffers_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLGENFRAMEBUFFERSPROC _glGenFramebuffers_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLCHECKFRAMEBUFFERSTATUSPROC _glCheckFramebufferStatus_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLFRAMEBUFFERTEXTURE1DPROC _glFramebufferTexture1D_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLFRAMEBUFFERTEXTURE2DPROC _glFramebufferTexture2D_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLFRAMEBUFFERTEXTURE3DPROC _glFramebufferTexture3D_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLFRAMEBUFFERRENDERBUFFERPROC _glFramebufferRenderbuffer_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC _glGetFramebufferAttachmentParameteriv_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLGENERATEMIPMAPPROC _glGenerateMipmap_ptr = nullptr; // ARB_framebuffer_object
OGL_API PFNGLBLITFRAMEBUFFERPROC _glBlitFramebuffer_ptr = nullptr; // ARB_framebuffer_object

// OpenGL 1.5
OGL_API PFNGLGENQUERIESPROC _glGenQueries_ptr = nullptr;
OGL_API PFNGLDELETEQUERIESPROC _glDeleteQueries_ptr = nullptr;
OGL_API PFNGLISQUERYPROC _glIsQuery_ptr = nullptr;
OGL_API PFNGLBEGINQUERYPROC _glBeginQuery_ptr = nullptr;
OGL_API PFNGLENDQUERYPROC _glEndQuery_ptr = nullptr;
OGL_API PFNGLGETQUERYIVPROC _glGetQueryiv_ptr = nullptr;
OGL_API PFNGLGETQUERYOBJECTIVPROC _glGetQueryObjectiv_ptr = nullptr;
OGL_API PFNGLGETQUERYOBJECTUIVPROC _glGetQueryObjectuiv_ptr = nullptr;
OGL_API PFNGLBINDBUFFERPROC _glBindBuffer_ptr = nullptr;
OGL_API PFNGLDELETEBUFFERSPROC _glDeleteBuffers_ptr = nullptr;
OGL_API PFNGLGENBUFFERSPROC _glGenBuffers_ptr = nullptr;
OGL_API PFNGLISBUFFERPROC _glIsBuffer_ptr = nullptr;
OGL_API PFNGLBUFFERDATAPROC _glBufferData_ptr = nullptr;
OGL_API PFNGLBUFFERSUBDATAPROC _glBufferSubData_ptr = nullptr;
OGL_API PFNGLGETBUFFERSUBDATAPROC _glGetBufferSubData_ptr = nullptr;
OGL_API PFNGLMAPBUFFERPROC _glMapBuffer_ptr = nullptr;
OGL_API PFNGLUNMAPBUFFERPROC _glUnmapBuffer_ptr = nullptr;
OGL_API PFNGLGETBUFFERPARAMETERIVPROC _glGetBufferParameteriv_ptr = nullptr;
OGL_API PFNGLGETBUFFERPOINTERVPROC _glGetBufferPointerv_ptr = nullptr;

// OpenGL 2.0
OGL_API PFNGLBLENDEQUATIONSEPARATEPROC _glBlendEquationSeparate_ptr = nullptr;
OGL_API PFNGLDRAWBUFFERSPROC _glDrawBuffers_ptr = nullptr;
OGL_API PFNGLSTENCILOPSEPARATEPROC _glStencilOpSeparate_ptr = nullptr;
OGL_API PFNGLSTENCILFUNCSEPARATEPROC _glStencilFuncSeparate_ptr = nullptr;
OGL_API PFNGLSTENCILMASKSEPARATEPROC _glStencilMaskSeparate_ptr = nullptr;
OGL_API PFNGLATTACHSHADERPROC _glAttachShader_ptr = nullptr;
OGL_API PFNGLBINDATTRIBLOCATIONPROC _glBindAttribLocation_ptr = nullptr;
OGL_API PFNGLCOMPILESHADERPROC _glCompileShader_ptr = nullptr;
OGL_API PFNGLCREATEPROGRAMPROC _glCreateProgram_ptr = nullptr;
OGL_API PFNGLCREATESHADERPROC _glCreateShader_ptr = nullptr;
OGL_API PFNGLDELETEPROGRAMPROC _glDeleteProgram_ptr = nullptr;
OGL_API PFNGLDELETESHADERPROC _glDeleteShader_ptr = nullptr;
OGL_API PFNGLDETACHSHADERPROC _glDetachShader_ptr = nullptr;
OGL_API PFNGLDISABLEVERTEXATTRIBARRAYPROC _glDisableVertexAttribArray_ptr = nullptr;
OGL_API PFNGLENABLEVERTEXATTRIBARRAYPROC _glEnableVertexAttribArray_ptr = nullptr;
OGL_API PFNGLGETACTIVEATTRIBPROC _glGetActiveAttrib_ptr = nullptr;
OGL_API PFNGLGETACTIVEUNIFORMPROC _glGetActiveUniform_ptr = nullptr;
OGL_API PFNGLGETATTACHEDSHADERSPROC _glGetAttachedShaders_ptr = nullptr;
OGL_API PFNGLGETATTRIBLOCATIONPROC _glGetAttribLocation_ptr = nullptr;
OGL_API PFNGLGETPROGRAMIVPROC _glGetProgramiv_ptr = nullptr;
OGL_API PFNGLGETPROGRAMINFOLOGPROC _glGetProgramInfoLog_ptr = nullptr;
OGL_API PFNGLGETSHADERIVPROC _glGetShaderiv_ptr = nullptr;
OGL_API PFNGLGETSHADERINFOLOGPROC _glGetShaderInfoLog_ptr = nullptr;
OGL_API PFNGLGETSHADERSOURCEPROC _glGetShaderSource_ptr = nullptr;
OGL_API PFNGLGETUNIFORMLOCATIONPROC _glGetUniformLocation_ptr = nullptr;
OGL_API PFNGLGETUNIFORMFVPROC _glGetUniformfv_ptr = nullptr;
OGL_API PFNGLGETUNIFORMIVPROC _glGetUniformiv_ptr = nullptr;
OGL_API PFNGLGETVERTEXATTRIBDVPROC _glGetVertexAttribdv_ptr = nullptr;
OGL_API PFNGLGETVERTEXATTRIBFVPROC _glGetVertexAttribfv_ptr = nullptr;
OGL_API PFNGLGETVERTEXATTRIBIVPROC _glGetVertexAttribiv_ptr = nullptr;
OGL_API PFNGLGETVERTEXATTRIBPOINTERVPROC _glGetVertexAttribPointerv_ptr = nullptr;
OGL_API PFNGLISPROGRAMPROC _glIsProgram_ptr = nullptr;
OGL_API PFNGLISSHADERPROC _glIsShader_ptr = nullptr;
OGL_API PFNGLLINKPROGRAMPROC _glLinkProgram_ptr = nullptr;
OGL_API PFNGLSHADERSOURCEPROC _glShaderSource_ptr = nullptr;
OGL_API PFNGLUSEPROGRAMPROC _glUseProgram_ptr = nullptr;
OGL_API PFNGLUNIFORM1FPROC _glUniform1f_ptr = nullptr;
OGL_API PFNGLUNIFORM2FPROC _glUniform2f_ptr = nullptr;
OGL_API PFNGLUNIFORM3FPROC _glUniform3f_ptr = nullptr;
OGL_API PFNGLUNIFORM4FPROC _glUniform4f_ptr = nullptr;
OGL_API PFNGLUNIFORM1IPROC _glUniform1i_ptr = nullptr;
OGL_API PFNGLUNIFORM2IPROC _glUniform2i_ptr = nullptr;
OGL_API PFNGLUNIFORM3IPROC _glUniform3i_ptr = nullptr;
OGL_API PFNGLUNIFORM4IPROC _glUniform4i_ptr = nullptr;
OGL_API PFNGLUNIFORM1FVPROC _glUniform1fv_ptr = nullptr;
OGL_API PFNGLUNIFORM2FVPROC _glUniform2fv_ptr = nullptr;
OGL_API PFNGLUNIFORM3FVPROC _glUniform3fv_ptr = nullptr;
OGL_API PFNGLUNIFORM4FVPROC _glUniform4fv_ptr = nullptr;
OGL_API PFNGLUNIFORM1IVPROC _glUniform1iv_ptr = nullptr;
OGL_API PFNGLUNIFORM2IVPROC _glUniform2iv_ptr = nullptr;
OGL_API PFNGLUNIFORM3IVPROC _glUniform3iv_ptr = nullptr;
OGL_API PFNGLUNIFORM4IVPROC _glUniform4iv_ptr = nullptr;
OGL_API PFNGLUNIFORMMATRIX2FVPROC _glUniformMatrix2fv_ptr = nullptr;
OGL_API PFNGLUNIFORMMATRIX3FVPROC _glUniformMatrix3fv_ptr = nullptr;
OGL_API PFNGLUNIFORMMATRIX4FVPROC _glUniformMatrix4fv_ptr = nullptr;
OGL_API PFNGLVALIDATEPROGRAMPROC _glValidateProgram_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB1DPROC _glVertexAttrib1d_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB1DVPROC _glVertexAttrib1dv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB1FPROC _glVertexAttrib1f_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB1FVPROC _glVertexAttrib1fv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB1SPROC _glVertexAttrib1s_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB1SVPROC _glVertexAttrib1sv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB2DPROC _glVertexAttrib2d_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB2DVPROC _glVertexAttrib2dv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB2FPROC _glVertexAttrib2f_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB2FVPROC _glVertexAttrib2fv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB2SPROC _glVertexAttrib2s_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB2SVPROC _glVertexAttrib2sv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB3DPROC _glVertexAttrib3d_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB3DVPROC _glVertexAttrib3dv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB3FPROC _glVertexAttrib3f_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB3FVPROC _glVertexAttrib3fv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB3SPROC _glVertexAttrib3s_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB3SVPROC _glVertexAttrib3sv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4NBVPROC _glVertexAttrib4Nbv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4NIVPROC _glVertexAttrib4Niv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4NSVPROC _glVertexAttrib4Nsv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4NUBPROC _glVertexAttrib4Nub_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4NUBVPROC _glVertexAttrib4Nubv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4NUIVPROC _glVertexAttrib4Nuiv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4NUSVPROC _glVertexAttrib4Nusv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4BVPROC _glVertexAttrib4bv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4DPROC _glVertexAttrib4d_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4DVPROC _glVertexAttrib4dv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4FPROC _glVertexAttrib4f_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4FVPROC _glVertexAttrib4fv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4IVPROC _glVertexAttrib4iv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4SPROC _glVertexAttrib4s_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4SVPROC _glVertexAttrib4sv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4UBVPROC _glVertexAttrib4ubv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4UIVPROC _glVertexAttrib4uiv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIB4USVPROC _glVertexAttrib4usv_ptr = nullptr;
OGL_API PFNGLVERTEXATTRIBPOINTERPROC _glVertexAttribPointer_ptr = nullptr;

void init_gl_funcs() {
	// OpenGL 1.5
	_glGenQueries_ptr = (PFNGLGENQUERIESPROC)glGetProcAddress((ProcType)"glGenQueries");
	_glDeleteQueries_ptr = (PFNGLDELETEQUERIESPROC)glGetProcAddress((ProcType)"glDeleteQueries");
	_glIsQuery_ptr = (PFNGLISQUERYPROC)glGetProcAddress((ProcType)"glIsQuery");
	_glBeginQuery_ptr = (PFNGLBEGINQUERYPROC)glGetProcAddress((ProcType)"glBeginQuery");
	_glEndQuery_ptr = (PFNGLENDQUERYPROC)glGetProcAddress((ProcType)"glEndQuery");
	_glGetQueryiv_ptr = (PFNGLGETQUERYIVPROC)glGetProcAddress((ProcType)"glGetQueryiv");
	_glGetQueryObjectiv_ptr = (PFNGLGETQUERYOBJECTIVPROC)glGetProcAddress((ProcType)"glGetQueryObjectiv");
	_glGetQueryObjectuiv_ptr = (PFNGLGETQUERYOBJECTUIVPROC)glGetProcAddress((ProcType)"glGetQueryObjectuiv");
	_glBindBuffer_ptr = (PFNGLBINDBUFFERPROC)glGetProcAddress((ProcType)"glBindBuffer");
	_glDeleteBuffers_ptr = (PFNGLDELETEBUFFERSPROC)glGetProcAddress((ProcType)"glDeleteBuffers");
	_glGenBuffers_ptr = (PFNGLGENBUFFERSPROC)glGetProcAddress((ProcType)"glGenBuffers");
	_glIsBuffer_ptr = (PFNGLISBUFFERPROC)glGetProcAddress((ProcType)"glIsBuffer");
	_glBufferData_ptr = (PFNGLBUFFERDATAPROC)glGetProcAddress((ProcType)"glBufferData");
	_glBufferSubData_ptr = (PFNGLBUFFERSUBDATAPROC)glGetProcAddress((ProcType)"glBufferSubData");
	_glGetBufferSubData_ptr = (PFNGLGETBUFFERSUBDATAPROC)glGetProcAddress((ProcType)"glGetBufferSubData");
	_glMapBuffer_ptr = (PFNGLMAPBUFFERPROC)glGetProcAddress((ProcType)"glMapBuffer");
	_glUnmapBuffer_ptr = (PFNGLUNMAPBUFFERPROC)glGetProcAddress((ProcType)"glUnmapBuffer");
	_glGetBufferParameteriv_ptr = (PFNGLGETBUFFERPARAMETERIVPROC)glGetProcAddress((ProcType)"glGetBufferParameteriv");
	_glGetBufferPointerv_ptr = (PFNGLGETBUFFERPOINTERVPROC)glGetProcAddress((ProcType)"glGetBufferPointerv");
	
	// OpenGL 2.0
	_glBlendEquationSeparate_ptr = (PFNGLBLENDEQUATIONSEPARATEPROC)glGetProcAddress((ProcType)"glBlendEquationSeparate");
	_glDrawBuffers_ptr = (PFNGLDRAWBUFFERSPROC)glGetProcAddress((ProcType)"glDrawBuffers");
	_glStencilOpSeparate_ptr = (PFNGLSTENCILOPSEPARATEPROC)glGetProcAddress((ProcType)"glStencilOpSeparate");
	_glStencilFuncSeparate_ptr = (PFNGLSTENCILFUNCSEPARATEPROC)glGetProcAddress((ProcType)"glStencilFuncSeparate");
	_glStencilMaskSeparate_ptr = (PFNGLSTENCILMASKSEPARATEPROC)glGetProcAddress((ProcType)"glStencilMaskSeparate");
	_glAttachShader_ptr = (PFNGLATTACHSHADERPROC)glGetProcAddress((ProcType)"glAttachShader");
	_glBindAttribLocation_ptr = (PFNGLBINDATTRIBLOCATIONPROC)glGetProcAddress((ProcType)"glBindAttribLocation");
	_glCompileShader_ptr = (PFNGLCOMPILESHADERPROC)glGetProcAddress((ProcType)"glCompileShader");
	_glCreateProgram_ptr = (PFNGLCREATEPROGRAMPROC)glGetProcAddress((ProcType)"glCreateProgram");
	_glCreateShader_ptr = (PFNGLCREATESHADERPROC)glGetProcAddress((ProcType)"glCreateShader");
	_glDeleteProgram_ptr = (PFNGLDELETEPROGRAMPROC)glGetProcAddress((ProcType)"glDeleteProgram");
	_glDeleteShader_ptr = (PFNGLDELETESHADERPROC)glGetProcAddress((ProcType)"glDeleteShader");
	_glDetachShader_ptr = (PFNGLDETACHSHADERPROC)glGetProcAddress((ProcType)"glDetachShader");
	_glDisableVertexAttribArray_ptr = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)glGetProcAddress((ProcType)"glDisableVertexAttribArray");
	_glEnableVertexAttribArray_ptr = (PFNGLENABLEVERTEXATTRIBARRAYPROC)glGetProcAddress((ProcType)"glEnableVertexAttribArray");
	_glGetActiveAttrib_ptr = (PFNGLGETACTIVEATTRIBPROC)glGetProcAddress((ProcType)"glGetActiveAttrib");
	_glGetActiveUniform_ptr = (PFNGLGETACTIVEUNIFORMPROC)glGetProcAddress((ProcType)"glGetActiveUniform");
	_glGetAttachedShaders_ptr = (PFNGLGETATTACHEDSHADERSPROC)glGetProcAddress((ProcType)"glGetAttachedShaders");
	_glGetAttribLocation_ptr = (PFNGLGETATTRIBLOCATIONPROC)glGetProcAddress((ProcType)"glGetAttribLocation");
	_glGetProgramiv_ptr = (PFNGLGETPROGRAMIVPROC)glGetProcAddress((ProcType)"glGetProgramiv");
	_glGetProgramInfoLog_ptr = (PFNGLGETPROGRAMINFOLOGPROC)glGetProcAddress((ProcType)"glGetProgramInfoLog");
	_glGetShaderiv_ptr = (PFNGLGETSHADERIVPROC)glGetProcAddress((ProcType)"glGetShaderiv");
	_glGetShaderInfoLog_ptr = (PFNGLGETSHADERINFOLOGPROC)glGetProcAddress((ProcType)"glGetShaderInfoLog");
	_glGetShaderSource_ptr = (PFNGLGETSHADERSOURCEPROC)glGetProcAddress((ProcType)"glGetShaderSource");
	_glGetUniformLocation_ptr = (PFNGLGETUNIFORMLOCATIONPROC)glGetProcAddress((ProcType)"glGetUniformLocation");
	_glGetUniformfv_ptr = (PFNGLGETUNIFORMFVPROC)glGetProcAddress((ProcType)"glGetUniformfv");
	_glGetUniformiv_ptr = (PFNGLGETUNIFORMIVPROC)glGetProcAddress((ProcType)"glGetUniformiv");
	_glGetVertexAttribdv_ptr = (PFNGLGETVERTEXATTRIBDVPROC)glGetProcAddress((ProcType)"glGetVertexAttribdv");
	_glGetVertexAttribfv_ptr = (PFNGLGETVERTEXATTRIBFVPROC)glGetProcAddress((ProcType)"glGetVertexAttribfv");
	_glGetVertexAttribiv_ptr = (PFNGLGETVERTEXATTRIBIVPROC)glGetProcAddress((ProcType)"glGetVertexAttribiv");
	_glGetVertexAttribPointerv_ptr = (PFNGLGETVERTEXATTRIBPOINTERVPROC)glGetProcAddress((ProcType)"glGetVertexAttribPointerv");
	_glIsProgram_ptr = (PFNGLISPROGRAMPROC)glGetProcAddress((ProcType)"glIsProgram");
	_glIsShader_ptr = (PFNGLISSHADERPROC)glGetProcAddress((ProcType)"glIsShader");
	_glLinkProgram_ptr = (PFNGLLINKPROGRAMPROC)glGetProcAddress((ProcType)"glLinkProgram");
	_glShaderSource_ptr = (PFNGLSHADERSOURCEPROC)glGetProcAddress((ProcType)"glShaderSource");
	_glUseProgram_ptr = (PFNGLUSEPROGRAMPROC)glGetProcAddress((ProcType)"glUseProgram");
	_glUniform1f_ptr = (PFNGLUNIFORM1FPROC)glGetProcAddress((ProcType)"glUniform1f");
	_glUniform2f_ptr = (PFNGLUNIFORM2FPROC)glGetProcAddress((ProcType)"glUniform2f");
	_glUniform3f_ptr = (PFNGLUNIFORM3FPROC)glGetProcAddress((ProcType)"glUniform3f");
	_glUniform4f_ptr = (PFNGLUNIFORM4FPROC)glGetProcAddress((ProcType)"glUniform4f");
	_glUniform1i_ptr = (PFNGLUNIFORM1IPROC)glGetProcAddress((ProcType)"glUniform1i");
	_glUniform2i_ptr = (PFNGLUNIFORM2IPROC)glGetProcAddress((ProcType)"glUniform2i");
	_glUniform3i_ptr = (PFNGLUNIFORM3IPROC)glGetProcAddress((ProcType)"glUniform3i");
	_glUniform4i_ptr = (PFNGLUNIFORM4IPROC)glGetProcAddress((ProcType)"glUniform4i");
	_glUniform1fv_ptr = (PFNGLUNIFORM1FVPROC)glGetProcAddress((ProcType)"glUniform1fv");
	_glUniform2fv_ptr = (PFNGLUNIFORM2FVPROC)glGetProcAddress((ProcType)"glUniform2fv");
	_glUniform3fv_ptr = (PFNGLUNIFORM3FVPROC)glGetProcAddress((ProcType)"glUniform3fv");
	_glUniform4fv_ptr = (PFNGLUNIFORM4FVPROC)glGetProcAddress((ProcType)"glUniform4fv");
	_glUniform1iv_ptr = (PFNGLUNIFORM1IVPROC)glGetProcAddress((ProcType)"glUniform1iv");
	_glUniform2iv_ptr = (PFNGLUNIFORM2IVPROC)glGetProcAddress((ProcType)"glUniform2iv");
	_glUniform3iv_ptr = (PFNGLUNIFORM3IVPROC)glGetProcAddress((ProcType)"glUniform3iv");
	_glUniform4iv_ptr = (PFNGLUNIFORM4IVPROC)glGetProcAddress((ProcType)"glUniform4iv");
	_glUniformMatrix2fv_ptr = (PFNGLUNIFORMMATRIX2FVPROC)glGetProcAddress((ProcType)"glUniformMatrix2fv");
	_glUniformMatrix3fv_ptr = (PFNGLUNIFORMMATRIX3FVPROC)glGetProcAddress((ProcType)"glUniformMatrix3fv");
	_glUniformMatrix4fv_ptr = (PFNGLUNIFORMMATRIX4FVPROC)glGetProcAddress((ProcType)"glUniformMatrix4fv");
	_glValidateProgram_ptr = (PFNGLVALIDATEPROGRAMPROC)glGetProcAddress((ProcType)"glValidateProgram");
	_glVertexAttrib1d_ptr = (PFNGLVERTEXATTRIB1DPROC)glGetProcAddress((ProcType)"glVertexAttrib1d");
	_glVertexAttrib1dv_ptr = (PFNGLVERTEXATTRIB1DVPROC)glGetProcAddress((ProcType)"glVertexAttrib1dv");
	_glVertexAttrib1f_ptr = (PFNGLVERTEXATTRIB1FPROC)glGetProcAddress((ProcType)"glVertexAttrib1f");
	_glVertexAttrib1fv_ptr = (PFNGLVERTEXATTRIB1FVPROC)glGetProcAddress((ProcType)"glVertexAttrib1fv");
	_glVertexAttrib1s_ptr = (PFNGLVERTEXATTRIB1SPROC)glGetProcAddress((ProcType)"glVertexAttrib1s");
	_glVertexAttrib1sv_ptr = (PFNGLVERTEXATTRIB1SVPROC)glGetProcAddress((ProcType)"glVertexAttrib1sv");
	_glVertexAttrib2d_ptr = (PFNGLVERTEXATTRIB2DPROC)glGetProcAddress((ProcType)"glVertexAttrib2d");
	_glVertexAttrib2dv_ptr = (PFNGLVERTEXATTRIB2DVPROC)glGetProcAddress((ProcType)"glVertexAttrib2dv");
	_glVertexAttrib2f_ptr = (PFNGLVERTEXATTRIB2FPROC)glGetProcAddress((ProcType)"glVertexAttrib2f");
	_glVertexAttrib2fv_ptr = (PFNGLVERTEXATTRIB2FVPROC)glGetProcAddress((ProcType)"glVertexAttrib2fv");
	_glVertexAttrib2s_ptr = (PFNGLVERTEXATTRIB2SPROC)glGetProcAddress((ProcType)"glVertexAttrib2s");
	_glVertexAttrib2sv_ptr = (PFNGLVERTEXATTRIB2SVPROC)glGetProcAddress((ProcType)"glVertexAttrib2sv");
	_glVertexAttrib3d_ptr = (PFNGLVERTEXATTRIB3DPROC)glGetProcAddress((ProcType)"glVertexAttrib3d");
	_glVertexAttrib3dv_ptr = (PFNGLVERTEXATTRIB3DVPROC)glGetProcAddress((ProcType)"glVertexAttrib3dv");
	_glVertexAttrib3f_ptr = (PFNGLVERTEXATTRIB3FPROC)glGetProcAddress((ProcType)"glVertexAttrib3f");
	_glVertexAttrib3fv_ptr = (PFNGLVERTEXATTRIB3FVPROC)glGetProcAddress((ProcType)"glVertexAttrib3fv");
	_glVertexAttrib3s_ptr = (PFNGLVERTEXATTRIB3SPROC)glGetProcAddress((ProcType)"glVertexAttrib3s");
	_glVertexAttrib3sv_ptr = (PFNGLVERTEXATTRIB3SVPROC)glGetProcAddress((ProcType)"glVertexAttrib3sv");
	_glVertexAttrib4Nbv_ptr = (PFNGLVERTEXATTRIB4NBVPROC)glGetProcAddress((ProcType)"glVertexAttrib4Nbv");
	_glVertexAttrib4Niv_ptr = (PFNGLVERTEXATTRIB4NIVPROC)glGetProcAddress((ProcType)"glVertexAttrib4Niv");
	_glVertexAttrib4Nsv_ptr = (PFNGLVERTEXATTRIB4NSVPROC)glGetProcAddress((ProcType)"glVertexAttrib4Nsv");
	_glVertexAttrib4Nub_ptr = (PFNGLVERTEXATTRIB4NUBPROC)glGetProcAddress((ProcType)"glVertexAttrib4Nub");
	_glVertexAttrib4Nubv_ptr = (PFNGLVERTEXATTRIB4NUBVPROC)glGetProcAddress((ProcType)"glVertexAttrib4Nubv");
	_glVertexAttrib4Nuiv_ptr = (PFNGLVERTEXATTRIB4NUIVPROC)glGetProcAddress((ProcType)"glVertexAttrib4Nuiv");
	_glVertexAttrib4Nusv_ptr = (PFNGLVERTEXATTRIB4NUSVPROC)glGetProcAddress((ProcType)"glVertexAttrib4Nusv");
	_glVertexAttrib4bv_ptr = (PFNGLVERTEXATTRIB4BVPROC)glGetProcAddress((ProcType)"glVertexAttrib4bv");
	_glVertexAttrib4d_ptr = (PFNGLVERTEXATTRIB4DPROC)glGetProcAddress((ProcType)"glVertexAttrib4d");
	_glVertexAttrib4dv_ptr = (PFNGLVERTEXATTRIB4DVPROC)glGetProcAddress((ProcType)"glVertexAttrib4dv");
	_glVertexAttrib4f_ptr = (PFNGLVERTEXATTRIB4FPROC)glGetProcAddress((ProcType)"glVertexAttrib4f");
	_glVertexAttrib4fv_ptr = (PFNGLVERTEXATTRIB4FVPROC)glGetProcAddress((ProcType)"glVertexAttrib4fv");
	_glVertexAttrib4iv_ptr = (PFNGLVERTEXATTRIB4IVPROC)glGetProcAddress((ProcType)"glVertexAttrib4iv");
	_glVertexAttrib4s_ptr = (PFNGLVERTEXATTRIB4SPROC)glGetProcAddress((ProcType)"glVertexAttrib4s");
	_glVertexAttrib4sv_ptr = (PFNGLVERTEXATTRIB4SVPROC)glGetProcAddress((ProcType)"glVertexAttrib4sv");
	_glVertexAttrib4ubv_ptr = (PFNGLVERTEXATTRIB4UBVPROC)glGetProcAddress((ProcType)"glVertexAttrib4ubv");
	_glVertexAttrib4uiv_ptr = (PFNGLVERTEXATTRIB4UIVPROC)glGetProcAddress((ProcType)"glVertexAttrib4uiv");
	_glVertexAttrib4usv_ptr = (PFNGLVERTEXATTRIB4USVPROC)glGetProcAddress((ProcType)"glVertexAttrib4usv");
	_glVertexAttribPointer_ptr = (PFNGLVERTEXATTRIBPOINTERPROC)glGetProcAddress((ProcType)"glVertexAttribPointer");
	
	// try core functions first
	_glIsRenderbuffer_ptr = (PFNGLISRENDERBUFFERPROC)glGetProcAddress((ProcType)"glIsRenderbuffer");
	_glBindRenderbuffer_ptr = (PFNGLBINDRENDERBUFFERPROC)glGetProcAddress((ProcType)"glBindRenderbuffer");
	_glDeleteRenderbuffers_ptr = (PFNGLDELETERENDERBUFFERSPROC)glGetProcAddress((ProcType)"glDeleteRenderbuffers");
	_glGenRenderbuffers_ptr = (PFNGLGENRENDERBUFFERSPROC)glGetProcAddress((ProcType)"glGenRenderbuffers");
	_glRenderbufferStorage_ptr = (PFNGLRENDERBUFFERSTORAGEPROC)glGetProcAddress((ProcType)"glRenderbufferStorage");
	_glGetRenderbufferParameteriv_ptr = (PFNGLGETRENDERBUFFERPARAMETERIVPROC)glGetProcAddress((ProcType)"glGetRenderbufferParameteriv");
	_glIsFramebuffer_ptr = (PFNGLISFRAMEBUFFERPROC)glGetProcAddress((ProcType)"glIsFramebuffer");
	_glBindFramebuffer_ptr = (PFNGLBINDFRAMEBUFFERPROC)glGetProcAddress((ProcType)"glBindFramebuffer");
	_glDeleteFramebuffers_ptr = (PFNGLDELETEFRAMEBUFFERSPROC)glGetProcAddress((ProcType)"glDeleteFramebuffers");
	_glGenFramebuffers_ptr = (PFNGLGENFRAMEBUFFERSPROC)glGetProcAddress((ProcType)"glGenFramebuffers");
	_glCheckFramebufferStatus_ptr = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)glGetProcAddress((ProcType)"glCheckFramebufferStatus");
	_glFramebufferTexture1D_ptr = (PFNGLFRAMEBUFFERTEXTURE1DPROC)glGetProcAddress((ProcType)"glFramebufferTexture1D");
	_glFramebufferTexture2D_ptr = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glGetProcAddress((ProcType)"glFramebufferTexture2D");
	_glFramebufferTexture3D_ptr = (PFNGLFRAMEBUFFERTEXTURE3DPROC)glGetProcAddress((ProcType)"glFramebufferTexture3D");
	_glFramebufferRenderbuffer_ptr = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)glGetProcAddress((ProcType)"glFramebufferRenderbuffer");
	_glGetFramebufferAttachmentParameteriv_ptr = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)glGetProcAddress((ProcType)"glGetFramebufferAttachmentParameteriv");
	_glGenerateMipmap_ptr = (PFNGLGENERATEMIPMAPPROC)glGetProcAddress((ProcType)"glGenerateMipmap");
	_glBlitFramebuffer_ptr = (PFNGLBLITFRAMEBUFFERPROC)glGetProcAddress((ProcType)"glBlitFramebuffer");
	
	
	// fallback (ARB_framebuffer_object)
	if(_glIsRenderbuffer_ptr == nullptr) _glIsRenderbuffer_ptr = (PFNGLISRENDERBUFFERPROC)glGetProcAddress((ProcType)"glIsRenderbufferARB"); // ARB_framebuffer_object
	if(_glBindRenderbuffer_ptr == nullptr) _glBindRenderbuffer_ptr = (PFNGLBINDRENDERBUFFERPROC)glGetProcAddress((ProcType)"glBindRenderbufferARB"); // ARB_framebuffer_object
	if(_glDeleteRenderbuffers_ptr == nullptr) _glDeleteRenderbuffers_ptr = (PFNGLDELETERENDERBUFFERSPROC)glGetProcAddress((ProcType)"glDeleteRenderbuffersARB"); // ARB_framebuffer_object
	if(_glGenRenderbuffers_ptr == nullptr) _glGenRenderbuffers_ptr = (PFNGLGENRENDERBUFFERSPROC)glGetProcAddress((ProcType)"glGenRenderbuffersARB"); // ARB_framebuffer_object
	if(_glRenderbufferStorage_ptr == nullptr) _glRenderbufferStorage_ptr = (PFNGLRENDERBUFFERSTORAGEPROC)glGetProcAddress((ProcType)"glRenderbufferStorageARB"); // ARB_framebuffer_object
	if(_glGetRenderbufferParameteriv_ptr == nullptr) _glGetRenderbufferParameteriv_ptr = (PFNGLGETRENDERBUFFERPARAMETERIVPROC)glGetProcAddress((ProcType)"glGetRenderbufferParameterivARB"); // ARB_framebuffer_object
	if(_glIsFramebuffer_ptr == nullptr) _glIsFramebuffer_ptr = (PFNGLISFRAMEBUFFERPROC)glGetProcAddress((ProcType)"glIsFramebufferARB"); // ARB_framebuffer_object
	if(_glBindFramebuffer_ptr == nullptr) _glBindFramebuffer_ptr = (PFNGLBINDFRAMEBUFFERPROC)glGetProcAddress((ProcType)"glBindFramebufferARB"); // ARB_framebuffer_object
	if(_glDeleteFramebuffers_ptr == nullptr) _glDeleteFramebuffers_ptr = (PFNGLDELETEFRAMEBUFFERSPROC)glGetProcAddress((ProcType)"glDeleteFramebuffersARB"); // ARB_framebuffer_object
	if(_glGenFramebuffers_ptr == nullptr) _glGenFramebuffers_ptr = (PFNGLGENFRAMEBUFFERSPROC)glGetProcAddress((ProcType)"glGenFramebuffersARB"); // ARB_framebuffer_object
	if(_glCheckFramebufferStatus_ptr == nullptr) _glCheckFramebufferStatus_ptr = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)glGetProcAddress((ProcType)"glCheckFramebufferStatusARB"); // ARB_framebuffer_object
	if(_glFramebufferTexture1D_ptr == nullptr) _glFramebufferTexture1D_ptr = (PFNGLFRAMEBUFFERTEXTURE1DPROC)glGetProcAddress((ProcType)"glFramebufferTexture1DARB"); // ARB_framebuffer_object
	if(_glFramebufferTexture2D_ptr == nullptr) _glFramebufferTexture2D_ptr = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glGetProcAddress((ProcType)"glFramebufferTexture2DARB"); // ARB_framebuffer_object
	if(_glFramebufferTexture3D_ptr == nullptr) _glFramebufferTexture3D_ptr = (PFNGLFRAMEBUFFERTEXTURE3DPROC)glGetProcAddress((ProcType)"glFramebufferTexture3DARB"); // ARB_framebuffer_object
	if(_glFramebufferRenderbuffer_ptr == nullptr) _glFramebufferRenderbuffer_ptr = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)glGetProcAddress((ProcType)"glFramebufferRenderbufferARB"); // ARB_framebuffer_object
	if(_glGetFramebufferAttachmentParameteriv_ptr == nullptr) _glGetFramebufferAttachmentParameteriv_ptr = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)glGetProcAddress((ProcType)"glGetFramebufferAttachmentParameterivARB"); // ARB_framebuffer_object
	if(_glGenerateMipmap_ptr == nullptr) _glGenerateMipmap_ptr = (PFNGLGENERATEMIPMAPPROC)glGetProcAddress((ProcType)"glGenerateMipmapARB"); // ARB_framebuffer_object
	if(_glBlitFramebuffer_ptr == nullptr) _glBlitFramebuffer_ptr = (PFNGLBLITFRAMEBUFFERPROC)glGetProcAddress((ProcType)"glBlitFramebufferARB"); // ARB_framebuffer_object
	
	// fallback (EXT_framebuffer_object, EXT_framebuffer_blit)
	if(_glIsRenderbuffer_ptr == nullptr) _glIsRenderbuffer_ptr = (PFNGLISRENDERBUFFERPROC)glGetProcAddress((ProcType)"glIsRenderbufferEXT"); // EXT_framebuffer_object
	if(_glBindRenderbuffer_ptr == nullptr) _glBindRenderbuffer_ptr = (PFNGLBINDRENDERBUFFERPROC)glGetProcAddress((ProcType)"glBindRenderbufferEXT"); // EXT_framebuffer_object
	if(_glDeleteRenderbuffers_ptr == nullptr) _glDeleteRenderbuffers_ptr = (PFNGLDELETERENDERBUFFERSPROC)glGetProcAddress((ProcType)"glDeleteRenderbuffersEXT"); // EXT_framebuffer_object
	if(_glGenRenderbuffers_ptr == nullptr) _glGenRenderbuffers_ptr = (PFNGLGENRENDERBUFFERSPROC)glGetProcAddress((ProcType)"glGenRenderbuffersEXT"); // EXT_framebuffer_object
	if(_glRenderbufferStorage_ptr == nullptr) _glRenderbufferStorage_ptr = (PFNGLRENDERBUFFERSTORAGEPROC)glGetProcAddress((ProcType)"glRenderbufferStorageEXT"); // EXT_framebuffer_object
	if(_glGetRenderbufferParameteriv_ptr == nullptr) _glGetRenderbufferParameteriv_ptr = (PFNGLGETRENDERBUFFERPARAMETERIVPROC)glGetProcAddress((ProcType)"glGetRenderbufferParameterivEXT"); // EXT_framebuffer_object
	if(_glIsFramebuffer_ptr == nullptr) _glIsFramebuffer_ptr = (PFNGLISFRAMEBUFFERPROC)glGetProcAddress((ProcType)"glIsFramebufferEXT"); // EXT_framebuffer_object
	if(_glBindFramebuffer_ptr == nullptr) _glBindFramebuffer_ptr = (PFNGLBINDFRAMEBUFFERPROC)glGetProcAddress((ProcType)"glBindFramebufferEXT"); // EXT_framebuffer_object
	if(_glDeleteFramebuffers_ptr == nullptr) _glDeleteFramebuffers_ptr = (PFNGLDELETEFRAMEBUFFERSPROC)glGetProcAddress((ProcType)"glDeleteFramebuffersEXT"); // EXT_framebuffer_object
	if(_glGenFramebuffers_ptr == nullptr) _glGenFramebuffers_ptr = (PFNGLGENFRAMEBUFFERSPROC)glGetProcAddress((ProcType)"glGenFramebuffersEXT"); // EXT_framebuffer_object
	if(_glCheckFramebufferStatus_ptr == nullptr) _glCheckFramebufferStatus_ptr = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)glGetProcAddress((ProcType)"glCheckFramebufferStatusEXT"); // EXT_framebuffer_object
	if(_glFramebufferTexture1D_ptr == nullptr) _glFramebufferTexture1D_ptr = (PFNGLFRAMEBUFFERTEXTURE1DPROC)glGetProcAddress((ProcType)"glFramebufferTexture1DEXT"); // EXT_framebuffer_object
	if(_glFramebufferTexture2D_ptr == nullptr) _glFramebufferTexture2D_ptr = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glGetProcAddress((ProcType)"glFramebufferTexture2DEXT"); // EXT_framebuffer_object
	if(_glFramebufferTexture3D_ptr == nullptr) _glFramebufferTexture3D_ptr = (PFNGLFRAMEBUFFERTEXTURE3DPROC)glGetProcAddress((ProcType)"glFramebufferTexture3DEXT"); // EXT_framebuffer_object
	if(_glFramebufferRenderbuffer_ptr == nullptr) _glFramebufferRenderbuffer_ptr = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)glGetProcAddress((ProcType)"glFramebufferRenderbufferEXT"); // EXT_framebuffer_object
	if(_glGetFramebufferAttachmentParameteriv_ptr == nullptr) _glGetFramebufferAttachmentParameteriv_ptr = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)glGetProcAddress((ProcType)"glGetFramebufferAttachmentParameterivEXT"); // EXT_framebuffer_object
	if(_glGenerateMipmap_ptr == nullptr) _glGenerateMipmap_ptr = (PFNGLGENERATEMIPMAPPROC)glGetProcAddress((ProcType)"glGenerateMipmapEXT"); // EXT_framebuffer_object
	if(_glBlitFramebuffer_ptr == nullptr) _glBlitFramebuffer_ptr = (PFNGLBLITFRAMEBUFFERPROC)glGetProcAddress((ProcType)"glBlitFramebufferEXT"); // EXT_framebuffer_blit
	
	// check gl function pointers (print error if nullptr)
	if(_glIsRenderbuffer_ptr == nullptr) log_error("couldn't get function pointer to \"glIsRenderbuffer\"!");
	if(_glBindRenderbuffer_ptr == nullptr) log_error("couldn't get function pointer to \"glBindRenderbuffer\"!");
	if(_glDeleteRenderbuffers_ptr == nullptr) log_error("couldn't get function pointer to \"glDeleteRenderbuffers\"!");
	if(_glGenRenderbuffers_ptr == nullptr) log_error("couldn't get function pointer to \"glGenRenderbuffers\"!");
	if(_glRenderbufferStorage_ptr == nullptr) log_error("couldn't get function pointer to \"glRenderbufferStorage\"!");
	if(_glGetRenderbufferParameteriv_ptr == nullptr) log_error("couldn't get function pointer to \"glGetRenderbufferParameteriv\"!");
	if(_glIsFramebuffer_ptr == nullptr) log_error("couldn't get function pointer to \"glIsFramebuffer\"!");
	if(_glBindFramebuffer_ptr == nullptr) log_error("couldn't get function pointer to \"glBindFramebuffer\"!");
	if(_glDeleteFramebuffers_ptr == nullptr) log_error("couldn't get function pointer to \"glDeleteFramebuffers\"!");
	if(_glGenFramebuffers_ptr == nullptr) log_error("couldn't get function pointer to \"glGenFramebuffers\"!");
	if(_glCheckFramebufferStatus_ptr == nullptr) log_error("couldn't get function pointer to \"glCheckFramebufferStatus\"!");
	if(_glFramebufferTexture1D_ptr == nullptr) log_error("couldn't get function pointer to \"glFramebufferTexture1D\"!");
	if(_glFramebufferTexture2D_ptr == nullptr) log_error("couldn't get function pointer to \"glFramebufferTexture2D\"!");
	if(_glFramebufferTexture3D_ptr == nullptr) log_error("couldn't get function pointer to \"glFramebufferTexture3D\"!");
	if(_glFramebufferRenderbuffer_ptr == nullptr) log_error("couldn't get function pointer to \"glFramebufferRenderbuffer\"!");
	if(_glGetFramebufferAttachmentParameteriv_ptr == nullptr) log_error("couldn't get function pointer to \"glGetFramebufferAttachmentParameteriv\"!");
	if(_glGenerateMipmap_ptr == nullptr) log_error("couldn't get function pointer to \"glGenerateMipmap\"!");
	if(_glBlitFramebuffer_ptr == nullptr) log_error("couldn't get function pointer to \"glBlitFramebuffer\"!");
}

#endif
