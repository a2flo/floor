/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#include <floor/core/gl_support.hpp>

#if defined(__APPLE__)
#include <floor/core/logger.hpp>
#if !defined(FLOOR_IOS)
void glCopyImageSubData(GLuint, GLenum, GLint, GLint, GLint, GLint,
						GLuint, GLenum, GLint, GLint, GLint, GLint,
						GLsizei, GLsizei, GLsizei) {
	log_error("glCopyImageSubData is not supported!");
}
#endif
#else // !__APPLE__
gl_api_ptrs gl_api;

#define glGetProcAddress(x) SDL_GL_GetProcAddress(x)

void init_gl_funcs() {
	memset(&gl_api, 0, sizeof(gl_api_ptrs));

#if !defined(__LINUX__)
#if defined(__WINDOWS__) && !defined(MINGW)
	//OpenGL1.0
	gl_api._glCullFace_ptr = (PFNGLCULLFACEPROC)glGetProcAddress("glCullFace");
	gl_api._glFrontFace_ptr = (PFNGLFRONTFACEPROC)glGetProcAddress("glFrontFace");
	gl_api._glHint_ptr = (PFNGLHINTPROC)glGetProcAddress("glHint");
	gl_api._glLineWidth_ptr = (PFNGLLINEWIDTHPROC)glGetProcAddress("glLineWidth");
	gl_api._glPointSize_ptr = (PFNGLPOINTSIZEPROC)glGetProcAddress("glPointSize");
	gl_api._glPolygonMode_ptr = (PFNGLPOLYGONMODEPROC)glGetProcAddress("glPolygonMode");
	gl_api._glScissor_ptr = (PFNGLSCISSORPROC)glGetProcAddress("glScissor");
	gl_api._glTexParameterf_ptr = (PFNGLTEXPARAMETERFPROC)glGetProcAddress("glTexParameterf");
	gl_api._glTexParameterfv_ptr = (PFNGLTEXPARAMETERFVPROC)glGetProcAddress("glTexParameterfv");
	gl_api._glTexParameteri_ptr = (PFNGLTEXPARAMETERIPROC)glGetProcAddress("glTexParameteri");
	gl_api._glTexParameteriv_ptr = (PFNGLTEXPARAMETERIVPROC)glGetProcAddress("glTexParameteriv");
	gl_api._glTexImage1D_ptr = (PFNGLTEXIMAGE1DPROC)glGetProcAddress("glTexImage1D");
	gl_api._glTexImage2D_ptr = (PFNGLTEXIMAGE2DPROC)glGetProcAddress("glTexImage2D");
	gl_api._glDrawBuffer_ptr = (PFNGLDRAWBUFFERPROC)glGetProcAddress("glDrawBuffer");
	gl_api._glClear_ptr = (PFNGLCLEARPROC)glGetProcAddress("glClear");
	gl_api._glClearColor_ptr = (PFNGLCLEARCOLORPROC)glGetProcAddress("glClearColor");
	gl_api._glClearStencil_ptr = (PFNGLCLEARSTENCILPROC)glGetProcAddress("glClearStencil");
	gl_api._glClearDepth_ptr = (PFNGLCLEARDEPTHPROC)glGetProcAddress("glClearDepth");
	gl_api._glStencilMask_ptr = (PFNGLSTENCILMASKPROC)glGetProcAddress("glStencilMask");
	gl_api._glColorMask_ptr = (PFNGLCOLORMASKPROC)glGetProcAddress("glColorMask");
	gl_api._glDepthMask_ptr = (PFNGLDEPTHMASKPROC)glGetProcAddress("glDepthMask");
	gl_api._glDisable_ptr = (PFNGLDISABLEPROC)glGetProcAddress("glDisable");
	gl_api._glEnable_ptr = (PFNGLENABLEPROC)glGetProcAddress("glEnable");
	gl_api._glFinish_ptr = (PFNGLFINISHPROC)glGetProcAddress("glFinish");
	gl_api._glFlush_ptr = (PFNGLFLUSHPROC)glGetProcAddress("glFlush");
	gl_api._glBlendFunc_ptr = (PFNGLBLENDFUNCPROC)glGetProcAddress("glBlendFunc");
	gl_api._glLogicOp_ptr = (PFNGLLOGICOPPROC)glGetProcAddress("glLogicOp");
	gl_api._glStencilFunc_ptr = (PFNGLSTENCILFUNCPROC)glGetProcAddress("glStencilFunc");
	gl_api._glStencilOp_ptr = (PFNGLSTENCILOPPROC)glGetProcAddress("glStencilOp");
	gl_api._glDepthFunc_ptr = (PFNGLDEPTHFUNCPROC)glGetProcAddress("glDepthFunc");
	gl_api._glPixelStoref_ptr = (PFNGLPIXELSTOREFPROC)glGetProcAddress("glPixelStoref");
	gl_api._glPixelStorei_ptr = (PFNGLPIXELSTOREIPROC)glGetProcAddress("glPixelStorei");
	gl_api._glReadBuffer_ptr = (PFNGLREADBUFFERPROC)glGetProcAddress("glReadBuffer");
	gl_api._glReadPixels_ptr = (PFNGLREADPIXELSPROC)glGetProcAddress("glReadPixels");
	gl_api._glGetBooleanv_ptr = (PFNGLGETBOOLEANVPROC)glGetProcAddress("glGetBooleanv");
	gl_api._glGetDoublev_ptr = (PFNGLGETDOUBLEVPROC)glGetProcAddress("glGetDoublev");
	gl_api._glGetError_ptr = (PFNGLGETERRORPROC)glGetProcAddress("glGetError");
	gl_api._glGetFloatv_ptr = (PFNGLGETFLOATVPROC)glGetProcAddress("glGetFloatv");
	gl_api._glGetIntegerv_ptr = (PFNGLGETINTEGERVPROC)glGetProcAddress("glGetIntegerv");
	gl_api._glGetString_ptr = (PFNGLGETSTRINGPROC)glGetProcAddress("glGetString");
	gl_api._glGetTexImage_ptr = (PFNGLGETTEXIMAGEPROC)glGetProcAddress("glGetTexImage");
	gl_api._glGetTexParameterfv_ptr = (PFNGLGETTEXPARAMETERFVPROC)glGetProcAddress("glGetTexParameterfv");
	gl_api._glGetTexParameteriv_ptr = (PFNGLGETTEXPARAMETERIVPROC)glGetProcAddress("glGetTexParameteriv");
	gl_api._glGetTexLevelParameterfv_ptr = (PFNGLGETTEXLEVELPARAMETERFVPROC)glGetProcAddress("glGetTexLevelParameterfv");
	gl_api._glGetTexLevelParameteriv_ptr = (PFNGLGETTEXLEVELPARAMETERIVPROC)glGetProcAddress("glGetTexLevelParameteriv");
	gl_api._glIsEnabled_ptr = (PFNGLISENABLEDPROC)glGetProcAddress("glIsEnabled");
	gl_api._glDepthRange_ptr = (PFNGLDEPTHRANGEPROC)glGetProcAddress("glDepthRange");
	gl_api._glViewport_ptr = (PFNGLVIEWPORTPROC)glGetProcAddress("glViewport");
	
	//OpenGL1.1
	gl_api._glDrawArrays_ptr = (PFNGLDRAWARRAYSPROC)glGetProcAddress("glDrawArrays");
	gl_api._glDrawElements_ptr = (PFNGLDRAWELEMENTSPROC)glGetProcAddress("glDrawElements");
	gl_api._glGetPointerv_ptr = (PFNGLGETPOINTERVPROC)glGetProcAddress("glGetPointerv");
	gl_api._glPolygonOffset_ptr = (PFNGLPOLYGONOFFSETPROC)glGetProcAddress("glPolygonOffset");
	gl_api._glCopyTexImage1D_ptr = (PFNGLCOPYTEXIMAGE1DPROC)glGetProcAddress("glCopyTexImage1D");
	gl_api._glCopyTexImage2D_ptr = (PFNGLCOPYTEXIMAGE2DPROC)glGetProcAddress("glCopyTexImage2D");
	gl_api._glCopyTexSubImage1D_ptr = (PFNGLCOPYTEXSUBIMAGE1DPROC)glGetProcAddress("glCopyTexSubImage1D");
	gl_api._glCopyTexSubImage2D_ptr = (PFNGLCOPYTEXSUBIMAGE2DPROC)glGetProcAddress("glCopyTexSubImage2D");
	gl_api._glTexSubImage1D_ptr = (PFNGLTEXSUBIMAGE1DPROC)glGetProcAddress("glTexSubImage1D");
	gl_api._glTexSubImage2D_ptr = (PFNGLTEXSUBIMAGE2DPROC)glGetProcAddress("glTexSubImage2D");
	gl_api._glBindTexture_ptr = (PFNGLBINDTEXTUREPROC)glGetProcAddress("glBindTexture");
	gl_api._glDeleteTextures_ptr = (PFNGLDELETETEXTURESPROC)glGetProcAddress("glDeleteTextures");
	gl_api._glGenTextures_ptr = (PFNGLGENTEXTURESPROC)glGetProcAddress("glGenTextures");
	gl_api._glIsTexture_ptr = (PFNGLISTEXTUREPROC)glGetProcAddress("glIsTexture");
#endif

	// OpenGL 1.2
	gl_api._glBlendColor_ptr = (PFNGLBLENDCOLORPROC)glGetProcAddress("glBlendColor");
	gl_api._glBlendEquation_ptr = (PFNGLBLENDEQUATIONPROC)glGetProcAddress("glBlendEquation");
	gl_api._glDrawRangeElements_ptr = (PFNGLDRAWRANGEELEMENTSPROC)glGetProcAddress("glDrawRangeElements");
	gl_api._glTexImage3D_ptr = (PFNGLTEXIMAGE3DPROC)glGetProcAddress("glTexImage3D");
	gl_api._glTexSubImage3D_ptr = (PFNGLTEXSUBIMAGE3DPROC)glGetProcAddress("glTexSubImage3D");
	gl_api._glCopyTexSubImage3D_ptr = (PFNGLCOPYTEXSUBIMAGE3DPROC)glGetProcAddress("glCopyTexSubImage3D");
	
	// OpenGL 1.3
	gl_api._glActiveTexture_ptr = (PFNGLACTIVETEXTUREPROC)glGetProcAddress("glActiveTexture");
	gl_api._glSampleCoverage_ptr = (PFNGLSAMPLECOVERAGEPROC)glGetProcAddress("glSampleCoverage");
	gl_api._glCompressedTexImage3D_ptr = (PFNGLCOMPRESSEDTEXIMAGE3DPROC)glGetProcAddress("glCompressedTexImage3D");
	gl_api._glCompressedTexImage2D_ptr = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)glGetProcAddress("glCompressedTexImage2D");
	gl_api._glCompressedTexImage1D_ptr = (PFNGLCOMPRESSEDTEXIMAGE1DPROC)glGetProcAddress("glCompressedTexImage1D");
	gl_api._glCompressedTexSubImage3D_ptr = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)glGetProcAddress("glCompressedTexSubImage3D");
	gl_api._glCompressedTexSubImage2D_ptr = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)glGetProcAddress("glCompressedTexSubImage2D");
	gl_api._glCompressedTexSubImage1D_ptr = (PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC)glGetProcAddress("glCompressedTexSubImage1D");
	gl_api._glGetCompressedTexImage_ptr = (PFNGLGETCOMPRESSEDTEXIMAGEPROC)glGetProcAddress("glGetCompressedTexImage");
#endif
	
	// OpenGL 1.4
	gl_api._glBlendFuncSeparate_ptr = (PFNGLBLENDFUNCSEPARATEPROC)glGetProcAddress("glBlendFuncSeparate");
	gl_api._glMultiDrawArrays_ptr = (PFNGLMULTIDRAWARRAYSPROC)glGetProcAddress("glMultiDrawArrays");
	gl_api._glMultiDrawElements_ptr = (PFNGLMULTIDRAWELEMENTSPROC)glGetProcAddress("glMultiDrawElements");
	gl_api._glPointParameterf_ptr = (PFNGLPOINTPARAMETERFPROC)glGetProcAddress("glPointParameterf");
	gl_api._glPointParameterfv_ptr = (PFNGLPOINTPARAMETERFVPROC)glGetProcAddress("glPointParameterfv");
	gl_api._glPointParameteri_ptr = (PFNGLPOINTPARAMETERIPROC)glGetProcAddress("glPointParameteri");
	gl_api._glPointParameteriv_ptr = (PFNGLPOINTPARAMETERIVPROC)glGetProcAddress("glPointParameteriv");
	
	// OpenGL 1.5
	gl_api._glGenQueries_ptr = (PFNGLGENQUERIESPROC)glGetProcAddress("glGenQueries");
	gl_api._glDeleteQueries_ptr = (PFNGLDELETEQUERIESPROC)glGetProcAddress("glDeleteQueries");
	gl_api._glIsQuery_ptr = (PFNGLISQUERYPROC)glGetProcAddress("glIsQuery");
	gl_api._glBeginQuery_ptr = (PFNGLBEGINQUERYPROC)glGetProcAddress("glBeginQuery");
	gl_api._glEndQuery_ptr = (PFNGLENDQUERYPROC)glGetProcAddress("glEndQuery");
	gl_api._glGetQueryiv_ptr = (PFNGLGETQUERYIVPROC)glGetProcAddress("glGetQueryiv");
	gl_api._glGetQueryObjectiv_ptr = (PFNGLGETQUERYOBJECTIVPROC)glGetProcAddress("glGetQueryObjectiv");
	gl_api._glGetQueryObjectuiv_ptr = (PFNGLGETQUERYOBJECTUIVPROC)glGetProcAddress("glGetQueryObjectuiv");
	gl_api._glBindBuffer_ptr = (PFNGLBINDBUFFERPROC)glGetProcAddress("glBindBuffer");
	gl_api._glDeleteBuffers_ptr = (PFNGLDELETEBUFFERSPROC)glGetProcAddress("glDeleteBuffers");
	gl_api._glGenBuffers_ptr = (PFNGLGENBUFFERSPROC)glGetProcAddress("glGenBuffers");
	gl_api._glIsBuffer_ptr = (PFNGLISBUFFERPROC)glGetProcAddress("glIsBuffer");
	gl_api._glBufferData_ptr = (PFNGLBUFFERDATAPROC)glGetProcAddress("glBufferData");
	gl_api._glBufferSubData_ptr = (PFNGLBUFFERSUBDATAPROC)glGetProcAddress("glBufferSubData");
	gl_api._glGetBufferSubData_ptr = (PFNGLGETBUFFERSUBDATAPROC)glGetProcAddress("glGetBufferSubData");
	gl_api._glMapBuffer_ptr = (PFNGLMAPBUFFERPROC)glGetProcAddress("glMapBuffer");
	gl_api._glUnmapBuffer_ptr = (PFNGLUNMAPBUFFERPROC)glGetProcAddress("glUnmapBuffer");
	gl_api._glGetBufferParameteriv_ptr = (PFNGLGETBUFFERPARAMETERIVPROC)glGetProcAddress("glGetBufferParameteriv");
	gl_api._glGetBufferPointerv_ptr = (PFNGLGETBUFFERPOINTERVPROC)glGetProcAddress("glGetBufferPointerv");
	
	// OpenGL 2.0
	gl_api._glBlendEquationSeparate_ptr = (PFNGLBLENDEQUATIONSEPARATEPROC)glGetProcAddress("glBlendEquationSeparate");
	gl_api._glDrawBuffers_ptr = (PFNGLDRAWBUFFERSPROC)glGetProcAddress("glDrawBuffers");
	gl_api._glStencilOpSeparate_ptr = (PFNGLSTENCILOPSEPARATEPROC)glGetProcAddress("glStencilOpSeparate");
	gl_api._glStencilFuncSeparate_ptr = (PFNGLSTENCILFUNCSEPARATEPROC)glGetProcAddress("glStencilFuncSeparate");
	gl_api._glStencilMaskSeparate_ptr = (PFNGLSTENCILMASKSEPARATEPROC)glGetProcAddress("glStencilMaskSeparate");
	gl_api._glAttachShader_ptr = (PFNGLATTACHSHADERPROC)glGetProcAddress("glAttachShader");
	gl_api._glBindAttribLocation_ptr = (PFNGLBINDATTRIBLOCATIONPROC)glGetProcAddress("glBindAttribLocation");
	gl_api._glCompileShader_ptr = (PFNGLCOMPILESHADERPROC)glGetProcAddress("glCompileShader");
	gl_api._glCreateProgram_ptr = (PFNGLCREATEPROGRAMPROC)glGetProcAddress("glCreateProgram");
	gl_api._glCreateShader_ptr = (PFNGLCREATESHADERPROC)glGetProcAddress("glCreateShader");
	gl_api._glDeleteProgram_ptr = (PFNGLDELETEPROGRAMPROC)glGetProcAddress("glDeleteProgram");
	gl_api._glDeleteShader_ptr = (PFNGLDELETESHADERPROC)glGetProcAddress("glDeleteShader");
	gl_api._glDetachShader_ptr = (PFNGLDETACHSHADERPROC)glGetProcAddress("glDetachShader");
	gl_api._glDisableVertexAttribArray_ptr = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)glGetProcAddress("glDisableVertexAttribArray");
	gl_api._glEnableVertexAttribArray_ptr = (PFNGLENABLEVERTEXATTRIBARRAYPROC)glGetProcAddress("glEnableVertexAttribArray");
	gl_api._glGetActiveAttrib_ptr = (PFNGLGETACTIVEATTRIBPROC)glGetProcAddress("glGetActiveAttrib");
	gl_api._glGetActiveUniform_ptr = (PFNGLGETACTIVEUNIFORMPROC)glGetProcAddress("glGetActiveUniform");
	gl_api._glGetAttachedShaders_ptr = (PFNGLGETATTACHEDSHADERSPROC)glGetProcAddress("glGetAttachedShaders");
	gl_api._glGetAttribLocation_ptr = (PFNGLGETATTRIBLOCATIONPROC)glGetProcAddress("glGetAttribLocation");
	gl_api._glGetProgramiv_ptr = (PFNGLGETPROGRAMIVPROC)glGetProcAddress("glGetProgramiv");
	gl_api._glGetProgramInfoLog_ptr = (PFNGLGETPROGRAMINFOLOGPROC)glGetProcAddress("glGetProgramInfoLog");
	gl_api._glGetShaderiv_ptr = (PFNGLGETSHADERIVPROC)glGetProcAddress("glGetShaderiv");
	gl_api._glGetShaderInfoLog_ptr = (PFNGLGETSHADERINFOLOGPROC)glGetProcAddress("glGetShaderInfoLog");
	gl_api._glGetShaderSource_ptr = (PFNGLGETSHADERSOURCEPROC)glGetProcAddress("glGetShaderSource");
	gl_api._glGetUniformLocation_ptr = (PFNGLGETUNIFORMLOCATIONPROC)glGetProcAddress("glGetUniformLocation");
	gl_api._glGetUniformfv_ptr = (PFNGLGETUNIFORMFVPROC)glGetProcAddress("glGetUniformfv");
	gl_api._glGetUniformiv_ptr = (PFNGLGETUNIFORMIVPROC)glGetProcAddress("glGetUniformiv");
	gl_api._glGetVertexAttribdv_ptr = (PFNGLGETVERTEXATTRIBDVPROC)glGetProcAddress("glGetVertexAttribdv");
	gl_api._glGetVertexAttribfv_ptr = (PFNGLGETVERTEXATTRIBFVPROC)glGetProcAddress("glGetVertexAttribfv");
	gl_api._glGetVertexAttribiv_ptr = (PFNGLGETVERTEXATTRIBIVPROC)glGetProcAddress("glGetVertexAttribiv");
	gl_api._glGetVertexAttribPointerv_ptr = (PFNGLGETVERTEXATTRIBPOINTERVPROC)glGetProcAddress("glGetVertexAttribPointerv");
	gl_api._glIsProgram_ptr = (PFNGLISPROGRAMPROC)glGetProcAddress("glIsProgram");
	gl_api._glIsShader_ptr = (PFNGLISSHADERPROC)glGetProcAddress("glIsShader");
	gl_api._glLinkProgram_ptr = (PFNGLLINKPROGRAMPROC)glGetProcAddress("glLinkProgram");
	gl_api._glShaderSource_ptr = (PFNGLSHADERSOURCEPROC)glGetProcAddress("glShaderSource");
	gl_api._glUseProgram_ptr = (PFNGLUSEPROGRAMPROC)glGetProcAddress("glUseProgram");
	gl_api._glUniform1f_ptr = (PFNGLUNIFORM1FPROC)glGetProcAddress("glUniform1f");
	gl_api._glUniform2f_ptr = (PFNGLUNIFORM2FPROC)glGetProcAddress("glUniform2f");
	gl_api._glUniform3f_ptr = (PFNGLUNIFORM3FPROC)glGetProcAddress("glUniform3f");
	gl_api._glUniform4f_ptr = (PFNGLUNIFORM4FPROC)glGetProcAddress("glUniform4f");
	gl_api._glUniform1i_ptr = (PFNGLUNIFORM1IPROC)glGetProcAddress("glUniform1i");
	gl_api._glUniform2i_ptr = (PFNGLUNIFORM2IPROC)glGetProcAddress("glUniform2i");
	gl_api._glUniform3i_ptr = (PFNGLUNIFORM3IPROC)glGetProcAddress("glUniform3i");
	gl_api._glUniform4i_ptr = (PFNGLUNIFORM4IPROC)glGetProcAddress("glUniform4i");
	gl_api._glUniform1fv_ptr = (PFNGLUNIFORM1FVPROC)glGetProcAddress("glUniform1fv");
	gl_api._glUniform2fv_ptr = (PFNGLUNIFORM2FVPROC)glGetProcAddress("glUniform2fv");
	gl_api._glUniform3fv_ptr = (PFNGLUNIFORM3FVPROC)glGetProcAddress("glUniform3fv");
	gl_api._glUniform4fv_ptr = (PFNGLUNIFORM4FVPROC)glGetProcAddress("glUniform4fv");
	gl_api._glUniform1iv_ptr = (PFNGLUNIFORM1IVPROC)glGetProcAddress("glUniform1iv");
	gl_api._glUniform2iv_ptr = (PFNGLUNIFORM2IVPROC)glGetProcAddress("glUniform2iv");
	gl_api._glUniform3iv_ptr = (PFNGLUNIFORM3IVPROC)glGetProcAddress("glUniform3iv");
	gl_api._glUniform4iv_ptr = (PFNGLUNIFORM4IVPROC)glGetProcAddress("glUniform4iv");
	gl_api._glUniformMatrix2fv_ptr = (PFNGLUNIFORMMATRIX2FVPROC)glGetProcAddress("glUniformMatrix2fv");
	gl_api._glUniformMatrix3fv_ptr = (PFNGLUNIFORMMATRIX3FVPROC)glGetProcAddress("glUniformMatrix3fv");
	gl_api._glUniformMatrix4fv_ptr = (PFNGLUNIFORMMATRIX4FVPROC)glGetProcAddress("glUniformMatrix4fv");
	gl_api._glValidateProgram_ptr = (PFNGLVALIDATEPROGRAMPROC)glGetProcAddress("glValidateProgram");
	gl_api._glVertexAttrib1d_ptr = (PFNGLVERTEXATTRIB1DPROC)glGetProcAddress("glVertexAttrib1d");
	gl_api._glVertexAttrib1dv_ptr = (PFNGLVERTEXATTRIB1DVPROC)glGetProcAddress("glVertexAttrib1dv");
	gl_api._glVertexAttrib1f_ptr = (PFNGLVERTEXATTRIB1FPROC)glGetProcAddress("glVertexAttrib1f");
	gl_api._glVertexAttrib1fv_ptr = (PFNGLVERTEXATTRIB1FVPROC)glGetProcAddress("glVertexAttrib1fv");
	gl_api._glVertexAttrib1s_ptr = (PFNGLVERTEXATTRIB1SPROC)glGetProcAddress("glVertexAttrib1s");
	gl_api._glVertexAttrib1sv_ptr = (PFNGLVERTEXATTRIB1SVPROC)glGetProcAddress("glVertexAttrib1sv");
	gl_api._glVertexAttrib2d_ptr = (PFNGLVERTEXATTRIB2DPROC)glGetProcAddress("glVertexAttrib2d");
	gl_api._glVertexAttrib2dv_ptr = (PFNGLVERTEXATTRIB2DVPROC)glGetProcAddress("glVertexAttrib2dv");
	gl_api._glVertexAttrib2f_ptr = (PFNGLVERTEXATTRIB2FPROC)glGetProcAddress("glVertexAttrib2f");
	gl_api._glVertexAttrib2fv_ptr = (PFNGLVERTEXATTRIB2FVPROC)glGetProcAddress("glVertexAttrib2fv");
	gl_api._glVertexAttrib2s_ptr = (PFNGLVERTEXATTRIB2SPROC)glGetProcAddress("glVertexAttrib2s");
	gl_api._glVertexAttrib2sv_ptr = (PFNGLVERTEXATTRIB2SVPROC)glGetProcAddress("glVertexAttrib2sv");
	gl_api._glVertexAttrib3d_ptr = (PFNGLVERTEXATTRIB3DPROC)glGetProcAddress("glVertexAttrib3d");
	gl_api._glVertexAttrib3dv_ptr = (PFNGLVERTEXATTRIB3DVPROC)glGetProcAddress("glVertexAttrib3dv");
	gl_api._glVertexAttrib3f_ptr = (PFNGLVERTEXATTRIB3FPROC)glGetProcAddress("glVertexAttrib3f");
	gl_api._glVertexAttrib3fv_ptr = (PFNGLVERTEXATTRIB3FVPROC)glGetProcAddress("glVertexAttrib3fv");
	gl_api._glVertexAttrib3s_ptr = (PFNGLVERTEXATTRIB3SPROC)glGetProcAddress("glVertexAttrib3s");
	gl_api._glVertexAttrib3sv_ptr = (PFNGLVERTEXATTRIB3SVPROC)glGetProcAddress("glVertexAttrib3sv");
	gl_api._glVertexAttrib4Nbv_ptr = (PFNGLVERTEXATTRIB4NBVPROC)glGetProcAddress("glVertexAttrib4Nbv");
	gl_api._glVertexAttrib4Niv_ptr = (PFNGLVERTEXATTRIB4NIVPROC)glGetProcAddress("glVertexAttrib4Niv");
	gl_api._glVertexAttrib4Nsv_ptr = (PFNGLVERTEXATTRIB4NSVPROC)glGetProcAddress("glVertexAttrib4Nsv");
	gl_api._glVertexAttrib4Nub_ptr = (PFNGLVERTEXATTRIB4NUBPROC)glGetProcAddress("glVertexAttrib4Nub");
	gl_api._glVertexAttrib4Nubv_ptr = (PFNGLVERTEXATTRIB4NUBVPROC)glGetProcAddress("glVertexAttrib4Nubv");
	gl_api._glVertexAttrib4Nuiv_ptr = (PFNGLVERTEXATTRIB4NUIVPROC)glGetProcAddress("glVertexAttrib4Nuiv");
	gl_api._glVertexAttrib4Nusv_ptr = (PFNGLVERTEXATTRIB4NUSVPROC)glGetProcAddress("glVertexAttrib4Nusv");
	gl_api._glVertexAttrib4bv_ptr = (PFNGLVERTEXATTRIB4BVPROC)glGetProcAddress("glVertexAttrib4bv");
	gl_api._glVertexAttrib4d_ptr = (PFNGLVERTEXATTRIB4DPROC)glGetProcAddress("glVertexAttrib4d");
	gl_api._glVertexAttrib4dv_ptr = (PFNGLVERTEXATTRIB4DVPROC)glGetProcAddress("glVertexAttrib4dv");
	gl_api._glVertexAttrib4f_ptr = (PFNGLVERTEXATTRIB4FPROC)glGetProcAddress("glVertexAttrib4f");
	gl_api._glVertexAttrib4fv_ptr = (PFNGLVERTEXATTRIB4FVPROC)glGetProcAddress("glVertexAttrib4fv");
	gl_api._glVertexAttrib4iv_ptr = (PFNGLVERTEXATTRIB4IVPROC)glGetProcAddress("glVertexAttrib4iv");
	gl_api._glVertexAttrib4s_ptr = (PFNGLVERTEXATTRIB4SPROC)glGetProcAddress("glVertexAttrib4s");
	gl_api._glVertexAttrib4sv_ptr = (PFNGLVERTEXATTRIB4SVPROC)glGetProcAddress("glVertexAttrib4sv");
	gl_api._glVertexAttrib4ubv_ptr = (PFNGLVERTEXATTRIB4UBVPROC)glGetProcAddress("glVertexAttrib4ubv");
	gl_api._glVertexAttrib4uiv_ptr = (PFNGLVERTEXATTRIB4UIVPROC)glGetProcAddress("glVertexAttrib4uiv");
	gl_api._glVertexAttrib4usv_ptr = (PFNGLVERTEXATTRIB4USVPROC)glGetProcAddress("glVertexAttrib4usv");
	gl_api._glVertexAttribPointer_ptr = (PFNGLVERTEXATTRIBPOINTERPROC)glGetProcAddress("glVertexAttribPointer");
	
	// OpenGL 2.1
	gl_api._glUniformMatrix2x3fv_ptr = (PFNGLUNIFORMMATRIX2X3FVPROC)glGetProcAddress("glUniformMatrix2x3fv");
	gl_api._glUniformMatrix3x2fv_ptr = (PFNGLUNIFORMMATRIX3X2FVPROC)glGetProcAddress("glUniformMatrix3x2fv");
	gl_api._glUniformMatrix2x4fv_ptr = (PFNGLUNIFORMMATRIX2X4FVPROC)glGetProcAddress("glUniformMatrix2x4fv");
	gl_api._glUniformMatrix4x2fv_ptr = (PFNGLUNIFORMMATRIX4X2FVPROC)glGetProcAddress("glUniformMatrix4x2fv");
	gl_api._glUniformMatrix3x4fv_ptr = (PFNGLUNIFORMMATRIX3X4FVPROC)glGetProcAddress("glUniformMatrix3x4fv");
	gl_api._glUniformMatrix4x3fv_ptr = (PFNGLUNIFORMMATRIX4X3FVPROC)glGetProcAddress("glUniformMatrix4x3fv");
	
	// OpenGL 3.0
	gl_api._glColorMaski_ptr = (PFNGLCOLORMASKIPROC)glGetProcAddress("glColorMaski");
	gl_api._glGetBooleani_v_ptr = (PFNGLGETBOOLEANI_VPROC)glGetProcAddress("glGetBooleani_v");
	gl_api._glGetIntegeri_v_ptr = (PFNGLGETINTEGERI_VPROC)glGetProcAddress("glGetIntegeri_v");
	gl_api._glEnablei_ptr = (PFNGLENABLEIPROC)glGetProcAddress("glEnablei");
	gl_api._glDisablei_ptr = (PFNGLDISABLEIPROC)glGetProcAddress("glDisablei");
	gl_api._glIsEnabledi_ptr = (PFNGLISENABLEDIPROC)glGetProcAddress("glIsEnabledi");
	gl_api._glBeginTransformFeedback_ptr = (PFNGLBEGINTRANSFORMFEEDBACKPROC)glGetProcAddress("glBeginTransformFeedback");
	gl_api._glEndTransformFeedback_ptr = (PFNGLENDTRANSFORMFEEDBACKPROC)glGetProcAddress("glEndTransformFeedback");
	gl_api._glBindBufferRange_ptr = (PFNGLBINDBUFFERRANGEPROC)glGetProcAddress("glBindBufferRange");
	gl_api._glBindBufferBase_ptr = (PFNGLBINDBUFFERBASEPROC)glGetProcAddress("glBindBufferBase");
	gl_api._glTransformFeedbackVaryings_ptr = (PFNGLTRANSFORMFEEDBACKVARYINGSPROC)glGetProcAddress("glTransformFeedbackVaryings");
	gl_api._glGetTransformFeedbackVarying_ptr = (PFNGLGETTRANSFORMFEEDBACKVARYINGPROC)glGetProcAddress("glGetTransformFeedbackVarying");
	gl_api._glClampColor_ptr = (PFNGLCLAMPCOLORPROC)glGetProcAddress("glClampColor");
	gl_api._glBeginConditionalRender_ptr = (PFNGLBEGINCONDITIONALRENDERPROC)glGetProcAddress("glBeginConditionalRender");
	gl_api._glEndConditionalRender_ptr = (PFNGLENDCONDITIONALRENDERPROC)glGetProcAddress("glEndConditionalRender");
	gl_api._glVertexAttribIPointer_ptr = (PFNGLVERTEXATTRIBIPOINTERPROC)glGetProcAddress("glVertexAttribIPointer");
	gl_api._glGetVertexAttribIiv_ptr = (PFNGLGETVERTEXATTRIBIIVPROC)glGetProcAddress("glGetVertexAttribIiv");
	gl_api._glGetVertexAttribIuiv_ptr = (PFNGLGETVERTEXATTRIBIUIVPROC)glGetProcAddress("glGetVertexAttribIuiv");
	gl_api._glVertexAttribI1i_ptr = (PFNGLVERTEXATTRIBI1IPROC)glGetProcAddress("glVertexAttribI1i");
	gl_api._glVertexAttribI2i_ptr = (PFNGLVERTEXATTRIBI2IPROC)glGetProcAddress("glVertexAttribI2i");
	gl_api._glVertexAttribI3i_ptr = (PFNGLVERTEXATTRIBI3IPROC)glGetProcAddress("glVertexAttribI3i");
	gl_api._glVertexAttribI4i_ptr = (PFNGLVERTEXATTRIBI4IPROC)glGetProcAddress("glVertexAttribI4i");
	gl_api._glVertexAttribI1ui_ptr = (PFNGLVERTEXATTRIBI1UIPROC)glGetProcAddress("glVertexAttribI1ui");
	gl_api._glVertexAttribI2ui_ptr = (PFNGLVERTEXATTRIBI2UIPROC)glGetProcAddress("glVertexAttribI2ui");
	gl_api._glVertexAttribI3ui_ptr = (PFNGLVERTEXATTRIBI3UIPROC)glGetProcAddress("glVertexAttribI3ui");
	gl_api._glVertexAttribI4ui_ptr = (PFNGLVERTEXATTRIBI4UIPROC)glGetProcAddress("glVertexAttribI4ui");
	gl_api._glVertexAttribI1iv_ptr = (PFNGLVERTEXATTRIBI1IVPROC)glGetProcAddress("glVertexAttribI1iv");
	gl_api._glVertexAttribI2iv_ptr = (PFNGLVERTEXATTRIBI2IVPROC)glGetProcAddress("glVertexAttribI2iv");
	gl_api._glVertexAttribI3iv_ptr = (PFNGLVERTEXATTRIBI3IVPROC)glGetProcAddress("glVertexAttribI3iv");
	gl_api._glVertexAttribI4iv_ptr = (PFNGLVERTEXATTRIBI4IVPROC)glGetProcAddress("glVertexAttribI4iv");
	gl_api._glVertexAttribI1uiv_ptr = (PFNGLVERTEXATTRIBI1UIVPROC)glGetProcAddress("glVertexAttribI1uiv");
	gl_api._glVertexAttribI2uiv_ptr = (PFNGLVERTEXATTRIBI2UIVPROC)glGetProcAddress("glVertexAttribI2uiv");
	gl_api._glVertexAttribI3uiv_ptr = (PFNGLVERTEXATTRIBI3UIVPROC)glGetProcAddress("glVertexAttribI3uiv");
	gl_api._glVertexAttribI4uiv_ptr = (PFNGLVERTEXATTRIBI4UIVPROC)glGetProcAddress("glVertexAttribI4uiv");
	gl_api._glVertexAttribI4bv_ptr = (PFNGLVERTEXATTRIBI4BVPROC)glGetProcAddress("glVertexAttribI4bv");
	gl_api._glVertexAttribI4sv_ptr = (PFNGLVERTEXATTRIBI4SVPROC)glGetProcAddress("glVertexAttribI4sv");
	gl_api._glVertexAttribI4ubv_ptr = (PFNGLVERTEXATTRIBI4UBVPROC)glGetProcAddress("glVertexAttribI4ubv");
	gl_api._glVertexAttribI4usv_ptr = (PFNGLVERTEXATTRIBI4USVPROC)glGetProcAddress("glVertexAttribI4usv");
	gl_api._glGetUniformuiv_ptr = (PFNGLGETUNIFORMUIVPROC)glGetProcAddress("glGetUniformuiv");
	gl_api._glBindFragDataLocation_ptr = (PFNGLBINDFRAGDATALOCATIONPROC)glGetProcAddress("glBindFragDataLocation");
	gl_api._glGetFragDataLocation_ptr = (PFNGLGETFRAGDATALOCATIONPROC)glGetProcAddress("glGetFragDataLocation");
	gl_api._glUniform1ui_ptr = (PFNGLUNIFORM1UIPROC)glGetProcAddress("glUniform1ui");
	gl_api._glUniform2ui_ptr = (PFNGLUNIFORM2UIPROC)glGetProcAddress("glUniform2ui");
	gl_api._glUniform3ui_ptr = (PFNGLUNIFORM3UIPROC)glGetProcAddress("glUniform3ui");
	gl_api._glUniform4ui_ptr = (PFNGLUNIFORM4UIPROC)glGetProcAddress("glUniform4ui");
	gl_api._glUniform1uiv_ptr = (PFNGLUNIFORM1UIVPROC)glGetProcAddress("glUniform1uiv");
	gl_api._glUniform2uiv_ptr = (PFNGLUNIFORM2UIVPROC)glGetProcAddress("glUniform2uiv");
	gl_api._glUniform3uiv_ptr = (PFNGLUNIFORM3UIVPROC)glGetProcAddress("glUniform3uiv");
	gl_api._glUniform4uiv_ptr = (PFNGLUNIFORM4UIVPROC)glGetProcAddress("glUniform4uiv");
	gl_api._glTexParameterIiv_ptr = (PFNGLTEXPARAMETERIIVPROC)glGetProcAddress("glTexParameterIiv");
	gl_api._glTexParameterIuiv_ptr = (PFNGLTEXPARAMETERIUIVPROC)glGetProcAddress("glTexParameterIuiv");
	gl_api._glGetTexParameterIiv_ptr = (PFNGLGETTEXPARAMETERIIVPROC)glGetProcAddress("glGetTexParameterIiv");
	gl_api._glGetTexParameterIuiv_ptr = (PFNGLGETTEXPARAMETERIUIVPROC)glGetProcAddress("glGetTexParameterIuiv");
	gl_api._glClearBufferiv_ptr = (PFNGLCLEARBUFFERIVPROC)glGetProcAddress("glClearBufferiv");
	gl_api._glClearBufferuiv_ptr = (PFNGLCLEARBUFFERUIVPROC)glGetProcAddress("glClearBufferuiv");
	gl_api._glClearBufferfv_ptr = (PFNGLCLEARBUFFERFVPROC)glGetProcAddress("glClearBufferfv");
	gl_api._glClearBufferfi_ptr = (PFNGLCLEARBUFFERFIPROC)glGetProcAddress("glClearBufferfi");
	gl_api._glGetStringi_ptr = (PFNGLGETSTRINGIPROC)glGetProcAddress("glGetStringi");
	gl_api._glIsRenderbuffer_ptr = (PFNGLISRENDERBUFFERPROC)glGetProcAddress("glIsRenderbuffer"); // ARB_framebuffer_object
	gl_api._glBindRenderbuffer_ptr = (PFNGLBINDRENDERBUFFERPROC)glGetProcAddress("glBindRenderbuffer"); // ARB_framebuffer_object
	gl_api._glDeleteRenderbuffers_ptr = (PFNGLDELETERENDERBUFFERSPROC)glGetProcAddress("glDeleteRenderbuffers"); // ARB_framebuffer_object
	gl_api._glGenRenderbuffers_ptr = (PFNGLGENRENDERBUFFERSPROC)glGetProcAddress("glGenRenderbuffers"); // ARB_framebuffer_object
	gl_api._glRenderbufferStorage_ptr = (PFNGLRENDERBUFFERSTORAGEPROC)glGetProcAddress("glRenderbufferStorage"); // ARB_framebuffer_object
	gl_api._glGetRenderbufferParameteriv_ptr = (PFNGLGETRENDERBUFFERPARAMETERIVPROC)glGetProcAddress("glGetRenderbufferParameteriv"); // ARB_framebuffer_object
	gl_api._glIsFramebuffer_ptr = (PFNGLISFRAMEBUFFERPROC)glGetProcAddress("glIsFramebuffer"); // ARB_framebuffer_object
	gl_api._glBindFramebuffer_ptr = (PFNGLBINDFRAMEBUFFERPROC)glGetProcAddress("glBindFramebuffer"); // ARB_framebuffer_object
	gl_api._glDeleteFramebuffers_ptr = (PFNGLDELETEFRAMEBUFFERSPROC)glGetProcAddress("glDeleteFramebuffers"); // ARB_framebuffer_object
	gl_api._glGenFramebuffers_ptr = (PFNGLGENFRAMEBUFFERSPROC)glGetProcAddress("glGenFramebuffers"); // ARB_framebuffer_object
	gl_api._glCheckFramebufferStatus_ptr = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)glGetProcAddress("glCheckFramebufferStatus"); // ARB_framebuffer_object
	gl_api._glFramebufferTexture1D_ptr = (PFNGLFRAMEBUFFERTEXTURE1DPROC)glGetProcAddress("glFramebufferTexture1D"); // ARB_framebuffer_object
	gl_api._glFramebufferTexture2D_ptr = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glGetProcAddress("glFramebufferTexture2D"); // ARB_framebuffer_object
	gl_api._glFramebufferTexture3D_ptr = (PFNGLFRAMEBUFFERTEXTURE3DPROC)glGetProcAddress("glFramebufferTexture3D"); // ARB_framebuffer_object
	gl_api._glFramebufferRenderbuffer_ptr = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)glGetProcAddress("glFramebufferRenderbuffer"); // ARB_framebuffer_object
	gl_api._glGetFramebufferAttachmentParameteriv_ptr = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)glGetProcAddress("glGetFramebufferAttachmentParameteriv"); // ARB_framebuffer_object
	gl_api._glGenerateMipmap_ptr = (PFNGLGENERATEMIPMAPPROC)glGetProcAddress("glGenerateMipmap"); // ARB_framebuffer_object
	gl_api._glBlitFramebuffer_ptr = (PFNGLBLITFRAMEBUFFERPROC)glGetProcAddress("glBlitFramebuffer"); // ARB_framebuffer_object
	gl_api._glRenderbufferStorageMultisample_ptr = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)glGetProcAddress("glRenderbufferStorageMultisample"); // ARB_framebuffer_object
	gl_api._glFramebufferTextureLayer_ptr = (PFNGLFRAMEBUFFERTEXTURELAYERPROC)glGetProcAddress("glFramebufferTextureLayer"); // ARB_framebuffer_object
	gl_api._glMapBufferRange_ptr = (PFNGLMAPBUFFERRANGEPROC)glGetProcAddress("glMapBufferRange"); // ARB_map_buffer_range
	gl_api._glFlushMappedBufferRange_ptr = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC)glGetProcAddress("glFlushMappedBufferRange"); // ARB_map_buffer_range
	gl_api._glBindVertexArray_ptr = (PFNGLBINDVERTEXARRAYPROC)glGetProcAddress("glBindVertexArray"); // ARB_vertex_array_object
	gl_api._glDeleteVertexArrays_ptr = (PFNGLDELETEVERTEXARRAYSPROC)glGetProcAddress("glDeleteVertexArrays"); // ARB_vertex_array_object
	gl_api._glGenVertexArrays_ptr = (PFNGLGENVERTEXARRAYSPROC)glGetProcAddress("glGenVertexArrays"); // ARB_vertex_array_object
	gl_api._glIsVertexArray_ptr = (PFNGLISVERTEXARRAYPROC)glGetProcAddress("glIsVertexArray"); // ARB_vertex_array_object
	
	// OpenGL 3.1
	gl_api._glDrawArraysInstanced_ptr = (PFNGLDRAWARRAYSINSTANCEDPROC)glGetProcAddress("glDrawArraysInstanced");
	gl_api._glDrawElementsInstanced_ptr = (PFNGLDRAWELEMENTSINSTANCEDPROC)glGetProcAddress("glDrawElementsInstanced");
	gl_api._glTexBuffer_ptr = (PFNGLTEXBUFFERPROC)glGetProcAddress("glTexBuffer");
	gl_api._glPrimitiveRestartIndex_ptr = (PFNGLPRIMITIVERESTARTINDEXPROC)glGetProcAddress("glPrimitiveRestartIndex");
	gl_api._glCopyBufferSubData_ptr = (PFNGLCOPYBUFFERSUBDATAPROC)glGetProcAddress("glCopyBufferSubData"); // ARB_copy_buffer
	gl_api._glGetUniformIndices_ptr = (PFNGLGETUNIFORMINDICESPROC)glGetProcAddress("glGetUniformIndices"); // ARB_uniform_buffer_object
	gl_api._glGetActiveUniformsiv_ptr = (PFNGLGETACTIVEUNIFORMSIVPROC)glGetProcAddress("glGetActiveUniformsiv"); // ARB_uniform_buffer_object
	gl_api._glGetActiveUniformName_ptr = (PFNGLGETACTIVEUNIFORMNAMEPROC)glGetProcAddress("glGetActiveUniformName"); // ARB_uniform_buffer_object
	gl_api._glGetUniformBlockIndex_ptr = (PFNGLGETUNIFORMBLOCKINDEXPROC)glGetProcAddress("glGetUniformBlockIndex"); // ARB_uniform_buffer_object
	gl_api._glGetActiveUniformBlockiv_ptr = (PFNGLGETACTIVEUNIFORMBLOCKIVPROC)glGetProcAddress("glGetActiveUniformBlockiv"); // ARB_uniform_buffer_object
	gl_api._glGetActiveUniformBlockName_ptr = (PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC)glGetProcAddress("glGetActiveUniformBlockName"); // ARB_uniform_buffer_object
	gl_api._glUniformBlockBinding_ptr = (PFNGLUNIFORMBLOCKBINDINGPROC)glGetProcAddress("glUniformBlockBinding"); // ARB_uniform_buffer_object
	
	// OpenGL 3.2
	gl_api._glGetInteger64i_v_ptr = (PFNGLGETINTEGER64I_VPROC)glGetProcAddress("glGetInteger64i_v");
	gl_api._glGetBufferParameteri64v_ptr = (PFNGLGETBUFFERPARAMETERI64VPROC)glGetProcAddress("glGetBufferParameteri64v");
	gl_api._glFramebufferTexture_ptr = (PFNGLFRAMEBUFFERTEXTUREPROC)glGetProcAddress("glFramebufferTexture");
	gl_api._glDrawElementsBaseVertex_ptr = (PFNGLDRAWELEMENTSBASEVERTEXPROC)glGetProcAddress("glDrawElementsBaseVertex"); // ARB_draw_elements_base_vertex
	gl_api._glDrawRangeElementsBaseVertex_ptr = (PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC)glGetProcAddress("glDrawRangeElementsBaseVertex"); // ARB_draw_elements_base_vertex
	gl_api._glDrawElementsInstancedBaseVertex_ptr = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC)glGetProcAddress("glDrawElementsInstancedBaseVertex"); // ARB_draw_elements_base_vertex
	gl_api._glMultiDrawElementsBaseVertex_ptr = (PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC)glGetProcAddress("glMultiDrawElementsBaseVertex"); // ARB_draw_elements_base_vertex
	gl_api._glProvokingVertex_ptr = (PFNGLPROVOKINGVERTEXPROC)glGetProcAddress("glProvokingVertex"); // ARB_provoking_vertex
	gl_api._glFenceSync_ptr = (PFNGLFENCESYNCPROC)glGetProcAddress("glFenceSync"); // ARB_sync
	gl_api._glIsSync_ptr = (PFNGLISSYNCPROC)glGetProcAddress("glIsSync"); // ARB_sync
	gl_api._glDeleteSync_ptr = (PFNGLDELETESYNCPROC)glGetProcAddress("glDeleteSync"); // ARB_sync
	gl_api._glClientWaitSync_ptr = (PFNGLCLIENTWAITSYNCPROC)glGetProcAddress("glClientWaitSync"); // ARB_sync
	gl_api._glWaitSync_ptr = (PFNGLWAITSYNCPROC)glGetProcAddress("glWaitSync"); // ARB_sync
	gl_api._glGetInteger64v_ptr = (PFNGLGETINTEGER64VPROC)glGetProcAddress("glGetInteger64v"); // ARB_sync
	gl_api._glGetSynciv_ptr = (PFNGLGETSYNCIVPROC)glGetProcAddress("glGetSynciv"); // ARB_sync
	gl_api._glTexImage2DMultisample_ptr = (PFNGLTEXIMAGE2DMULTISAMPLEPROC)glGetProcAddress("glTexImage2DMultisample"); // ARB_texture_multisample
	gl_api._glTexImage3DMultisample_ptr = (PFNGLTEXIMAGE3DMULTISAMPLEPROC)glGetProcAddress("glTexImage3DMultisample"); // ARB_texture_multisample
	gl_api._glGetMultisamplefv_ptr = (PFNGLGETMULTISAMPLEFVPROC)glGetProcAddress("glGetMultisamplefv"); // ARB_texture_multisample
	gl_api._glSampleMaski_ptr = (PFNGLSAMPLEMASKIPROC)glGetProcAddress("glSampleMaski"); // ARB_texture_multisample
	
	// OpenGL 3.3
	gl_api._glVertexAttribDivisor_ptr = (PFNGLVERTEXATTRIBDIVISORPROC)glGetProcAddress("glVertexAttribDivisor");
	gl_api._glBindFragDataLocationIndexed_ptr = (PFNGLBINDFRAGDATALOCATIONINDEXEDPROC)glGetProcAddress("glBindFragDataLocationIndexed"); // ARB_blend_func_extended
	gl_api._glGetFragDataIndex_ptr = (PFNGLGETFRAGDATAINDEXPROC)glGetProcAddress("glGetFragDataIndex"); // ARB_blend_func_extended
	gl_api._glGenSamplers_ptr = (PFNGLGENSAMPLERSPROC)glGetProcAddress("glGenSamplers"); // ARB_sampler_objects
	gl_api._glDeleteSamplers_ptr = (PFNGLDELETESAMPLERSPROC)glGetProcAddress("glDeleteSamplers"); // ARB_sampler_objects
	gl_api._glIsSampler_ptr = (PFNGLISSAMPLERPROC)glGetProcAddress("glIsSampler"); // ARB_sampler_objects
	gl_api._glBindSampler_ptr = (PFNGLBINDSAMPLERPROC)glGetProcAddress("glBindSampler"); // ARB_sampler_objects
	gl_api._glSamplerParameteri_ptr = (PFNGLSAMPLERPARAMETERIPROC)glGetProcAddress("glSamplerParameteri"); // ARB_sampler_objects
	gl_api._glSamplerParameteriv_ptr = (PFNGLSAMPLERPARAMETERIVPROC)glGetProcAddress("glSamplerParameteriv"); // ARB_sampler_objects
	gl_api._glSamplerParameterf_ptr = (PFNGLSAMPLERPARAMETERFPROC)glGetProcAddress("glSamplerParameterf"); // ARB_sampler_objects
	gl_api._glSamplerParameterfv_ptr = (PFNGLSAMPLERPARAMETERFVPROC)glGetProcAddress("glSamplerParameterfv"); // ARB_sampler_objects
	gl_api._glSamplerParameterIiv_ptr = (PFNGLSAMPLERPARAMETERIIVPROC)glGetProcAddress("glSamplerParameterIiv"); // ARB_sampler_objects
	gl_api._glSamplerParameterIuiv_ptr = (PFNGLSAMPLERPARAMETERIUIVPROC)glGetProcAddress("glSamplerParameterIuiv"); // ARB_sampler_objects
	gl_api._glGetSamplerParameteriv_ptr = (PFNGLGETSAMPLERPARAMETERIVPROC)glGetProcAddress("glGetSamplerParameteriv"); // ARB_sampler_objects
	gl_api._glGetSamplerParameterIiv_ptr = (PFNGLGETSAMPLERPARAMETERIIVPROC)glGetProcAddress("glGetSamplerParameterIiv"); // ARB_sampler_objects
	gl_api._glGetSamplerParameterfv_ptr = (PFNGLGETSAMPLERPARAMETERFVPROC)glGetProcAddress("glGetSamplerParameterfv"); // ARB_sampler_objects
	gl_api._glGetSamplerParameterIuiv_ptr = (PFNGLGETSAMPLERPARAMETERIUIVPROC)glGetProcAddress("glGetSamplerParameterIuiv"); // ARB_sampler_objects
	gl_api._glQueryCounter_ptr = (PFNGLQUERYCOUNTERPROC)glGetProcAddress("glQueryCounter"); // ARB_timer_query
	gl_api._glGetQueryObjecti64v_ptr = (PFNGLGETQUERYOBJECTI64VPROC)glGetProcAddress("glGetQueryObjecti64v"); // ARB_timer_query
	gl_api._glGetQueryObjectui64v_ptr = (PFNGLGETQUERYOBJECTUI64VPROC)glGetProcAddress("glGetQueryObjectui64v"); // ARB_timer_query
	gl_api._glVertexAttribP1ui_ptr = (PFNGLVERTEXATTRIBP1UIPROC)glGetProcAddress("glVertexAttribP1ui"); // ARB_vertex_type_2_10_10_10_rev
	gl_api._glVertexAttribP1uiv_ptr = (PFNGLVERTEXATTRIBP1UIVPROC)glGetProcAddress("glVertexAttribP1uiv"); // ARB_vertex_type_2_10_10_10_rev
	gl_api._glVertexAttribP2ui_ptr = (PFNGLVERTEXATTRIBP2UIPROC)glGetProcAddress("glVertexAttribP2ui"); // ARB_vertex_type_2_10_10_10_rev
	gl_api._glVertexAttribP2uiv_ptr = (PFNGLVERTEXATTRIBP2UIVPROC)glGetProcAddress("glVertexAttribP2uiv"); // ARB_vertex_type_2_10_10_10_rev
	gl_api._glVertexAttribP3ui_ptr = (PFNGLVERTEXATTRIBP3UIPROC)glGetProcAddress("glVertexAttribP3ui"); // ARB_vertex_type_2_10_10_10_rev
	gl_api._glVertexAttribP3uiv_ptr = (PFNGLVERTEXATTRIBP3UIVPROC)glGetProcAddress("glVertexAttribP3uiv"); // ARB_vertex_type_2_10_10_10_rev
	gl_api._glVertexAttribP4ui_ptr = (PFNGLVERTEXATTRIBP4UIPROC)glGetProcAddress("glVertexAttribP4ui"); // ARB_vertex_type_2_10_10_10_rev
	gl_api._glVertexAttribP4uiv_ptr = (PFNGLVERTEXATTRIBP4UIVPROC)glGetProcAddress("glVertexAttribP4uiv"); // ARB_vertex_type_2_10_10_10_rev
	
	// OpenGL 4.0
	gl_api._glMinSampleShading_ptr = (PFNGLMINSAMPLESHADINGPROC)glGetProcAddress("glMinSampleShading");
	gl_api._glBlendEquationi_ptr = (PFNGLBLENDEQUATIONIPROC)glGetProcAddress("glBlendEquationi");
	gl_api._glBlendEquationSeparatei_ptr = (PFNGLBLENDEQUATIONSEPARATEIPROC)glGetProcAddress("glBlendEquationSeparatei");
	gl_api._glBlendFunci_ptr = (PFNGLBLENDFUNCIPROC)glGetProcAddress("glBlendFunci");
	gl_api._glBlendFuncSeparatei_ptr = (PFNGLBLENDFUNCSEPARATEIPROC)glGetProcAddress("glBlendFuncSeparatei");
	gl_api._glDrawArraysIndirect_ptr = (PFNGLDRAWARRAYSINDIRECTPROC)glGetProcAddress("glDrawArraysIndirect"); // ARB_draw_indirect
	gl_api._glDrawElementsIndirect_ptr = (PFNGLDRAWELEMENTSINDIRECTPROC)glGetProcAddress("glDrawElementsIndirect"); // ARB_draw_indirect
	gl_api._glUniform1d_ptr = (PFNGLUNIFORM1DPROC)glGetProcAddress("glUniform1d"); // ARB_gpu_shader_fp64
	gl_api._glUniform2d_ptr = (PFNGLUNIFORM2DPROC)glGetProcAddress("glUniform2d"); // ARB_gpu_shader_fp64
	gl_api._glUniform3d_ptr = (PFNGLUNIFORM3DPROC)glGetProcAddress("glUniform3d"); // ARB_gpu_shader_fp64
	gl_api._glUniform4d_ptr = (PFNGLUNIFORM4DPROC)glGetProcAddress("glUniform4d"); // ARB_gpu_shader_fp64
	gl_api._glUniform1dv_ptr = (PFNGLUNIFORM1DVPROC)glGetProcAddress("glUniform1dv"); // ARB_gpu_shader_fp64
	gl_api._glUniform2dv_ptr = (PFNGLUNIFORM2DVPROC)glGetProcAddress("glUniform2dv"); // ARB_gpu_shader_fp64
	gl_api._glUniform3dv_ptr = (PFNGLUNIFORM3DVPROC)glGetProcAddress("glUniform3dv"); // ARB_gpu_shader_fp64
	gl_api._glUniform4dv_ptr = (PFNGLUNIFORM4DVPROC)glGetProcAddress("glUniform4dv"); // ARB_gpu_shader_fp64
	gl_api._glUniformMatrix2dv_ptr = (PFNGLUNIFORMMATRIX2DVPROC)glGetProcAddress("glUniformMatrix2dv"); // ARB_gpu_shader_fp64
	gl_api._glUniformMatrix3dv_ptr = (PFNGLUNIFORMMATRIX3DVPROC)glGetProcAddress("glUniformMatrix3dv"); // ARB_gpu_shader_fp64
	gl_api._glUniformMatrix4dv_ptr = (PFNGLUNIFORMMATRIX4DVPROC)glGetProcAddress("glUniformMatrix4dv"); // ARB_gpu_shader_fp64
	gl_api._glUniformMatrix2x3dv_ptr = (PFNGLUNIFORMMATRIX2X3DVPROC)glGetProcAddress("glUniformMatrix2x3dv"); // ARB_gpu_shader_fp64
	gl_api._glUniformMatrix2x4dv_ptr = (PFNGLUNIFORMMATRIX2X4DVPROC)glGetProcAddress("glUniformMatrix2x4dv"); // ARB_gpu_shader_fp64
	gl_api._glUniformMatrix3x2dv_ptr = (PFNGLUNIFORMMATRIX3X2DVPROC)glGetProcAddress("glUniformMatrix3x2dv"); // ARB_gpu_shader_fp64
	gl_api._glUniformMatrix3x4dv_ptr = (PFNGLUNIFORMMATRIX3X4DVPROC)glGetProcAddress("glUniformMatrix3x4dv"); // ARB_gpu_shader_fp64
	gl_api._glUniformMatrix4x2dv_ptr = (PFNGLUNIFORMMATRIX4X2DVPROC)glGetProcAddress("glUniformMatrix4x2dv"); // ARB_gpu_shader_fp64
	gl_api._glUniformMatrix4x3dv_ptr = (PFNGLUNIFORMMATRIX4X3DVPROC)glGetProcAddress("glUniformMatrix4x3dv"); // ARB_gpu_shader_fp64
	gl_api._glGetUniformdv_ptr = (PFNGLGETUNIFORMDVPROC)glGetProcAddress("glGetUniformdv"); // ARB_gpu_shader_fp64
	gl_api._glGetSubroutineUniformLocation_ptr = (PFNGLGETSUBROUTINEUNIFORMLOCATIONPROC)glGetProcAddress("glGetSubroutineUniformLocation"); // ARB_shader_subroutine
	gl_api._glGetSubroutineIndex_ptr = (PFNGLGETSUBROUTINEINDEXPROC)glGetProcAddress("glGetSubroutineIndex"); // ARB_shader_subroutine
	gl_api._glGetActiveSubroutineUniformiv_ptr = (PFNGLGETACTIVESUBROUTINEUNIFORMIVPROC)glGetProcAddress("glGetActiveSubroutineUniformiv"); // ARB_shader_subroutine
	gl_api._glGetActiveSubroutineUniformName_ptr = (PFNGLGETACTIVESUBROUTINEUNIFORMNAMEPROC)glGetProcAddress("glGetActiveSubroutineUniformName"); // ARB_shader_subroutine
	gl_api._glGetActiveSubroutineName_ptr = (PFNGLGETACTIVESUBROUTINENAMEPROC)glGetProcAddress("glGetActiveSubroutineName"); // ARB_shader_subroutine
	gl_api._glUniformSubroutinesuiv_ptr = (PFNGLUNIFORMSUBROUTINESUIVPROC)glGetProcAddress("glUniformSubroutinesuiv"); // ARB_shader_subroutine
	gl_api._glGetUniformSubroutineuiv_ptr = (PFNGLGETUNIFORMSUBROUTINEUIVPROC)glGetProcAddress("glGetUniformSubroutineuiv"); // ARB_shader_subroutine
	gl_api._glGetProgramStageiv_ptr = (PFNGLGETPROGRAMSTAGEIVPROC)glGetProcAddress("glGetProgramStageiv"); // ARB_shader_subroutine
	gl_api._glPatchParameteri_ptr = (PFNGLPATCHPARAMETERIPROC)glGetProcAddress("glPatchParameteri"); // ARB_tessellation_shader
	gl_api._glPatchParameterfv_ptr = (PFNGLPATCHPARAMETERFVPROC)glGetProcAddress("glPatchParameterfv"); // ARB_tessellation_shader
	gl_api._glBindTransformFeedback_ptr = (PFNGLBINDTRANSFORMFEEDBACKPROC)glGetProcAddress("glBindTransformFeedback"); // ARB_transform_feedback2
	gl_api._glDeleteTransformFeedbacks_ptr = (PFNGLDELETETRANSFORMFEEDBACKSPROC)glGetProcAddress("glDeleteTransformFeedbacks"); // ARB_transform_feedback2
	gl_api._glGenTransformFeedbacks_ptr = (PFNGLGENTRANSFORMFEEDBACKSPROC)glGetProcAddress("glGenTransformFeedbacks"); // ARB_transform_feedback2
	gl_api._glIsTransformFeedback_ptr = (PFNGLISTRANSFORMFEEDBACKPROC)glGetProcAddress("glIsTransformFeedback"); // ARB_transform_feedback2
	gl_api._glPauseTransformFeedback_ptr = (PFNGLPAUSETRANSFORMFEEDBACKPROC)glGetProcAddress("glPauseTransformFeedback"); // ARB_transform_feedback2
	gl_api._glResumeTransformFeedback_ptr = (PFNGLRESUMETRANSFORMFEEDBACKPROC)glGetProcAddress("glResumeTransformFeedback"); // ARB_transform_feedback2
	gl_api._glDrawTransformFeedback_ptr = (PFNGLDRAWTRANSFORMFEEDBACKPROC)glGetProcAddress("glDrawTransformFeedback"); // ARB_transform_feedback2
	gl_api._glDrawTransformFeedbackStream_ptr = (PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC)glGetProcAddress("glDrawTransformFeedbackStream"); // ARB_transform_feedback3
	gl_api._glBeginQueryIndexed_ptr = (PFNGLBEGINQUERYINDEXEDPROC)glGetProcAddress("glBeginQueryIndexed"); // ARB_transform_feedback3
	gl_api._glEndQueryIndexed_ptr = (PFNGLENDQUERYINDEXEDPROC)glGetProcAddress("glEndQueryIndexed"); // ARB_transform_feedback3
	gl_api._glGetQueryIndexediv_ptr = (PFNGLGETQUERYINDEXEDIVPROC)glGetProcAddress("glGetQueryIndexediv"); // ARB_transform_feedback3
	
	// OpenGL 4.1
	gl_api._glReleaseShaderCompiler_ptr = (PFNGLRELEASESHADERCOMPILERPROC)glGetProcAddress("glReleaseShaderCompiler"); // ARB_ES2_compatibility
	gl_api._glShaderBinary_ptr = (PFNGLSHADERBINARYPROC)glGetProcAddress("glShaderBinary"); // ARB_ES2_compatibility
	gl_api._glGetShaderPrecisionFormat_ptr = (PFNGLGETSHADERPRECISIONFORMATPROC)glGetProcAddress("glGetShaderPrecisionFormat"); // ARB_ES2_compatibility
	gl_api._glDepthRangef_ptr = (PFNGLDEPTHRANGEFPROC)glGetProcAddress("glDepthRangef"); // ARB_ES2_compatibility
	gl_api._glClearDepthf_ptr = (PFNGLCLEARDEPTHFPROC)glGetProcAddress("glClearDepthf"); // ARB_ES2_compatibility
	gl_api._glGetProgramBinary_ptr = (PFNGLGETPROGRAMBINARYPROC)glGetProcAddress("glGetProgramBinary"); // ARB_get_program_binary
	gl_api._glProgramBinary_ptr = (PFNGLPROGRAMBINARYPROC)glGetProcAddress("glProgramBinary"); // ARB_get_program_binary
	gl_api._glProgramParameteri_ptr = (PFNGLPROGRAMPARAMETERIPROC)glGetProcAddress("glProgramParameteri"); // ARB_get_program_binary
	gl_api._glUseProgramStages_ptr = (PFNGLUSEPROGRAMSTAGESPROC)glGetProcAddress("glUseProgramStages"); // ARB_separate_shader_objects
	gl_api._glActiveShaderProgram_ptr = (PFNGLACTIVESHADERPROGRAMPROC)glGetProcAddress("glActiveShaderProgram"); // ARB_separate_shader_objects
	gl_api._glCreateShaderProgramv_ptr = (PFNGLCREATESHADERPROGRAMVPROC)glGetProcAddress("glCreateShaderProgramv"); // ARB_separate_shader_objects
	gl_api._glBindProgramPipeline_ptr = (PFNGLBINDPROGRAMPIPELINEPROC)glGetProcAddress("glBindProgramPipeline"); // ARB_separate_shader_objects
	gl_api._glDeleteProgramPipelines_ptr = (PFNGLDELETEPROGRAMPIPELINESPROC)glGetProcAddress("glDeleteProgramPipelines"); // ARB_separate_shader_objects
	gl_api._glGenProgramPipelines_ptr = (PFNGLGENPROGRAMPIPELINESPROC)glGetProcAddress("glGenProgramPipelines"); // ARB_separate_shader_objects
	gl_api._glIsProgramPipeline_ptr = (PFNGLISPROGRAMPIPELINEPROC)glGetProcAddress("glIsProgramPipeline"); // ARB_separate_shader_objects
	gl_api._glGetProgramPipelineiv_ptr = (PFNGLGETPROGRAMPIPELINEIVPROC)glGetProcAddress("glGetProgramPipelineiv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform1i_ptr = (PFNGLPROGRAMUNIFORM1IPROC)glGetProcAddress("glProgramUniform1i"); // ARB_separate_shader_objects
	gl_api._glProgramUniform1iv_ptr = (PFNGLPROGRAMUNIFORM1IVPROC)glGetProcAddress("glProgramUniform1iv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform1f_ptr = (PFNGLPROGRAMUNIFORM1FPROC)glGetProcAddress("glProgramUniform1f"); // ARB_separate_shader_objects
	gl_api._glProgramUniform1fv_ptr = (PFNGLPROGRAMUNIFORM1FVPROC)glGetProcAddress("glProgramUniform1fv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform1d_ptr = (PFNGLPROGRAMUNIFORM1DPROC)glGetProcAddress("glProgramUniform1d"); // ARB_separate_shader_objects
	gl_api._glProgramUniform1dv_ptr = (PFNGLPROGRAMUNIFORM1DVPROC)glGetProcAddress("glProgramUniform1dv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform1ui_ptr = (PFNGLPROGRAMUNIFORM1UIPROC)glGetProcAddress("glProgramUniform1ui"); // ARB_separate_shader_objects
	gl_api._glProgramUniform1uiv_ptr = (PFNGLPROGRAMUNIFORM1UIVPROC)glGetProcAddress("glProgramUniform1uiv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform2i_ptr = (PFNGLPROGRAMUNIFORM2IPROC)glGetProcAddress("glProgramUniform2i"); // ARB_separate_shader_objects
	gl_api._glProgramUniform2iv_ptr = (PFNGLPROGRAMUNIFORM2IVPROC)glGetProcAddress("glProgramUniform2iv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform2f_ptr = (PFNGLPROGRAMUNIFORM2FPROC)glGetProcAddress("glProgramUniform2f"); // ARB_separate_shader_objects
	gl_api._glProgramUniform2fv_ptr = (PFNGLPROGRAMUNIFORM2FVPROC)glGetProcAddress("glProgramUniform2fv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform2d_ptr = (PFNGLPROGRAMUNIFORM2DPROC)glGetProcAddress("glProgramUniform2d"); // ARB_separate_shader_objects
	gl_api._glProgramUniform2dv_ptr = (PFNGLPROGRAMUNIFORM2DVPROC)glGetProcAddress("glProgramUniform2dv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform2ui_ptr = (PFNGLPROGRAMUNIFORM2UIPROC)glGetProcAddress("glProgramUniform2ui"); // ARB_separate_shader_objects
	gl_api._glProgramUniform2uiv_ptr = (PFNGLPROGRAMUNIFORM2UIVPROC)glGetProcAddress("glProgramUniform2uiv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform3i_ptr = (PFNGLPROGRAMUNIFORM3IPROC)glGetProcAddress("glProgramUniform3i"); // ARB_separate_shader_objects
	gl_api._glProgramUniform3iv_ptr = (PFNGLPROGRAMUNIFORM3IVPROC)glGetProcAddress("glProgramUniform3iv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform3f_ptr = (PFNGLPROGRAMUNIFORM3FPROC)glGetProcAddress("glProgramUniform3f"); // ARB_separate_shader_objects
	gl_api._glProgramUniform3fv_ptr = (PFNGLPROGRAMUNIFORM3FVPROC)glGetProcAddress("glProgramUniform3fv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform3d_ptr = (PFNGLPROGRAMUNIFORM3DPROC)glGetProcAddress("glProgramUniform3d"); // ARB_separate_shader_objects
	gl_api._glProgramUniform3dv_ptr = (PFNGLPROGRAMUNIFORM3DVPROC)glGetProcAddress("glProgramUniform3dv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform3ui_ptr = (PFNGLPROGRAMUNIFORM3UIPROC)glGetProcAddress("glProgramUniform3ui"); // ARB_separate_shader_objects
	gl_api._glProgramUniform3uiv_ptr = (PFNGLPROGRAMUNIFORM3UIVPROC)glGetProcAddress("glProgramUniform3uiv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform4i_ptr = (PFNGLPROGRAMUNIFORM4IPROC)glGetProcAddress("glProgramUniform4i"); // ARB_separate_shader_objects
	gl_api._glProgramUniform4iv_ptr = (PFNGLPROGRAMUNIFORM4IVPROC)glGetProcAddress("glProgramUniform4iv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform4f_ptr = (PFNGLPROGRAMUNIFORM4FPROC)glGetProcAddress("glProgramUniform4f"); // ARB_separate_shader_objects
	gl_api._glProgramUniform4fv_ptr = (PFNGLPROGRAMUNIFORM4FVPROC)glGetProcAddress("glProgramUniform4fv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform4d_ptr = (PFNGLPROGRAMUNIFORM4DPROC)glGetProcAddress("glProgramUniform4d"); // ARB_separate_shader_objects
	gl_api._glProgramUniform4dv_ptr = (PFNGLPROGRAMUNIFORM4DVPROC)glGetProcAddress("glProgramUniform4dv"); // ARB_separate_shader_objects
	gl_api._glProgramUniform4ui_ptr = (PFNGLPROGRAMUNIFORM4UIPROC)glGetProcAddress("glProgramUniform4ui"); // ARB_separate_shader_objects
	gl_api._glProgramUniform4uiv_ptr = (PFNGLPROGRAMUNIFORM4UIVPROC)glGetProcAddress("glProgramUniform4uiv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix2fv_ptr = (PFNGLPROGRAMUNIFORMMATRIX2FVPROC)glGetProcAddress("glProgramUniformMatrix2fv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix3fv_ptr = (PFNGLPROGRAMUNIFORMMATRIX3FVPROC)glGetProcAddress("glProgramUniformMatrix3fv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix4fv_ptr = (PFNGLPROGRAMUNIFORMMATRIX4FVPROC)glGetProcAddress("glProgramUniformMatrix4fv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix2dv_ptr = (PFNGLPROGRAMUNIFORMMATRIX2DVPROC)glGetProcAddress("glProgramUniformMatrix2dv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix3dv_ptr = (PFNGLPROGRAMUNIFORMMATRIX3DVPROC)glGetProcAddress("glProgramUniformMatrix3dv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix4dv_ptr = (PFNGLPROGRAMUNIFORMMATRIX4DVPROC)glGetProcAddress("glProgramUniformMatrix4dv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix2x3fv_ptr = (PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC)glGetProcAddress("glProgramUniformMatrix2x3fv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix3x2fv_ptr = (PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC)glGetProcAddress("glProgramUniformMatrix3x2fv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix2x4fv_ptr = (PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC)glGetProcAddress("glProgramUniformMatrix2x4fv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix4x2fv_ptr = (PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC)glGetProcAddress("glProgramUniformMatrix4x2fv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix3x4fv_ptr = (PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC)glGetProcAddress("glProgramUniformMatrix3x4fv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix4x3fv_ptr = (PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC)glGetProcAddress("glProgramUniformMatrix4x3fv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix2x3dv_ptr = (PFNGLPROGRAMUNIFORMMATRIX2X3DVPROC)glGetProcAddress("glProgramUniformMatrix2x3dv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix3x2dv_ptr = (PFNGLPROGRAMUNIFORMMATRIX3X2DVPROC)glGetProcAddress("glProgramUniformMatrix3x2dv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix2x4dv_ptr = (PFNGLPROGRAMUNIFORMMATRIX2X4DVPROC)glGetProcAddress("glProgramUniformMatrix2x4dv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix4x2dv_ptr = (PFNGLPROGRAMUNIFORMMATRIX4X2DVPROC)glGetProcAddress("glProgramUniformMatrix4x2dv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix3x4dv_ptr = (PFNGLPROGRAMUNIFORMMATRIX3X4DVPROC)glGetProcAddress("glProgramUniformMatrix3x4dv"); // ARB_separate_shader_objects
	gl_api._glProgramUniformMatrix4x3dv_ptr = (PFNGLPROGRAMUNIFORMMATRIX4X3DVPROC)glGetProcAddress("glProgramUniformMatrix4x3dv"); // ARB_separate_shader_objects
	gl_api._glValidateProgramPipeline_ptr = (PFNGLVALIDATEPROGRAMPIPELINEPROC)glGetProcAddress("glValidateProgramPipeline"); // ARB_separate_shader_objects
	gl_api._glGetProgramPipelineInfoLog_ptr = (PFNGLGETPROGRAMPIPELINEINFOLOGPROC)glGetProcAddress("glGetProgramPipelineInfoLog"); // ARB_separate_shader_objects
	gl_api._glVertexAttribL1d_ptr = (PFNGLVERTEXATTRIBL1DPROC)glGetProcAddress("glVertexAttribL1d"); // ARB_vertex_attrib_64bit
	gl_api._glVertexAttribL2d_ptr = (PFNGLVERTEXATTRIBL2DPROC)glGetProcAddress("glVertexAttribL2d"); // ARB_vertex_attrib_64bit
	gl_api._glVertexAttribL3d_ptr = (PFNGLVERTEXATTRIBL3DPROC)glGetProcAddress("glVertexAttribL3d"); // ARB_vertex_attrib_64bit
	gl_api._glVertexAttribL4d_ptr = (PFNGLVERTEXATTRIBL4DPROC)glGetProcAddress("glVertexAttribL4d"); // ARB_vertex_attrib_64bit
	gl_api._glVertexAttribL1dv_ptr = (PFNGLVERTEXATTRIBL1DVPROC)glGetProcAddress("glVertexAttribL1dv"); // ARB_vertex_attrib_64bit
	gl_api._glVertexAttribL2dv_ptr = (PFNGLVERTEXATTRIBL2DVPROC)glGetProcAddress("glVertexAttribL2dv"); // ARB_vertex_attrib_64bit
	gl_api._glVertexAttribL3dv_ptr = (PFNGLVERTEXATTRIBL3DVPROC)glGetProcAddress("glVertexAttribL3dv"); // ARB_vertex_attrib_64bit
	gl_api._glVertexAttribL4dv_ptr = (PFNGLVERTEXATTRIBL4DVPROC)glGetProcAddress("glVertexAttribL4dv"); // ARB_vertex_attrib_64bit
	gl_api._glVertexAttribLPointer_ptr = (PFNGLVERTEXATTRIBLPOINTERPROC)glGetProcAddress("glVertexAttribLPointer"); // ARB_vertex_attrib_64bit
	gl_api._glGetVertexAttribLdv_ptr = (PFNGLGETVERTEXATTRIBLDVPROC)glGetProcAddress("glGetVertexAttribLdv"); // ARB_vertex_attrib_64bit
	gl_api._glViewportArrayv_ptr = (PFNGLVIEWPORTARRAYVPROC)glGetProcAddress("glViewportArrayv"); // ARB_viewport_array
	gl_api._glViewportIndexedf_ptr = (PFNGLVIEWPORTINDEXEDFPROC)glGetProcAddress("glViewportIndexedf"); // ARB_viewport_array
	gl_api._glViewportIndexedfv_ptr = (PFNGLVIEWPORTINDEXEDFVPROC)glGetProcAddress("glViewportIndexedfv"); // ARB_viewport_array
	gl_api._glScissorArrayv_ptr = (PFNGLSCISSORARRAYVPROC)glGetProcAddress("glScissorArrayv"); // ARB_viewport_array
	gl_api._glScissorIndexed_ptr = (PFNGLSCISSORINDEXEDPROC)glGetProcAddress("glScissorIndexed"); // ARB_viewport_array
	gl_api._glScissorIndexedv_ptr = (PFNGLSCISSORINDEXEDVPROC)glGetProcAddress("glScissorIndexedv"); // ARB_viewport_array
	gl_api._glDepthRangeArrayv_ptr = (PFNGLDEPTHRANGEARRAYVPROC)glGetProcAddress("glDepthRangeArrayv"); // ARB_viewport_array
	gl_api._glDepthRangeIndexed_ptr = (PFNGLDEPTHRANGEINDEXEDPROC)glGetProcAddress("glDepthRangeIndexed"); // ARB_viewport_array
	gl_api._glGetFloati_v_ptr = (PFNGLGETFLOATI_VPROC)glGetProcAddress("glGetFloati_v"); // ARB_viewport_array
	gl_api._glGetDoublei_v_ptr = (PFNGLGETDOUBLEI_VPROC)glGetProcAddress("glGetDoublei_v"); // ARB_viewport_array
	
	// OpenGL 4.3
	gl_api._glCopyImageSubData_ptr = (PFNGLCOPYIMAGESUBDATAPROC)glGetProcAddress("glCopyImageSubData"); // ARB_copy_image
	
	
	// fallback (EXT_framebuffer_object, EXT_framebuffer_blit)
	if(gl_api._glIsRenderbuffer_ptr == nullptr) gl_api._glIsRenderbuffer_ptr = (PFNGLISRENDERBUFFERPROC)glGetProcAddress("glIsRenderbufferEXT"); // EXT_framebuffer_object
	if(gl_api._glBindRenderbuffer_ptr == nullptr) gl_api._glBindRenderbuffer_ptr = (PFNGLBINDRENDERBUFFERPROC)glGetProcAddress("glBindRenderbufferEXT"); // EXT_framebuffer_object
	if(gl_api._glDeleteRenderbuffers_ptr == nullptr) gl_api._glDeleteRenderbuffers_ptr = (PFNGLDELETERENDERBUFFERSPROC)glGetProcAddress("glDeleteRenderbuffersEXT"); // EXT_framebuffer_object
	if(gl_api._glGenRenderbuffers_ptr == nullptr) gl_api._glGenRenderbuffers_ptr = (PFNGLGENRENDERBUFFERSPROC)glGetProcAddress("glGenRenderbuffersEXT"); // EXT_framebuffer_object
	if(gl_api._glRenderbufferStorage_ptr == nullptr) gl_api._glRenderbufferStorage_ptr = (PFNGLRENDERBUFFERSTORAGEPROC)glGetProcAddress("glRenderbufferStorageEXT"); // EXT_framebuffer_object
	if(gl_api._glGetRenderbufferParameteriv_ptr == nullptr) gl_api._glGetRenderbufferParameteriv_ptr = (PFNGLGETRENDERBUFFERPARAMETERIVPROC)glGetProcAddress("glGetRenderbufferParameterivEXT"); // EXT_framebuffer_object
	if(gl_api._glIsFramebuffer_ptr == nullptr) gl_api._glIsFramebuffer_ptr = (PFNGLISFRAMEBUFFERPROC)glGetProcAddress("glIsFramebufferEXT"); // EXT_framebuffer_object
	if(gl_api._glBindFramebuffer_ptr == nullptr) gl_api._glBindFramebuffer_ptr = (PFNGLBINDFRAMEBUFFERPROC)glGetProcAddress("glBindFramebufferEXT"); // EXT_framebuffer_object
	if(gl_api._glDeleteFramebuffers_ptr == nullptr) gl_api._glDeleteFramebuffers_ptr = (PFNGLDELETEFRAMEBUFFERSPROC)glGetProcAddress("glDeleteFramebuffersEXT"); // EXT_framebuffer_object
	if(gl_api._glGenFramebuffers_ptr == nullptr) gl_api._glGenFramebuffers_ptr = (PFNGLGENFRAMEBUFFERSPROC)glGetProcAddress("glGenFramebuffersEXT"); // EXT_framebuffer_object
	if(gl_api._glCheckFramebufferStatus_ptr == nullptr) gl_api._glCheckFramebufferStatus_ptr = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)glGetProcAddress("glCheckFramebufferStatusEXT"); // EXT_framebuffer_object
	if(gl_api._glFramebufferTexture1D_ptr == nullptr) gl_api._glFramebufferTexture1D_ptr = (PFNGLFRAMEBUFFERTEXTURE1DPROC)glGetProcAddress("glFramebufferTexture1DEXT"); // EXT_framebuffer_object
	if(gl_api._glFramebufferTexture2D_ptr == nullptr) gl_api._glFramebufferTexture2D_ptr = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glGetProcAddress("glFramebufferTexture2DEXT"); // EXT_framebuffer_object
	if(gl_api._glFramebufferTexture3D_ptr == nullptr) gl_api._glFramebufferTexture3D_ptr = (PFNGLFRAMEBUFFERTEXTURE3DPROC)glGetProcAddress("glFramebufferTexture3DEXT"); // EXT_framebuffer_object
	if(gl_api._glFramebufferRenderbuffer_ptr == nullptr) gl_api._glFramebufferRenderbuffer_ptr = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)glGetProcAddress("glFramebufferRenderbufferEXT"); // EXT_framebuffer_object
	if(gl_api._glGetFramebufferAttachmentParameteriv_ptr == nullptr) gl_api._glGetFramebufferAttachmentParameteriv_ptr = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)glGetProcAddress("glGetFramebufferAttachmentParameterivEXT"); // EXT_framebuffer_object
	if(gl_api._glGenerateMipmap_ptr == nullptr) gl_api._glGenerateMipmap_ptr = (PFNGLGENERATEMIPMAPPROC)glGetProcAddress("glGenerateMipmapEXT"); // EXT_framebuffer_object
	if(gl_api._glBlitFramebuffer_ptr == nullptr) gl_api._glBlitFramebuffer_ptr = (PFNGLBLITFRAMEBUFFERPROC)glGetProcAddress("glBlitFramebufferEXT"); // EXT_framebuffer_blit
	
	// fallback (ARB_copy_image, NV_copy_image)
	if(gl_api._glCopyImageSubData_ptr == nullptr) gl_api._glCopyImageSubData_ptr = (PFNGLCOPYIMAGESUBDATAPROC)glGetProcAddress("glCopyImageSubDataARB"); // ARB_copy_image
	if(gl_api._glCopyImageSubData_ptr == nullptr) gl_api._glCopyImageSubData_ptr = (PFNGLCOPYIMAGESUBDATAPROC)glGetProcAddress("glCopyImageSubDataNV"); // NV_copy_image
	
	// check gl function pointers (print error if nullptr)
	if(gl_api._glIsRenderbuffer_ptr == nullptr) log_error("couldn't get function pointer to \"glIsRenderbuffer\"!");
	if(gl_api._glBindRenderbuffer_ptr == nullptr) log_error("couldn't get function pointer to \"glBindRenderbuffer\"!");
	if(gl_api._glDeleteRenderbuffers_ptr == nullptr) log_error("couldn't get function pointer to \"glDeleteRenderbuffers\"!");
	if(gl_api._glGenRenderbuffers_ptr == nullptr) log_error("couldn't get function pointer to \"glGenRenderbuffers\"!");
	if(gl_api._glRenderbufferStorage_ptr == nullptr) log_error("couldn't get function pointer to \"glRenderbufferStorage\"!");
	if(gl_api._glGetRenderbufferParameteriv_ptr == nullptr) log_error("couldn't get function pointer to \"glGetRenderbufferParameteriv\"!");
	if(gl_api._glIsFramebuffer_ptr == nullptr) log_error("couldn't get function pointer to \"glIsFramebuffer\"!");
	if(gl_api._glBindFramebuffer_ptr == nullptr) log_error("couldn't get function pointer to \"glBindFramebuffer\"!");
	if(gl_api._glDeleteFramebuffers_ptr == nullptr) log_error("couldn't get function pointer to \"glDeleteFramebuffers\"!");
	if(gl_api._glGenFramebuffers_ptr == nullptr) log_error("couldn't get function pointer to \"glGenFramebuffers\"!");
	if(gl_api._glCheckFramebufferStatus_ptr == nullptr) log_error("couldn't get function pointer to \"glCheckFramebufferStatus\"!");
	if(gl_api._glFramebufferTexture1D_ptr == nullptr) log_error("couldn't get function pointer to \"glFramebufferTexture1D\"!");
	if(gl_api._glFramebufferTexture2D_ptr == nullptr) log_error("couldn't get function pointer to \"glFramebufferTexture2D\"!");
	if(gl_api._glFramebufferTexture3D_ptr == nullptr) log_error("couldn't get function pointer to \"glFramebufferTexture3D\"!");
	if(gl_api._glFramebufferRenderbuffer_ptr == nullptr) log_error("couldn't get function pointer to \"glFramebufferRenderbuffer\"!");
	if(gl_api._glGetFramebufferAttachmentParameteriv_ptr == nullptr) log_error("couldn't get function pointer to \"glGetFramebufferAttachmentParameteriv\"!");
	if(gl_api._glGenerateMipmap_ptr == nullptr) log_error("couldn't get function pointer to \"glGenerateMipmap\"!");
	if(gl_api._glBlitFramebuffer_ptr == nullptr) log_error("couldn't get function pointer to \"glBlitFramebuffer\"!");
}

#endif
