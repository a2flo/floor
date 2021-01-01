/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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

#ifndef __FLOOR_GL_SUPPORT_HPP__
#define __FLOOR_GL_SUPPORT_HPP__

#if defined(__APPLE__)

#if !defined(FLOOR_IOS)
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

extern "C" {
	extern void glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
	extern void glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ,
								   GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ,
								   GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
}
#else

#define CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE 0x10000000

// arm64 (gl es 3.0 hardware)
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>

// generic redefines (apply to both es 2.0 and 3.0)
#define glClearDepth glClearDepthf

#endif

// we only need to get opengl functions pointers on windows, linux, *bsd, ...
#else
#include <floor/core/platform.hpp>

#if defined(MINGW)
#define GL3_PROTOTYPES // old glcorearb.h
#define GL_GLEXT_PROTOTYPES // new glcorearb.h
#endif
#if !defined(_MSC_VER)
#define _WINDOWS_
#include <GL/glcorearb.h>
#include <GL/glext.h>
#else
#include <gl/glcorearb.h>
#include <gl/glext.h>
#endif
#if !defined(_WIN32)
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

//
void init_gl_funcs();

//
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#define GL_RENDERBUFFER_COVERAGE_SAMPLES_NV 0x8CAB
#define GL_RENDERBUFFER_COLOR_SAMPLES_NV  0x8E10
#define GL_MAX_MULTISAMPLE_COVERAGE_MODES_NV 0x8E11
#define GL_MULTISAMPLE_COVERAGE_MODES_NV  0x8E12
typedef void (APIENTRYP PFNGLRENDERBUFFERSTORAGEMULTISAMPLECOVERAGENVPROC) (GLenum target, GLsizei coverageSamples, GLsizei colorSamples, GLenum internalformat, GLsizei width, GLsizei height);

struct gl_api_ptrs {
	PFNGLRENDERBUFFERSTORAGEMULTISAMPLECOVERAGENVPROC _glRenderbufferStorageMultisampleCoverageNV_ptr; // NV_framebuffer_multisample_coverage

#if !defined(__LINUX__) // gl 1.0 - 1.3 are already defined in linux
#if defined(__WINDOWS__) && !defined(MINGW) // windows no longer supports any opengl
// OpenGL 1.0
	PFNGLCULLFACEPROC _glCullFace_ptr;
	PFNGLFRONTFACEPROC _glFrontFace_ptr;
	PFNGLHINTPROC _glHint_ptr;
	PFNGLLINEWIDTHPROC _glLineWidth_ptr;
	PFNGLPOINTSIZEPROC _glPointSize_ptr;
	PFNGLPOLYGONMODEPROC _glPolygonMode_ptr;
	PFNGLSCISSORPROC _glScissor_ptr;
	PFNGLTEXPARAMETERFPROC _glTexParameterf_ptr;
	PFNGLTEXPARAMETERFVPROC _glTexParameterfv_ptr;
	PFNGLTEXPARAMETERIPROC _glTexParameteri_ptr;
	PFNGLTEXPARAMETERIVPROC _glTexParameteriv_ptr;
	PFNGLTEXIMAGE1DPROC _glTexImage1D_ptr;
	PFNGLTEXIMAGE2DPROC _glTexImage2D_ptr;
	PFNGLDRAWBUFFERPROC _glDrawBuffer_ptr;
	PFNGLCLEARPROC _glClear_ptr;
	PFNGLCLEARCOLORPROC _glClearColor_ptr;
	PFNGLCLEARSTENCILPROC _glClearStencil_ptr;
	PFNGLCLEARDEPTHPROC _glClearDepth_ptr;
	PFNGLSTENCILMASKPROC _glStencilMask_ptr;
	PFNGLCOLORMASKPROC _glColorMask_ptr;
	PFNGLDEPTHMASKPROC _glDepthMask_ptr;
	PFNGLDISABLEPROC _glDisable_ptr;
	PFNGLENABLEPROC _glEnable_ptr;
	PFNGLFINISHPROC _glFinish_ptr;
	PFNGLFLUSHPROC _glFlush_ptr;
	PFNGLBLENDFUNCPROC _glBlendFunc_ptr;
	PFNGLLOGICOPPROC _glLogicOp_ptr;
	PFNGLSTENCILFUNCPROC _glStencilFunc_ptr;
	PFNGLSTENCILOPPROC _glStencilOp_ptr;
	PFNGLDEPTHFUNCPROC _glDepthFunc_ptr;
	PFNGLPIXELSTOREFPROC _glPixelStoref_ptr;
	PFNGLPIXELSTOREIPROC _glPixelStorei_ptr;
	PFNGLREADBUFFERPROC _glReadBuffer_ptr;
	PFNGLREADPIXELSPROC _glReadPixels_ptr;
	PFNGLGETBOOLEANVPROC _glGetBooleanv_ptr;
	PFNGLGETDOUBLEVPROC _glGetDoublev_ptr;
	PFNGLGETERRORPROC _glGetError_ptr;
	PFNGLGETFLOATVPROC _glGetFloatv_ptr;
	PFNGLGETINTEGERVPROC _glGetIntegerv_ptr;
	PFNGLGETSTRINGPROC _glGetString_ptr;
	PFNGLGETTEXIMAGEPROC _glGetTexImage_ptr;
	PFNGLGETTEXPARAMETERFVPROC _glGetTexParameterfv_ptr;
	PFNGLGETTEXPARAMETERIVPROC _glGetTexParameteriv_ptr;
	PFNGLGETTEXLEVELPARAMETERFVPROC _glGetTexLevelParameterfv_ptr;
	PFNGLGETTEXLEVELPARAMETERIVPROC _glGetTexLevelParameteriv_ptr;
	PFNGLISENABLEDPROC _glIsEnabled_ptr;
	PFNGLDEPTHRANGEPROC _glDepthRange_ptr;
	PFNGLVIEWPORTPROC _glViewport_ptr;

	// OpenGL 1.1
	PFNGLDRAWARRAYSPROC _glDrawArrays_ptr;
	PFNGLDRAWELEMENTSPROC _glDrawElements_ptr;
	PFNGLGETPOINTERVPROC _glGetPointerv_ptr;
	PFNGLPOLYGONOFFSETPROC _glPolygonOffset_ptr;
	PFNGLCOPYTEXIMAGE1DPROC _glCopyTexImage1D_ptr;
	PFNGLCOPYTEXIMAGE2DPROC _glCopyTexImage2D_ptr;
	PFNGLCOPYTEXSUBIMAGE1DPROC _glCopyTexSubImage1D_ptr;
	PFNGLCOPYTEXSUBIMAGE2DPROC _glCopyTexSubImage2D_ptr;
	PFNGLTEXSUBIMAGE1DPROC _glTexSubImage1D_ptr;
	PFNGLTEXSUBIMAGE2DPROC _glTexSubImage2D_ptr;
	PFNGLBINDTEXTUREPROC _glBindTexture_ptr;
	PFNGLDELETETEXTURESPROC _glDeleteTextures_ptr;
	PFNGLGENTEXTURESPROC _glGenTextures_ptr;
	PFNGLISTEXTUREPROC _glIsTexture_ptr;

#endif

	// OpenGL 1.2
	PFNGLBLENDCOLORPROC _glBlendColor_ptr;
	PFNGLBLENDEQUATIONPROC _glBlendEquation_ptr;
	PFNGLDRAWRANGEELEMENTSPROC _glDrawRangeElements_ptr;
	PFNGLTEXIMAGE3DPROC _glTexImage3D_ptr;
	PFNGLTEXSUBIMAGE3DPROC _glTexSubImage3D_ptr;
	PFNGLCOPYTEXSUBIMAGE3DPROC _glCopyTexSubImage3D_ptr;

	// OpenGL 1.3
	PFNGLACTIVETEXTUREPROC _glActiveTexture_ptr;
	PFNGLSAMPLECOVERAGEPROC _glSampleCoverage_ptr;
	PFNGLCOMPRESSEDTEXIMAGE3DPROC _glCompressedTexImage3D_ptr;
	PFNGLCOMPRESSEDTEXIMAGE2DPROC _glCompressedTexImage2D_ptr;
	PFNGLCOMPRESSEDTEXIMAGE1DPROC _glCompressedTexImage1D_ptr;
	PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC _glCompressedTexSubImage3D_ptr;
	PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC _glCompressedTexSubImage2D_ptr;
	PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC _glCompressedTexSubImage1D_ptr;
	PFNGLGETCOMPRESSEDTEXIMAGEPROC _glGetCompressedTexImage_ptr;
#endif

	// OpenGL 1.4
	PFNGLBLENDFUNCSEPARATEPROC _glBlendFuncSeparate_ptr;
	PFNGLMULTIDRAWARRAYSPROC _glMultiDrawArrays_ptr;
	PFNGLMULTIDRAWELEMENTSPROC _glMultiDrawElements_ptr;
	PFNGLPOINTPARAMETERFPROC _glPointParameterf_ptr;
	PFNGLPOINTPARAMETERFVPROC _glPointParameterfv_ptr;
	PFNGLPOINTPARAMETERIPROC _glPointParameteri_ptr;
	PFNGLPOINTPARAMETERIVPROC _glPointParameteriv_ptr;

	// OpenGL 1.5
	PFNGLGENQUERIESPROC _glGenQueries_ptr;
	PFNGLDELETEQUERIESPROC _glDeleteQueries_ptr;
	PFNGLISQUERYPROC _glIsQuery_ptr;
	PFNGLBEGINQUERYPROC _glBeginQuery_ptr;
	PFNGLENDQUERYPROC _glEndQuery_ptr;
	PFNGLGETQUERYIVPROC _glGetQueryiv_ptr;
	PFNGLGETQUERYOBJECTIVPROC _glGetQueryObjectiv_ptr;
	PFNGLGETQUERYOBJECTUIVPROC _glGetQueryObjectuiv_ptr;
	PFNGLBINDBUFFERPROC _glBindBuffer_ptr;
	PFNGLDELETEBUFFERSPROC _glDeleteBuffers_ptr;
	PFNGLGENBUFFERSPROC _glGenBuffers_ptr;
	PFNGLISBUFFERPROC _glIsBuffer_ptr;
	PFNGLBUFFERDATAPROC _glBufferData_ptr;
	PFNGLBUFFERSUBDATAPROC _glBufferSubData_ptr;
	PFNGLGETBUFFERSUBDATAPROC _glGetBufferSubData_ptr;
	PFNGLMAPBUFFERPROC _glMapBuffer_ptr;
	PFNGLUNMAPBUFFERPROC _glUnmapBuffer_ptr;
	PFNGLGETBUFFERPARAMETERIVPROC _glGetBufferParameteriv_ptr;
	PFNGLGETBUFFERPOINTERVPROC _glGetBufferPointerv_ptr;

	// OpenGL 2.0
	PFNGLBLENDEQUATIONSEPARATEPROC _glBlendEquationSeparate_ptr;
	PFNGLDRAWBUFFERSPROC _glDrawBuffers_ptr;
	PFNGLSTENCILOPSEPARATEPROC _glStencilOpSeparate_ptr;
	PFNGLSTENCILFUNCSEPARATEPROC _glStencilFuncSeparate_ptr;
	PFNGLSTENCILMASKSEPARATEPROC _glStencilMaskSeparate_ptr;
	PFNGLATTACHSHADERPROC _glAttachShader_ptr;
	PFNGLBINDATTRIBLOCATIONPROC _glBindAttribLocation_ptr;
	PFNGLCOMPILESHADERPROC _glCompileShader_ptr;
	PFNGLCREATEPROGRAMPROC _glCreateProgram_ptr;
	PFNGLCREATESHADERPROC _glCreateShader_ptr;
	PFNGLDELETEPROGRAMPROC _glDeleteProgram_ptr;
	PFNGLDELETESHADERPROC _glDeleteShader_ptr;
	PFNGLDETACHSHADERPROC _glDetachShader_ptr;
	PFNGLDISABLEVERTEXATTRIBARRAYPROC _glDisableVertexAttribArray_ptr;
	PFNGLENABLEVERTEXATTRIBARRAYPROC _glEnableVertexAttribArray_ptr;
	PFNGLGETACTIVEATTRIBPROC _glGetActiveAttrib_ptr;
	PFNGLGETACTIVEUNIFORMPROC _glGetActiveUniform_ptr;
	PFNGLGETATTACHEDSHADERSPROC _glGetAttachedShaders_ptr;
	PFNGLGETATTRIBLOCATIONPROC _glGetAttribLocation_ptr;
	PFNGLGETPROGRAMIVPROC _glGetProgramiv_ptr;
	PFNGLGETPROGRAMINFOLOGPROC _glGetProgramInfoLog_ptr;
	PFNGLGETSHADERIVPROC _glGetShaderiv_ptr;
	PFNGLGETSHADERINFOLOGPROC _glGetShaderInfoLog_ptr;
	PFNGLGETSHADERSOURCEPROC _glGetShaderSource_ptr;
	PFNGLGETUNIFORMLOCATIONPROC _glGetUniformLocation_ptr;
	PFNGLGETUNIFORMFVPROC _glGetUniformfv_ptr;
	PFNGLGETUNIFORMIVPROC _glGetUniformiv_ptr;
	PFNGLGETVERTEXATTRIBDVPROC _glGetVertexAttribdv_ptr;
	PFNGLGETVERTEXATTRIBFVPROC _glGetVertexAttribfv_ptr;
	PFNGLGETVERTEXATTRIBIVPROC _glGetVertexAttribiv_ptr;
	PFNGLGETVERTEXATTRIBPOINTERVPROC _glGetVertexAttribPointerv_ptr;
	PFNGLISPROGRAMPROC _glIsProgram_ptr;
	PFNGLISSHADERPROC _glIsShader_ptr;
	PFNGLLINKPROGRAMPROC _glLinkProgram_ptr;
	PFNGLSHADERSOURCEPROC _glShaderSource_ptr;
	PFNGLUSEPROGRAMPROC _glUseProgram_ptr;
	PFNGLUNIFORM1FPROC _glUniform1f_ptr;
	PFNGLUNIFORM2FPROC _glUniform2f_ptr;
	PFNGLUNIFORM3FPROC _glUniform3f_ptr;
	PFNGLUNIFORM4FPROC _glUniform4f_ptr;
	PFNGLUNIFORM1IPROC _glUniform1i_ptr;
	PFNGLUNIFORM2IPROC _glUniform2i_ptr;
	PFNGLUNIFORM3IPROC _glUniform3i_ptr;
	PFNGLUNIFORM4IPROC _glUniform4i_ptr;
	PFNGLUNIFORM1FVPROC _glUniform1fv_ptr;
	PFNGLUNIFORM2FVPROC _glUniform2fv_ptr;
	PFNGLUNIFORM3FVPROC _glUniform3fv_ptr;
	PFNGLUNIFORM4FVPROC _glUniform4fv_ptr;
	PFNGLUNIFORM1IVPROC _glUniform1iv_ptr;
	PFNGLUNIFORM2IVPROC _glUniform2iv_ptr;
	PFNGLUNIFORM3IVPROC _glUniform3iv_ptr;
	PFNGLUNIFORM4IVPROC _glUniform4iv_ptr;
	PFNGLUNIFORMMATRIX2FVPROC _glUniformMatrix2fv_ptr;
	PFNGLUNIFORMMATRIX3FVPROC _glUniformMatrix3fv_ptr;
	PFNGLUNIFORMMATRIX4FVPROC _glUniformMatrix4fv_ptr;
	PFNGLVALIDATEPROGRAMPROC _glValidateProgram_ptr;
	PFNGLVERTEXATTRIB1DPROC _glVertexAttrib1d_ptr;
	PFNGLVERTEXATTRIB1DVPROC _glVertexAttrib1dv_ptr;
	PFNGLVERTEXATTRIB1FPROC _glVertexAttrib1f_ptr;
	PFNGLVERTEXATTRIB1FVPROC _glVertexAttrib1fv_ptr;
	PFNGLVERTEXATTRIB1SPROC _glVertexAttrib1s_ptr;
	PFNGLVERTEXATTRIB1SVPROC _glVertexAttrib1sv_ptr;
	PFNGLVERTEXATTRIB2DPROC _glVertexAttrib2d_ptr;
	PFNGLVERTEXATTRIB2DVPROC _glVertexAttrib2dv_ptr;
	PFNGLVERTEXATTRIB2FPROC _glVertexAttrib2f_ptr;
	PFNGLVERTEXATTRIB2FVPROC _glVertexAttrib2fv_ptr;
	PFNGLVERTEXATTRIB2SPROC _glVertexAttrib2s_ptr;
	PFNGLVERTEXATTRIB2SVPROC _glVertexAttrib2sv_ptr;
	PFNGLVERTEXATTRIB3DPROC _glVertexAttrib3d_ptr;
	PFNGLVERTEXATTRIB3DVPROC _glVertexAttrib3dv_ptr;
	PFNGLVERTEXATTRIB3FPROC _glVertexAttrib3f_ptr;
	PFNGLVERTEXATTRIB3FVPROC _glVertexAttrib3fv_ptr;
	PFNGLVERTEXATTRIB3SPROC _glVertexAttrib3s_ptr;
	PFNGLVERTEXATTRIB3SVPROC _glVertexAttrib3sv_ptr;
	PFNGLVERTEXATTRIB4NBVPROC _glVertexAttrib4Nbv_ptr;
	PFNGLVERTEXATTRIB4NIVPROC _glVertexAttrib4Niv_ptr;
	PFNGLVERTEXATTRIB4NSVPROC _glVertexAttrib4Nsv_ptr;
	PFNGLVERTEXATTRIB4NUBPROC _glVertexAttrib4Nub_ptr;
	PFNGLVERTEXATTRIB4NUBVPROC _glVertexAttrib4Nubv_ptr;
	PFNGLVERTEXATTRIB4NUIVPROC _glVertexAttrib4Nuiv_ptr;
	PFNGLVERTEXATTRIB4NUSVPROC _glVertexAttrib4Nusv_ptr;
	PFNGLVERTEXATTRIB4BVPROC _glVertexAttrib4bv_ptr;
	PFNGLVERTEXATTRIB4DPROC _glVertexAttrib4d_ptr;
	PFNGLVERTEXATTRIB4DVPROC _glVertexAttrib4dv_ptr;
	PFNGLVERTEXATTRIB4FPROC _glVertexAttrib4f_ptr;
	PFNGLVERTEXATTRIB4FVPROC _glVertexAttrib4fv_ptr;
	PFNGLVERTEXATTRIB4IVPROC _glVertexAttrib4iv_ptr;
	PFNGLVERTEXATTRIB4SPROC _glVertexAttrib4s_ptr;
	PFNGLVERTEXATTRIB4SVPROC _glVertexAttrib4sv_ptr;
	PFNGLVERTEXATTRIB4UBVPROC _glVertexAttrib4ubv_ptr;
	PFNGLVERTEXATTRIB4UIVPROC _glVertexAttrib4uiv_ptr;
	PFNGLVERTEXATTRIB4USVPROC _glVertexAttrib4usv_ptr;
	PFNGLVERTEXATTRIBPOINTERPROC _glVertexAttribPointer_ptr;

	// OpenGL 2.1
	PFNGLUNIFORMMATRIX2X3FVPROC _glUniformMatrix2x3fv_ptr;
	PFNGLUNIFORMMATRIX3X2FVPROC _glUniformMatrix3x2fv_ptr;
	PFNGLUNIFORMMATRIX2X4FVPROC _glUniformMatrix2x4fv_ptr;
	PFNGLUNIFORMMATRIX4X2FVPROC _glUniformMatrix4x2fv_ptr;
	PFNGLUNIFORMMATRIX3X4FVPROC _glUniformMatrix3x4fv_ptr;
	PFNGLUNIFORMMATRIX4X3FVPROC _glUniformMatrix4x3fv_ptr;

	// OpenGL 3.0
	PFNGLCOLORMASKIPROC _glColorMaski_ptr;
	PFNGLGETBOOLEANI_VPROC _glGetBooleani_v_ptr;
	PFNGLGETINTEGERI_VPROC _glGetIntegeri_v_ptr;
	PFNGLENABLEIPROC _glEnablei_ptr;
	PFNGLDISABLEIPROC _glDisablei_ptr;
	PFNGLISENABLEDIPROC _glIsEnabledi_ptr;
	PFNGLBEGINTRANSFORMFEEDBACKPROC _glBeginTransformFeedback_ptr;
	PFNGLENDTRANSFORMFEEDBACKPROC _glEndTransformFeedback_ptr;
	PFNGLBINDBUFFERRANGEPROC _glBindBufferRange_ptr;
	PFNGLBINDBUFFERBASEPROC _glBindBufferBase_ptr;
	PFNGLTRANSFORMFEEDBACKVARYINGSPROC _glTransformFeedbackVaryings_ptr;
	PFNGLGETTRANSFORMFEEDBACKVARYINGPROC _glGetTransformFeedbackVarying_ptr;
	PFNGLCLAMPCOLORPROC _glClampColor_ptr;
	PFNGLBEGINCONDITIONALRENDERPROC _glBeginConditionalRender_ptr;
	PFNGLENDCONDITIONALRENDERPROC _glEndConditionalRender_ptr;
	PFNGLVERTEXATTRIBIPOINTERPROC _glVertexAttribIPointer_ptr;
	PFNGLGETVERTEXATTRIBIIVPROC _glGetVertexAttribIiv_ptr;
	PFNGLGETVERTEXATTRIBIUIVPROC _glGetVertexAttribIuiv_ptr;
	PFNGLVERTEXATTRIBI1IPROC _glVertexAttribI1i_ptr;
	PFNGLVERTEXATTRIBI2IPROC _glVertexAttribI2i_ptr;
	PFNGLVERTEXATTRIBI3IPROC _glVertexAttribI3i_ptr;
	PFNGLVERTEXATTRIBI4IPROC _glVertexAttribI4i_ptr;
	PFNGLVERTEXATTRIBI1UIPROC _glVertexAttribI1ui_ptr;
	PFNGLVERTEXATTRIBI2UIPROC _glVertexAttribI2ui_ptr;
	PFNGLVERTEXATTRIBI3UIPROC _glVertexAttribI3ui_ptr;
	PFNGLVERTEXATTRIBI4UIPROC _glVertexAttribI4ui_ptr;
	PFNGLVERTEXATTRIBI1IVPROC _glVertexAttribI1iv_ptr;
	PFNGLVERTEXATTRIBI2IVPROC _glVertexAttribI2iv_ptr;
	PFNGLVERTEXATTRIBI3IVPROC _glVertexAttribI3iv_ptr;
	PFNGLVERTEXATTRIBI4IVPROC _glVertexAttribI4iv_ptr;
	PFNGLVERTEXATTRIBI1UIVPROC _glVertexAttribI1uiv_ptr;
	PFNGLVERTEXATTRIBI2UIVPROC _glVertexAttribI2uiv_ptr;
	PFNGLVERTEXATTRIBI3UIVPROC _glVertexAttribI3uiv_ptr;
	PFNGLVERTEXATTRIBI4UIVPROC _glVertexAttribI4uiv_ptr;
	PFNGLVERTEXATTRIBI4BVPROC _glVertexAttribI4bv_ptr;
	PFNGLVERTEXATTRIBI4SVPROC _glVertexAttribI4sv_ptr;
	PFNGLVERTEXATTRIBI4UBVPROC _glVertexAttribI4ubv_ptr;
	PFNGLVERTEXATTRIBI4USVPROC _glVertexAttribI4usv_ptr;
	PFNGLGETUNIFORMUIVPROC _glGetUniformuiv_ptr;
	PFNGLBINDFRAGDATALOCATIONPROC _glBindFragDataLocation_ptr;
	PFNGLGETFRAGDATALOCATIONPROC _glGetFragDataLocation_ptr;
	PFNGLUNIFORM1UIPROC _glUniform1ui_ptr;
	PFNGLUNIFORM2UIPROC _glUniform2ui_ptr;
	PFNGLUNIFORM3UIPROC _glUniform3ui_ptr;
	PFNGLUNIFORM4UIPROC _glUniform4ui_ptr;
	PFNGLUNIFORM1UIVPROC _glUniform1uiv_ptr;
	PFNGLUNIFORM2UIVPROC _glUniform2uiv_ptr;
	PFNGLUNIFORM3UIVPROC _glUniform3uiv_ptr;
	PFNGLUNIFORM4UIVPROC _glUniform4uiv_ptr;
	PFNGLTEXPARAMETERIIVPROC _glTexParameterIiv_ptr;
	PFNGLTEXPARAMETERIUIVPROC _glTexParameterIuiv_ptr;
	PFNGLGETTEXPARAMETERIIVPROC _glGetTexParameterIiv_ptr;
	PFNGLGETTEXPARAMETERIUIVPROC _glGetTexParameterIuiv_ptr;
	PFNGLCLEARBUFFERIVPROC _glClearBufferiv_ptr;
	PFNGLCLEARBUFFERUIVPROC _glClearBufferuiv_ptr;
	PFNGLCLEARBUFFERFVPROC _glClearBufferfv_ptr;
	PFNGLCLEARBUFFERFIPROC _glClearBufferfi_ptr;
	PFNGLGETSTRINGIPROC _glGetStringi_ptr;
	PFNGLISRENDERBUFFERPROC _glIsRenderbuffer_ptr; // ARB_framebuffer_object
	PFNGLBINDRENDERBUFFERPROC _glBindRenderbuffer_ptr; // ARB_framebuffer_object
	PFNGLDELETERENDERBUFFERSPROC _glDeleteRenderbuffers_ptr; // ARB_framebuffer_object
	PFNGLGENRENDERBUFFERSPROC _glGenRenderbuffers_ptr; // ARB_framebuffer_object
	PFNGLRENDERBUFFERSTORAGEPROC _glRenderbufferStorage_ptr; // ARB_framebuffer_object
	PFNGLGETRENDERBUFFERPARAMETERIVPROC _glGetRenderbufferParameteriv_ptr; // ARB_framebuffer_object
	PFNGLISFRAMEBUFFERPROC _glIsFramebuffer_ptr; // ARB_framebuffer_object
	PFNGLBINDFRAMEBUFFERPROC _glBindFramebuffer_ptr; // ARB_framebuffer_object
	PFNGLDELETEFRAMEBUFFERSPROC _glDeleteFramebuffers_ptr; // ARB_framebuffer_object
	PFNGLGENFRAMEBUFFERSPROC _glGenFramebuffers_ptr; // ARB_framebuffer_object
	PFNGLCHECKFRAMEBUFFERSTATUSPROC _glCheckFramebufferStatus_ptr; // ARB_framebuffer_object
	PFNGLFRAMEBUFFERTEXTURE1DPROC _glFramebufferTexture1D_ptr; // ARB_framebuffer_object
	PFNGLFRAMEBUFFERTEXTURE2DPROC _glFramebufferTexture2D_ptr; // ARB_framebuffer_object
	PFNGLFRAMEBUFFERTEXTURE3DPROC _glFramebufferTexture3D_ptr; // ARB_framebuffer_object
	PFNGLFRAMEBUFFERRENDERBUFFERPROC _glFramebufferRenderbuffer_ptr; // ARB_framebuffer_object
	PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC _glGetFramebufferAttachmentParameteriv_ptr; // ARB_framebuffer_object
	PFNGLGENERATEMIPMAPPROC _glGenerateMipmap_ptr; // ARB_framebuffer_object
	PFNGLBLITFRAMEBUFFERPROC _glBlitFramebuffer_ptr; // ARB_framebuffer_object
	PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC _glRenderbufferStorageMultisample_ptr; // ARB_framebuffer_object
	PFNGLFRAMEBUFFERTEXTURELAYERPROC _glFramebufferTextureLayer_ptr; // ARB_framebuffer_object
	PFNGLMAPBUFFERRANGEPROC _glMapBufferRange_ptr; // ARB_map_buffer_range
	PFNGLFLUSHMAPPEDBUFFERRANGEPROC _glFlushMappedBufferRange_ptr; // ARB_map_buffer_range
	PFNGLBINDVERTEXARRAYPROC _glBindVertexArray_ptr; // ARB_vertex_array_object
	PFNGLDELETEVERTEXARRAYSPROC _glDeleteVertexArrays_ptr; // ARB_vertex_array_object
	PFNGLGENVERTEXARRAYSPROC _glGenVertexArrays_ptr; // ARB_vertex_array_object
	PFNGLISVERTEXARRAYPROC _glIsVertexArray_ptr; // ARB_vertex_array_object

	// OpenGL 3.1
	PFNGLDRAWARRAYSINSTANCEDPROC _glDrawArraysInstanced_ptr;
	PFNGLDRAWELEMENTSINSTANCEDPROC _glDrawElementsInstanced_ptr;
	PFNGLTEXBUFFERPROC _glTexBuffer_ptr;
	PFNGLPRIMITIVERESTARTINDEXPROC _glPrimitiveRestartIndex_ptr;
	PFNGLCOPYBUFFERSUBDATAPROC _glCopyBufferSubData_ptr; // ARB_copy_buffer
	PFNGLGETUNIFORMINDICESPROC _glGetUniformIndices_ptr; // ARB_uniform_buffer_object
	PFNGLGETACTIVEUNIFORMSIVPROC _glGetActiveUniformsiv_ptr; // ARB_uniform_buffer_object
	PFNGLGETACTIVEUNIFORMNAMEPROC _glGetActiveUniformName_ptr; // ARB_uniform_buffer_object
	PFNGLGETUNIFORMBLOCKINDEXPROC _glGetUniformBlockIndex_ptr; // ARB_uniform_buffer_object
	PFNGLGETACTIVEUNIFORMBLOCKIVPROC _glGetActiveUniformBlockiv_ptr; // ARB_uniform_buffer_object
	PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC _glGetActiveUniformBlockName_ptr; // ARB_uniform_buffer_object
	PFNGLUNIFORMBLOCKBINDINGPROC _glUniformBlockBinding_ptr; // ARB_uniform_buffer_object

	// OpenGL 3.2
	PFNGLGETINTEGER64I_VPROC _glGetInteger64i_v_ptr;
	PFNGLGETBUFFERPARAMETERI64VPROC _glGetBufferParameteri64v_ptr;
	PFNGLFRAMEBUFFERTEXTUREPROC _glFramebufferTexture_ptr;
	PFNGLDRAWELEMENTSBASEVERTEXPROC _glDrawElementsBaseVertex_ptr; // ARB_draw_elements_base_vertex
	PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC _glDrawRangeElementsBaseVertex_ptr; // ARB_draw_elements_base_vertex
	PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC _glDrawElementsInstancedBaseVertex_ptr; // ARB_draw_elements_base_vertex
	PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC _glMultiDrawElementsBaseVertex_ptr; // ARB_draw_elements_base_vertex
	PFNGLPROVOKINGVERTEXPROC _glProvokingVertex_ptr; // ARB_provoking_vertex
	PFNGLFENCESYNCPROC _glFenceSync_ptr; // ARB_sync
	PFNGLISSYNCPROC _glIsSync_ptr; // ARB_sync
	PFNGLDELETESYNCPROC _glDeleteSync_ptr; // ARB_sync
	PFNGLCLIENTWAITSYNCPROC _glClientWaitSync_ptr; // ARB_sync
	PFNGLWAITSYNCPROC _glWaitSync_ptr; // ARB_sync
	PFNGLGETINTEGER64VPROC _glGetInteger64v_ptr; // ARB_sync
	PFNGLGETSYNCIVPROC _glGetSynciv_ptr; // ARB_sync
	PFNGLTEXIMAGE2DMULTISAMPLEPROC _glTexImage2DMultisample_ptr; // ARB_texture_multisample
	PFNGLTEXIMAGE3DMULTISAMPLEPROC _glTexImage3DMultisample_ptr; // ARB_texture_multisample
	PFNGLGETMULTISAMPLEFVPROC _glGetMultisamplefv_ptr; // ARB_texture_multisample
	PFNGLSAMPLEMASKIPROC _glSampleMaski_ptr; // ARB_texture_multisample

	// OpenGL 3.3
	PFNGLVERTEXATTRIBDIVISORPROC _glVertexAttribDivisor_ptr;
	PFNGLBINDFRAGDATALOCATIONINDEXEDPROC _glBindFragDataLocationIndexed_ptr; // ARB_blend_func_extended
	PFNGLGETFRAGDATAINDEXPROC _glGetFragDataIndex_ptr; // ARB_blend_func_extended
	PFNGLGENSAMPLERSPROC _glGenSamplers_ptr; // ARB_sampler_objects
	PFNGLDELETESAMPLERSPROC _glDeleteSamplers_ptr; // ARB_sampler_objects
	PFNGLISSAMPLERPROC _glIsSampler_ptr; // ARB_sampler_objects
	PFNGLBINDSAMPLERPROC _glBindSampler_ptr; // ARB_sampler_objects
	PFNGLSAMPLERPARAMETERIPROC _glSamplerParameteri_ptr; // ARB_sampler_objects
	PFNGLSAMPLERPARAMETERIVPROC _glSamplerParameteriv_ptr; // ARB_sampler_objects
	PFNGLSAMPLERPARAMETERFPROC _glSamplerParameterf_ptr; // ARB_sampler_objects
	PFNGLSAMPLERPARAMETERFVPROC _glSamplerParameterfv_ptr; // ARB_sampler_objects
	PFNGLSAMPLERPARAMETERIIVPROC _glSamplerParameterIiv_ptr; // ARB_sampler_objects
	PFNGLSAMPLERPARAMETERIUIVPROC _glSamplerParameterIuiv_ptr; // ARB_sampler_objects
	PFNGLGETSAMPLERPARAMETERIVPROC _glGetSamplerParameteriv_ptr; // ARB_sampler_objects
	PFNGLGETSAMPLERPARAMETERIIVPROC _glGetSamplerParameterIiv_ptr; // ARB_sampler_objects
	PFNGLGETSAMPLERPARAMETERFVPROC _glGetSamplerParameterfv_ptr; // ARB_sampler_objects
	PFNGLGETSAMPLERPARAMETERIUIVPROC _glGetSamplerParameterIuiv_ptr; // ARB_sampler_objects
	PFNGLQUERYCOUNTERPROC _glQueryCounter_ptr; // ARB_timer_query
	PFNGLGETQUERYOBJECTI64VPROC _glGetQueryObjecti64v_ptr; // ARB_timer_query
	PFNGLGETQUERYOBJECTUI64VPROC _glGetQueryObjectui64v_ptr; // ARB_timer_query
	PFNGLVERTEXATTRIBP1UIPROC _glVertexAttribP1ui_ptr; // ARB_vertex_type_2_10_10_10_rev
	PFNGLVERTEXATTRIBP1UIVPROC _glVertexAttribP1uiv_ptr; // ARB_vertex_type_2_10_10_10_rev
	PFNGLVERTEXATTRIBP2UIPROC _glVertexAttribP2ui_ptr; // ARB_vertex_type_2_10_10_10_rev
	PFNGLVERTEXATTRIBP2UIVPROC _glVertexAttribP2uiv_ptr; // ARB_vertex_type_2_10_10_10_rev
	PFNGLVERTEXATTRIBP3UIPROC _glVertexAttribP3ui_ptr; // ARB_vertex_type_2_10_10_10_rev
	PFNGLVERTEXATTRIBP3UIVPROC _glVertexAttribP3uiv_ptr; // ARB_vertex_type_2_10_10_10_rev
	PFNGLVERTEXATTRIBP4UIPROC _glVertexAttribP4ui_ptr; // ARB_vertex_type_2_10_10_10_rev
	PFNGLVERTEXATTRIBP4UIVPROC _glVertexAttribP4uiv_ptr; // ARB_vertex_type_2_10_10_10_rev

	// OpenGL 4.0
	PFNGLMINSAMPLESHADINGPROC _glMinSampleShading_ptr;
	PFNGLBLENDEQUATIONIPROC _glBlendEquationi_ptr;
	PFNGLBLENDEQUATIONSEPARATEIPROC _glBlendEquationSeparatei_ptr;
	PFNGLBLENDFUNCIPROC _glBlendFunci_ptr;
	PFNGLBLENDFUNCSEPARATEIPROC _glBlendFuncSeparatei_ptr;
	PFNGLDRAWARRAYSINDIRECTPROC _glDrawArraysIndirect_ptr; // ARB_draw_indirect
	PFNGLDRAWELEMENTSINDIRECTPROC _glDrawElementsIndirect_ptr; // ARB_draw_indirect
	PFNGLUNIFORM1DPROC _glUniform1d_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORM2DPROC _glUniform2d_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORM3DPROC _glUniform3d_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORM4DPROC _glUniform4d_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORM1DVPROC _glUniform1dv_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORM2DVPROC _glUniform2dv_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORM3DVPROC _glUniform3dv_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORM4DVPROC _glUniform4dv_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORMMATRIX2DVPROC _glUniformMatrix2dv_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORMMATRIX3DVPROC _glUniformMatrix3dv_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORMMATRIX4DVPROC _glUniformMatrix4dv_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORMMATRIX2X3DVPROC _glUniformMatrix2x3dv_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORMMATRIX2X4DVPROC _glUniformMatrix2x4dv_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORMMATRIX3X2DVPROC _glUniformMatrix3x2dv_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORMMATRIX3X4DVPROC _glUniformMatrix3x4dv_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORMMATRIX4X2DVPROC _glUniformMatrix4x2dv_ptr; // ARB_gpu_shader_fp64
	PFNGLUNIFORMMATRIX4X3DVPROC _glUniformMatrix4x3dv_ptr; // ARB_gpu_shader_fp64
	PFNGLGETUNIFORMDVPROC _glGetUniformdv_ptr; // ARB_gpu_shader_fp64
	PFNGLGETSUBROUTINEUNIFORMLOCATIONPROC _glGetSubroutineUniformLocation_ptr; // ARB_shader_subroutine
	PFNGLGETSUBROUTINEINDEXPROC _glGetSubroutineIndex_ptr; // ARB_shader_subroutine
	PFNGLGETACTIVESUBROUTINEUNIFORMIVPROC _glGetActiveSubroutineUniformiv_ptr; // ARB_shader_subroutine
	PFNGLGETACTIVESUBROUTINEUNIFORMNAMEPROC _glGetActiveSubroutineUniformName_ptr; // ARB_shader_subroutine
	PFNGLGETACTIVESUBROUTINENAMEPROC _glGetActiveSubroutineName_ptr; // ARB_shader_subroutine
	PFNGLUNIFORMSUBROUTINESUIVPROC _glUniformSubroutinesuiv_ptr; // ARB_shader_subroutine
	PFNGLGETUNIFORMSUBROUTINEUIVPROC _glGetUniformSubroutineuiv_ptr; // ARB_shader_subroutine
	PFNGLGETPROGRAMSTAGEIVPROC _glGetProgramStageiv_ptr; // ARB_shader_subroutine
	PFNGLPATCHPARAMETERIPROC _glPatchParameteri_ptr; // ARB_tessellation_shader
	PFNGLPATCHPARAMETERFVPROC _glPatchParameterfv_ptr; // ARB_tessellation_shader
	PFNGLBINDTRANSFORMFEEDBACKPROC _glBindTransformFeedback_ptr; // ARB_transform_feedback2
	PFNGLDELETETRANSFORMFEEDBACKSPROC _glDeleteTransformFeedbacks_ptr; // ARB_transform_feedback2
	PFNGLGENTRANSFORMFEEDBACKSPROC _glGenTransformFeedbacks_ptr; // ARB_transform_feedback2
	PFNGLISTRANSFORMFEEDBACKPROC _glIsTransformFeedback_ptr; // ARB_transform_feedback2
	PFNGLPAUSETRANSFORMFEEDBACKPROC _glPauseTransformFeedback_ptr; // ARB_transform_feedback2
	PFNGLRESUMETRANSFORMFEEDBACKPROC _glResumeTransformFeedback_ptr; // ARB_transform_feedback2
	PFNGLDRAWTRANSFORMFEEDBACKPROC _glDrawTransformFeedback_ptr; // ARB_transform_feedback2
	PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC _glDrawTransformFeedbackStream_ptr; // ARB_transform_feedback3
	PFNGLBEGINQUERYINDEXEDPROC _glBeginQueryIndexed_ptr; // ARB_transform_feedback3
	PFNGLENDQUERYINDEXEDPROC _glEndQueryIndexed_ptr; // ARB_transform_feedback3
	PFNGLGETQUERYINDEXEDIVPROC _glGetQueryIndexediv_ptr; // ARB_transform_feedback3

	// OpenGL 4.1
	PFNGLRELEASESHADERCOMPILERPROC _glReleaseShaderCompiler_ptr; // ARB_ES2_compatibility
	PFNGLSHADERBINARYPROC _glShaderBinary_ptr; // ARB_ES2_compatibility
	PFNGLGETSHADERPRECISIONFORMATPROC _glGetShaderPrecisionFormat_ptr; // ARB_ES2_compatibility
	PFNGLDEPTHRANGEFPROC _glDepthRangef_ptr; // ARB_ES2_compatibility
	PFNGLCLEARDEPTHFPROC _glClearDepthf_ptr; // ARB_ES2_compatibility
	PFNGLGETPROGRAMBINARYPROC _glGetProgramBinary_ptr; // ARB_get_program_binary
	PFNGLPROGRAMBINARYPROC _glProgramBinary_ptr; // ARB_get_program_binary
	PFNGLPROGRAMPARAMETERIPROC _glProgramParameteri_ptr; // ARB_get_program_binary
	PFNGLUSEPROGRAMSTAGESPROC _glUseProgramStages_ptr; // ARB_separate_shader_objects
	PFNGLACTIVESHADERPROGRAMPROC _glActiveShaderProgram_ptr; // ARB_separate_shader_objects
	PFNGLCREATESHADERPROGRAMVPROC _glCreateShaderProgramv_ptr; // ARB_separate_shader_objects
	PFNGLBINDPROGRAMPIPELINEPROC _glBindProgramPipeline_ptr; // ARB_separate_shader_objects
	PFNGLDELETEPROGRAMPIPELINESPROC _glDeleteProgramPipelines_ptr; // ARB_separate_shader_objects
	PFNGLGENPROGRAMPIPELINESPROC _glGenProgramPipelines_ptr; // ARB_separate_shader_objects
	PFNGLISPROGRAMPIPELINEPROC _glIsProgramPipeline_ptr; // ARB_separate_shader_objects
	PFNGLGETPROGRAMPIPELINEIVPROC _glGetProgramPipelineiv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM1IPROC _glProgramUniform1i_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM1IVPROC _glProgramUniform1iv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM1FPROC _glProgramUniform1f_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM1FVPROC _glProgramUniform1fv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM1DPROC _glProgramUniform1d_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM1DVPROC _glProgramUniform1dv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM1UIPROC _glProgramUniform1ui_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM1UIVPROC _glProgramUniform1uiv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM2IPROC _glProgramUniform2i_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM2IVPROC _glProgramUniform2iv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM2FPROC _glProgramUniform2f_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM2FVPROC _glProgramUniform2fv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM2DPROC _glProgramUniform2d_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM2DVPROC _glProgramUniform2dv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM2UIPROC _glProgramUniform2ui_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM2UIVPROC _glProgramUniform2uiv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM3IPROC _glProgramUniform3i_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM3IVPROC _glProgramUniform3iv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM3FPROC _glProgramUniform3f_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM3FVPROC _glProgramUniform3fv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM3DPROC _glProgramUniform3d_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM3DVPROC _glProgramUniform3dv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM3UIPROC _glProgramUniform3ui_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM3UIVPROC _glProgramUniform3uiv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM4IPROC _glProgramUniform4i_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM4IVPROC _glProgramUniform4iv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM4FPROC _glProgramUniform4f_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM4FVPROC _glProgramUniform4fv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM4DPROC _glProgramUniform4d_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM4DVPROC _glProgramUniform4dv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM4UIPROC _glProgramUniform4ui_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORM4UIVPROC _glProgramUniform4uiv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX2FVPROC _glProgramUniformMatrix2fv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX3FVPROC _glProgramUniformMatrix3fv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX4FVPROC _glProgramUniformMatrix4fv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX2DVPROC _glProgramUniformMatrix2dv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX3DVPROC _glProgramUniformMatrix3dv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX4DVPROC _glProgramUniformMatrix4dv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC _glProgramUniformMatrix2x3fv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC _glProgramUniformMatrix3x2fv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC _glProgramUniformMatrix2x4fv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC _glProgramUniformMatrix4x2fv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC _glProgramUniformMatrix3x4fv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC _glProgramUniformMatrix4x3fv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX2X3DVPROC _glProgramUniformMatrix2x3dv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX3X2DVPROC _glProgramUniformMatrix3x2dv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX2X4DVPROC _glProgramUniformMatrix2x4dv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX4X2DVPROC _glProgramUniformMatrix4x2dv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX3X4DVPROC _glProgramUniformMatrix3x4dv_ptr; // ARB_separate_shader_objects
	PFNGLPROGRAMUNIFORMMATRIX4X3DVPROC _glProgramUniformMatrix4x3dv_ptr; // ARB_separate_shader_objects
	PFNGLVALIDATEPROGRAMPIPELINEPROC _glValidateProgramPipeline_ptr; // ARB_separate_shader_objects
	PFNGLGETPROGRAMPIPELINEINFOLOGPROC _glGetProgramPipelineInfoLog_ptr; // ARB_separate_shader_objects
	PFNGLVERTEXATTRIBL1DPROC _glVertexAttribL1d_ptr; // ARB_vertex_attrib_64bit
	PFNGLVERTEXATTRIBL2DPROC _glVertexAttribL2d_ptr; // ARB_vertex_attrib_64bit
	PFNGLVERTEXATTRIBL3DPROC _glVertexAttribL3d_ptr; // ARB_vertex_attrib_64bit
	PFNGLVERTEXATTRIBL4DPROC _glVertexAttribL4d_ptr; // ARB_vertex_attrib_64bit
	PFNGLVERTEXATTRIBL1DVPROC _glVertexAttribL1dv_ptr; // ARB_vertex_attrib_64bit
	PFNGLVERTEXATTRIBL2DVPROC _glVertexAttribL2dv_ptr; // ARB_vertex_attrib_64bit
	PFNGLVERTEXATTRIBL3DVPROC _glVertexAttribL3dv_ptr; // ARB_vertex_attrib_64bit
	PFNGLVERTEXATTRIBL4DVPROC _glVertexAttribL4dv_ptr; // ARB_vertex_attrib_64bit
	PFNGLVERTEXATTRIBLPOINTERPROC _glVertexAttribLPointer_ptr; // ARB_vertex_attrib_64bit
	PFNGLGETVERTEXATTRIBLDVPROC _glGetVertexAttribLdv_ptr; // ARB_vertex_attrib_64bit
	PFNGLVIEWPORTARRAYVPROC _glViewportArrayv_ptr; // ARB_viewport_array
	PFNGLVIEWPORTINDEXEDFPROC _glViewportIndexedf_ptr; // ARB_viewport_array
	PFNGLVIEWPORTINDEXEDFVPROC _glViewportIndexedfv_ptr; // ARB_viewport_array
	PFNGLSCISSORARRAYVPROC _glScissorArrayv_ptr; // ARB_viewport_array
	PFNGLSCISSORINDEXEDPROC _glScissorIndexed_ptr; // ARB_viewport_array
	PFNGLSCISSORINDEXEDVPROC _glScissorIndexedv_ptr; // ARB_viewport_array
	PFNGLDEPTHRANGEARRAYVPROC _glDepthRangeArrayv_ptr; // ARB_viewport_array
	PFNGLDEPTHRANGEINDEXEDPROC _glDepthRangeIndexed_ptr; // ARB_viewport_array
	PFNGLGETFLOATI_VPROC _glGetFloati_v_ptr; // ARB_viewport_array
	PFNGLGETDOUBLEI_VPROC _glGetDoublei_v_ptr; // ARB_viewport_array

	// OpenGL 4.3
	PFNGLCOPYIMAGESUBDATAPROC _glCopyImageSubData_ptr; // ARB_copy_image
};
extern FLOOR_DLL_API gl_api_ptrs gl_api;

// syntactic sugar
#define glRenderbufferStorageMultisampleCoverageNV ((PFNGLRENDERBUFFERSTORAGEMULTISAMPLECOVERAGENVPROC)gl_api._glRenderbufferStorageMultisampleCoverageNV_ptr)
#if !defined(__LINUX__)
#if defined(__WINDOWS__) && !defined(MINGW)
#define glCullFace ((PFNGLCULLFACEPROC)gl_api._glCullFace_ptr)
#define glFrontFace ((PFNGLFRONTFACEPROC)gl_api._glFrontFace_ptr)
#define glHint ((PFNGLHINTPROC)gl_api._glHint_ptr)
#define glLineWidth ((PFNGLLINEWIDTHPROC)gl_api._glLineWidth_ptr)
#define glPointSize ((PFNGLPOINTSIZEPROC)gl_api._glPointSize_ptr)
#define glPolygonMode ((PFNGLPOLYGONMODEPROC)gl_api._glPolygonMode_ptr)
#define glScissor ((PFNGLSCISSORPROC)gl_api._glScissor_ptr)
#define glTexParameterf ((PFNGLTEXPARAMETERFPROC)gl_api._glTexParameterf_ptr)
#define glTexParameterfv ((PFNGLTEXPARAMETERFVPROC)gl_api._glTexParameterfv_ptr)
#define glTexParameteri ((PFNGLTEXPARAMETERIPROC)gl_api._glTexParameteri_ptr)
#define glTexParameteriv ((PFNGLTEXPARAMETERIVPROC)gl_api._glTexParameteriv_ptr)
#define glTexImage1D ((PFNGLTEXIMAGE1DPROC)gl_api._glTexImage1D_ptr)
#define glTexImage2D ((PFNGLTEXIMAGE2DPROC)gl_api._glTexImage2D_ptr)
#define glDrawBuffer ((PFNGLDRAWBUFFERPROC)gl_api._glDrawBuffer_ptr)
#define glClear ((PFNGLCLEARPROC)gl_api._glClear_ptr)
#define glClearColor ((PFNGLCLEARCOLORPROC)gl_api._glClearColor_ptr)
#define glClearStencil ((PFNGLCLEARSTENCILPROC)gl_api._glClearStencil_ptr)
#define glClearDepth ((PFNGLCLEARDEPTHPROC)gl_api._glClearDepth_ptr)
#define glStencilMask ((PFNGLSTENCILMASKPROC)gl_api._glStencilMask_ptr)
#define glColorMask ((PFNGLCOLORMASKPROC)gl_api._glColorMask_ptr)
#define glDepthMask ((PFNGLDEPTHMASKPROC)gl_api._glDepthMask_ptr)
#define glDisable ((PFNGLDISABLEPROC)gl_api._glDisable_ptr)
#define glEnable ((PFNGLENABLEPROC)gl_api._glEnable_ptr)
#define glFinish ((PFNGLFINISHPROC)gl_api._glFinish_ptr)
#define glFlush ((PFNGLFLUSHPROC)gl_api._glFlush_ptr)
#define glBlendFunc ((PFNGLBLENDFUNCPROC)gl_api._glBlendFunc_ptr)
#define glLogicOp ((PFNGLLOGICOPPROC)gl_api._glLogicOp_ptr)
#define glStencilFunc ((PFNGLSTENCILFUNCPROC)gl_api._glStencilFunc_ptr)
#define glStencilOp ((PFNGLSTENCILOPPROC)gl_api._glStencilOp_ptr)
#define glDepthFunc ((PFNGLDEPTHFUNCPROC)gl_api._glDepthFunc_ptr)
#define glPixelStoref ((PFNGLPIXELSTOREFPROC)gl_api._glPixelStoref_ptr)
#define glPixelStorei ((PFNGLPIXELSTOREIPROC)gl_api._glPixelStorei_ptr)
#define glReadBuffer ((PFNGLREADBUFFERPROC)gl_api._glReadBuffer_ptr)
#define glReadPixels ((PFNGLREADPIXELSPROC)gl_api._glReadPixels_ptr)
#define glGetBooleanv ((PFNGLGETBOOLEANVPROC)gl_api._glGetBooleanv_ptr)
#define glGetDoublev ((PFNGLGETDOUBLEVPROC)gl_api._glGetDoublev_ptr)
#define glGetError ((PFNGLGETERRORPROC)gl_api._glGetError_ptr)
#define glGetFloatv ((PFNGLGETFLOATVPROC)gl_api._glGetFloatv_ptr)
#define glGetIntegerv ((PFNGLGETINTEGERVPROC)gl_api._glGetIntegerv_ptr)
#define glGetString ((PFNGLGETSTRINGPROC)gl_api._glGetString_ptr)
#define glGetTexImage ((PFNGLGETTEXIMAGEPROC)gl_api._glGetTexImage_ptr)
#define glGetTexParameterfv ((PFNGLGETTEXPARAMETERFVPROC)gl_api._glGetTexParameterfv_ptr)
#define glGetTexParameteriv ((PFNGLGETTEXPARAMETERIVPROC)gl_api._glGetTexParameteriv_ptr)
#define glGetTexLevelParameterfv ((PFNGLGETTEXLEVELPARAMETERFVPROC)gl_api._glGetTexLevelParameterfv_ptr)
#define glGetTexLevelParameteriv ((PFNGLGETTEXLEVELPARAMETERIVPROC)gl_api._glGetTexLevelParameteriv_ptr)
#define glIsEnabled ((PFNGLISENABLEDPROC)gl_api._glIsEnabled_ptr)
#define glDepthRange ((PFNGLDEPTHRANGEPROC)gl_api._glDepthRange_ptr)
#define glViewport ((PFNGLVIEWPORTPROC)gl_api._glViewport_ptr)
#define glDrawArrays ((PFNGLDRAWARRAYSPROC)gl_api._glDrawArrays_ptr)
#define glDrawElements ((PFNGLDRAWELEMENTSPROC)gl_api._glDrawElements_ptr)
#define glGetPointerv ((PFNGLGETPOINTERVPROC)gl_api._glGetPointerv_ptr)
#define glPolygonOffset ((PFNGLPOLYGONOFFSETPROC)gl_api._glPolygonOffset_ptr)
#define glCopyTexImage1D ((PFNGLCOPYTEXIMAGE1DPROC)gl_api._glCopyTexImage1D_ptr)
#define glCopyTexImage2D ((PFNGLCOPYTEXIMAGE2DPROC)gl_api._glCopyTexImage2D_ptr)
#define glCopyTexSubImage1D ((PFNGLCOPYTEXSUBIMAGE1DPROC)gl_api._glCopyTexSubImage1D_ptr)
#define glCopyTexSubImage2D ((PFNGLCOPYTEXSUBIMAGE2DPROC)gl_api._glCopyTexSubImage2D_ptr)
#define glTexSubImage1D ((PFNGLTEXSUBIMAGE1DPROC)gl_api._glTexSubImage1D_ptr)
#define glTexSubImage2D ((PFNGLTEXSUBIMAGE2DPROC)gl_api._glTexSubImage2D_ptr)
#define glBindTexture ((PFNGLBINDTEXTUREPROC)gl_api._glBindTexture_ptr)
#define glDeleteTextures ((PFNGLDELETETEXTURESPROC)gl_api._glDeleteTextures_ptr)
#define glGenTextures ((PFNGLGENTEXTURESPROC)gl_api._glGenTextures_ptr)
#define glIsTexture ((PFNGLISTEXTUREPROC)gl_api._glIsTexture_ptr)
#endif
#define glBlendColor ((PFNGLBLENDCOLORPROC)gl_api._glBlendColor_ptr)
#define glBlendEquation ((PFNGLBLENDEQUATIONPROC)gl_api._glBlendEquation_ptr)
#define glDrawRangeElements ((PFNGLDRAWRANGEELEMENTSPROC)gl_api._glDrawRangeElements_ptr)
#define glTexImage3D ((PFNGLTEXIMAGE3DPROC)gl_api._glTexImage3D_ptr)
#define glTexSubImage3D ((PFNGLTEXSUBIMAGE3DPROC)gl_api._glTexSubImage3D_ptr)
#define glCopyTexSubImage3D ((PFNGLCOPYTEXSUBIMAGE3DPROC)gl_api._glCopyTexSubImage3D_ptr)
#define glActiveTexture ((PFNGLACTIVETEXTUREPROC)gl_api._glActiveTexture_ptr)
#define glSampleCoverage ((PFNGLSAMPLECOVERAGEPROC)gl_api._glSampleCoverage_ptr)
#define glCompressedTexImage3D ((PFNGLCOMPRESSEDTEXIMAGE3DPROC)gl_api._glCompressedTexImage3D_ptr)
#define glCompressedTexImage2D ((PFNGLCOMPRESSEDTEXIMAGE2DPROC)gl_api._glCompressedTexImage2D_ptr)
#define glCompressedTexImage1D ((PFNGLCOMPRESSEDTEXIMAGE1DPROC)gl_api._glCompressedTexImage1D_ptr)
#define glCompressedTexSubImage3D ((PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)gl_api._glCompressedTexSubImage3D_ptr)
#define glCompressedTexSubImage2D ((PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)gl_api._glCompressedTexSubImage2D_ptr)
#define glCompressedTexSubImage1D ((PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC)gl_api._glCompressedTexSubImage1D_ptr)
#define glGetCompressedTexImage ((PFNGLGETCOMPRESSEDTEXIMAGEPROC)gl_api._glGetCompressedTexImage_ptr)
#endif
#define glBlendFuncSeparate ((PFNGLBLENDFUNCSEPARATEPROC)gl_api._glBlendFuncSeparate_ptr)
#define glMultiDrawArrays ((PFNGLMULTIDRAWARRAYSPROC)gl_api._glMultiDrawArrays_ptr)
#define glMultiDrawElements ((PFNGLMULTIDRAWELEMENTSPROC)gl_api._glMultiDrawElements_ptr)
#define glPointParameterf ((PFNGLPOINTPARAMETERFPROC)gl_api._glPointParameterf_ptr)
#define glPointParameterfv ((PFNGLPOINTPARAMETERFVPROC)gl_api._glPointParameterfv_ptr)
#define glPointParameteri ((PFNGLPOINTPARAMETERIPROC)gl_api._glPointParameteri_ptr)
#define glPointParameteriv ((PFNGLPOINTPARAMETERIVPROC)gl_api._glPointParameteriv_ptr)
#define glGenQueries ((PFNGLGENQUERIESPROC)gl_api._glGenQueries_ptr)
#define glDeleteQueries ((PFNGLDELETEQUERIESPROC)gl_api._glDeleteQueries_ptr)
#define glIsQuery ((PFNGLISQUERYPROC)gl_api._glIsQuery_ptr)
#define glBeginQuery ((PFNGLBEGINQUERYPROC)gl_api._glBeginQuery_ptr)
#define glEndQuery ((PFNGLENDQUERYPROC)gl_api._glEndQuery_ptr)
#define glGetQueryiv ((PFNGLGETQUERYIVPROC)gl_api._glGetQueryiv_ptr)
#define glGetQueryObjectiv ((PFNGLGETQUERYOBJECTIVPROC)gl_api._glGetQueryObjectiv_ptr)
#define glGetQueryObjectuiv ((PFNGLGETQUERYOBJECTUIVPROC)gl_api._glGetQueryObjectuiv_ptr)
#define glBindBuffer ((PFNGLBINDBUFFERPROC)gl_api._glBindBuffer_ptr)
#define glDeleteBuffers ((PFNGLDELETEBUFFERSPROC)gl_api._glDeleteBuffers_ptr)
#define glGenBuffers ((PFNGLGENBUFFERSPROC)gl_api._glGenBuffers_ptr)
#define glIsBuffer ((PFNGLISBUFFERPROC)gl_api._glIsBuffer_ptr)
#define glBufferData ((PFNGLBUFFERDATAPROC)gl_api._glBufferData_ptr)
#define glBufferSubData ((PFNGLBUFFERSUBDATAPROC)gl_api._glBufferSubData_ptr)
#define glGetBufferSubData ((PFNGLGETBUFFERSUBDATAPROC)gl_api._glGetBufferSubData_ptr)
#define glMapBuffer ((PFNGLMAPBUFFERPROC)gl_api._glMapBuffer_ptr)
#define glUnmapBuffer ((PFNGLUNMAPBUFFERPROC)gl_api._glUnmapBuffer_ptr)
#define glGetBufferParameteriv ((PFNGLGETBUFFERPARAMETERIVPROC)gl_api._glGetBufferParameteriv_ptr)
#define glGetBufferPointerv ((PFNGLGETBUFFERPOINTERVPROC)gl_api._glGetBufferPointerv_ptr)
#define glBlendEquationSeparate ((PFNGLBLENDEQUATIONSEPARATEPROC)gl_api._glBlendEquationSeparate_ptr)
#define glDrawBuffers ((PFNGLDRAWBUFFERSPROC)gl_api._glDrawBuffers_ptr)
#define glStencilOpSeparate ((PFNGLSTENCILOPSEPARATEPROC)gl_api._glStencilOpSeparate_ptr)
#define glStencilFuncSeparate ((PFNGLSTENCILFUNCSEPARATEPROC)gl_api._glStencilFuncSeparate_ptr)
#define glStencilMaskSeparate ((PFNGLSTENCILMASKSEPARATEPROC)gl_api._glStencilMaskSeparate_ptr)
#define glAttachShader ((PFNGLATTACHSHADERPROC)gl_api._glAttachShader_ptr)
#define glBindAttribLocation ((PFNGLBINDATTRIBLOCATIONPROC)gl_api._glBindAttribLocation_ptr)
#define glCompileShader ((PFNGLCOMPILESHADERPROC)gl_api._glCompileShader_ptr)
#define glCreateProgram ((PFNGLCREATEPROGRAMPROC)gl_api._glCreateProgram_ptr)
#define glCreateShader ((PFNGLCREATESHADERPROC)gl_api._glCreateShader_ptr)
#define glDeleteProgram ((PFNGLDELETEPROGRAMPROC)gl_api._glDeleteProgram_ptr)
#define glDeleteShader ((PFNGLDELETESHADERPROC)gl_api._glDeleteShader_ptr)
#define glDetachShader ((PFNGLDETACHSHADERPROC)gl_api._glDetachShader_ptr)
#define glDisableVertexAttribArray ((PFNGLDISABLEVERTEXATTRIBARRAYPROC)gl_api._glDisableVertexAttribArray_ptr)
#define glEnableVertexAttribArray ((PFNGLENABLEVERTEXATTRIBARRAYPROC)gl_api._glEnableVertexAttribArray_ptr)
#define glGetActiveAttrib ((PFNGLGETACTIVEATTRIBPROC)gl_api._glGetActiveAttrib_ptr)
#define glGetActiveUniform ((PFNGLGETACTIVEUNIFORMPROC)gl_api._glGetActiveUniform_ptr)
#define glGetAttachedShaders ((PFNGLGETATTACHEDSHADERSPROC)gl_api._glGetAttachedShaders_ptr)
#define glGetAttribLocation ((PFNGLGETATTRIBLOCATIONPROC)gl_api._glGetAttribLocation_ptr)
#define glGetProgramiv ((PFNGLGETPROGRAMIVPROC)gl_api._glGetProgramiv_ptr)
#define glGetProgramInfoLog ((PFNGLGETPROGRAMINFOLOGPROC)gl_api._glGetProgramInfoLog_ptr)
#define glGetShaderiv ((PFNGLGETSHADERIVPROC)gl_api._glGetShaderiv_ptr)
#define glGetShaderInfoLog ((PFNGLGETSHADERINFOLOGPROC)gl_api._glGetShaderInfoLog_ptr)
#define glGetShaderSource ((PFNGLGETSHADERSOURCEPROC)gl_api._glGetShaderSource_ptr)
#define glGetUniformLocation ((PFNGLGETUNIFORMLOCATIONPROC)gl_api._glGetUniformLocation_ptr)
#define glGetUniformfv ((PFNGLGETUNIFORMFVPROC)gl_api._glGetUniformfv_ptr)
#define glGetUniformiv ((PFNGLGETUNIFORMIVPROC)gl_api._glGetUniformiv_ptr)
#define glGetVertexAttribdv ((PFNGLGETVERTEXATTRIBDVPROC)gl_api._glGetVertexAttribdv_ptr)
#define glGetVertexAttribfv ((PFNGLGETVERTEXATTRIBFVPROC)gl_api._glGetVertexAttribfv_ptr)
#define glGetVertexAttribiv ((PFNGLGETVERTEXATTRIBIVPROC)gl_api._glGetVertexAttribiv_ptr)
#define glGetVertexAttribPointerv ((PFNGLGETVERTEXATTRIBPOINTERVPROC)gl_api._glGetVertexAttribPointerv_ptr)
#define glIsProgram ((PFNGLISPROGRAMPROC)gl_api._glIsProgram_ptr)
#define glIsShader ((PFNGLISSHADERPROC)gl_api._glIsShader_ptr)
#define glLinkProgram ((PFNGLLINKPROGRAMPROC)gl_api._glLinkProgram_ptr)
#define glShaderSource ((PFNGLSHADERSOURCEPROC)gl_api._glShaderSource_ptr)
#define glUseProgram ((PFNGLUSEPROGRAMPROC)gl_api._glUseProgram_ptr)
#define glUniform1f ((PFNGLUNIFORM1FPROC)gl_api._glUniform1f_ptr)
#define glUniform2f ((PFNGLUNIFORM2FPROC)gl_api._glUniform2f_ptr)
#define glUniform3f ((PFNGLUNIFORM3FPROC)gl_api._glUniform3f_ptr)
#define glUniform4f ((PFNGLUNIFORM4FPROC)gl_api._glUniform4f_ptr)
#define glUniform1i ((PFNGLUNIFORM1IPROC)gl_api._glUniform1i_ptr)
#define glUniform2i ((PFNGLUNIFORM2IPROC)gl_api._glUniform2i_ptr)
#define glUniform3i ((PFNGLUNIFORM3IPROC)gl_api._glUniform3i_ptr)
#define glUniform4i ((PFNGLUNIFORM4IPROC)gl_api._glUniform4i_ptr)
#define glUniform1fv ((PFNGLUNIFORM1FVPROC)gl_api._glUniform1fv_ptr)
#define glUniform2fv ((PFNGLUNIFORM2FVPROC)gl_api._glUniform2fv_ptr)
#define glUniform3fv ((PFNGLUNIFORM3FVPROC)gl_api._glUniform3fv_ptr)
#define glUniform4fv ((PFNGLUNIFORM4FVPROC)gl_api._glUniform4fv_ptr)
#define glUniform1iv ((PFNGLUNIFORM1IVPROC)gl_api._glUniform1iv_ptr)
#define glUniform2iv ((PFNGLUNIFORM2IVPROC)gl_api._glUniform2iv_ptr)
#define glUniform3iv ((PFNGLUNIFORM3IVPROC)gl_api._glUniform3iv_ptr)
#define glUniform4iv ((PFNGLUNIFORM4IVPROC)gl_api._glUniform4iv_ptr)
#define glUniformMatrix2fv ((PFNGLUNIFORMMATRIX2FVPROC)gl_api._glUniformMatrix2fv_ptr)
#define glUniformMatrix3fv ((PFNGLUNIFORMMATRIX3FVPROC)gl_api._glUniformMatrix3fv_ptr)
#define glUniformMatrix4fv ((PFNGLUNIFORMMATRIX4FVPROC)gl_api._glUniformMatrix4fv_ptr)
#define glValidateProgram ((PFNGLVALIDATEPROGRAMPROC)gl_api._glValidateProgram_ptr)
#define glVertexAttrib1d ((PFNGLVERTEXATTRIB1DPROC)gl_api._glVertexAttrib1d_ptr)
#define glVertexAttrib1dv ((PFNGLVERTEXATTRIB1DVPROC)gl_api._glVertexAttrib1dv_ptr)
#define glVertexAttrib1f ((PFNGLVERTEXATTRIB1FPROC)gl_api._glVertexAttrib1f_ptr)
#define glVertexAttrib1fv ((PFNGLVERTEXATTRIB1FVPROC)gl_api._glVertexAttrib1fv_ptr)
#define glVertexAttrib1s ((PFNGLVERTEXATTRIB1SPROC)gl_api._glVertexAttrib1s_ptr)
#define glVertexAttrib1sv ((PFNGLVERTEXATTRIB1SVPROC)gl_api._glVertexAttrib1sv_ptr)
#define glVertexAttrib2d ((PFNGLVERTEXATTRIB2DPROC)gl_api._glVertexAttrib2d_ptr)
#define glVertexAttrib2dv ((PFNGLVERTEXATTRIB2DVPROC)gl_api._glVertexAttrib2dv_ptr)
#define glVertexAttrib2f ((PFNGLVERTEXATTRIB2FPROC)gl_api._glVertexAttrib2f_ptr)
#define glVertexAttrib2fv ((PFNGLVERTEXATTRIB2FVPROC)gl_api._glVertexAttrib2fv_ptr)
#define glVertexAttrib2s ((PFNGLVERTEXATTRIB2SPROC)gl_api._glVertexAttrib2s_ptr)
#define glVertexAttrib2sv ((PFNGLVERTEXATTRIB2SVPROC)gl_api._glVertexAttrib2sv_ptr)
#define glVertexAttrib3d ((PFNGLVERTEXATTRIB3DPROC)gl_api._glVertexAttrib3d_ptr)
#define glVertexAttrib3dv ((PFNGLVERTEXATTRIB3DVPROC)gl_api._glVertexAttrib3dv_ptr)
#define glVertexAttrib3f ((PFNGLVERTEXATTRIB3FPROC)gl_api._glVertexAttrib3f_ptr)
#define glVertexAttrib3fv ((PFNGLVERTEXATTRIB3FVPROC)gl_api._glVertexAttrib3fv_ptr)
#define glVertexAttrib3s ((PFNGLVERTEXATTRIB3SPROC)gl_api._glVertexAttrib3s_ptr)
#define glVertexAttrib3sv ((PFNGLVERTEXATTRIB3SVPROC)gl_api._glVertexAttrib3sv_ptr)
#define glVertexAttrib4Nbv ((PFNGLVERTEXATTRIB4NBVPROC)gl_api._glVertexAttrib4Nbv_ptr)
#define glVertexAttrib4Niv ((PFNGLVERTEXATTRIB4NIVPROC)gl_api._glVertexAttrib4Niv_ptr)
#define glVertexAttrib4Nsv ((PFNGLVERTEXATTRIB4NSVPROC)gl_api._glVertexAttrib4Nsv_ptr)
#define glVertexAttrib4Nub ((PFNGLVERTEXATTRIB4NUBPROC)gl_api._glVertexAttrib4Nub_ptr)
#define glVertexAttrib4Nubv ((PFNGLVERTEXATTRIB4NUBVPROC)gl_api._glVertexAttrib4Nubv_ptr)
#define glVertexAttrib4Nuiv ((PFNGLVERTEXATTRIB4NUIVPROC)gl_api._glVertexAttrib4Nuiv_ptr)
#define glVertexAttrib4Nusv ((PFNGLVERTEXATTRIB4NUSVPROC)gl_api._glVertexAttrib4Nusv_ptr)
#define glVertexAttrib4bv ((PFNGLVERTEXATTRIB4BVPROC)gl_api._glVertexAttrib4bv_ptr)
#define glVertexAttrib4d ((PFNGLVERTEXATTRIB4DPROC)gl_api._glVertexAttrib4d_ptr)
#define glVertexAttrib4dv ((PFNGLVERTEXATTRIB4DVPROC)gl_api._glVertexAttrib4dv_ptr)
#define glVertexAttrib4f ((PFNGLVERTEXATTRIB4FPROC)gl_api._glVertexAttrib4f_ptr)
#define glVertexAttrib4fv ((PFNGLVERTEXATTRIB4FVPROC)gl_api._glVertexAttrib4fv_ptr)
#define glVertexAttrib4iv ((PFNGLVERTEXATTRIB4IVPROC)gl_api._glVertexAttrib4iv_ptr)
#define glVertexAttrib4s ((PFNGLVERTEXATTRIB4SPROC)gl_api._glVertexAttrib4s_ptr)
#define glVertexAttrib4sv ((PFNGLVERTEXATTRIB4SVPROC)gl_api._glVertexAttrib4sv_ptr)
#define glVertexAttrib4ubv ((PFNGLVERTEXATTRIB4UBVPROC)gl_api._glVertexAttrib4ubv_ptr)
#define glVertexAttrib4uiv ((PFNGLVERTEXATTRIB4UIVPROC)gl_api._glVertexAttrib4uiv_ptr)
#define glVertexAttrib4usv ((PFNGLVERTEXATTRIB4USVPROC)gl_api._glVertexAttrib4usv_ptr)
#define glVertexAttribPointer ((PFNGLVERTEXATTRIBPOINTERPROC)gl_api._glVertexAttribPointer_ptr)
#define glUniformMatrix2x3fv ((PFNGLUNIFORMMATRIX2X3FVPROC)gl_api._glUniformMatrix2x3fv_ptr)
#define glUniformMatrix3x2fv ((PFNGLUNIFORMMATRIX3X2FVPROC)gl_api._glUniformMatrix3x2fv_ptr)
#define glUniformMatrix2x4fv ((PFNGLUNIFORMMATRIX2X4FVPROC)gl_api._glUniformMatrix2x4fv_ptr)
#define glUniformMatrix4x2fv ((PFNGLUNIFORMMATRIX4X2FVPROC)gl_api._glUniformMatrix4x2fv_ptr)
#define glUniformMatrix3x4fv ((PFNGLUNIFORMMATRIX3X4FVPROC)gl_api._glUniformMatrix3x4fv_ptr)
#define glUniformMatrix4x3fv ((PFNGLUNIFORMMATRIX4X3FVPROC)gl_api._glUniformMatrix4x3fv_ptr)
#define glColorMaski ((PFNGLCOLORMASKIPROC)gl_api._glColorMaski_ptr)
#define glGetBooleani_v ((PFNGLGETBOOLEANI_VPROC)gl_api._glGetBooleani_v_ptr)
#define glGetIntegeri_v ((PFNGLGETINTEGERI_VPROC)gl_api._glGetIntegeri_v_ptr)
#define glEnablei ((PFNGLENABLEIPROC)gl_api._glEnablei_ptr)
#define glDisablei ((PFNGLDISABLEIPROC)gl_api._glDisablei_ptr)
#define glIsEnabledi ((PFNGLISENABLEDIPROC)gl_api._glIsEnabledi_ptr)
#define glBeginTransformFeedback ((PFNGLBEGINTRANSFORMFEEDBACKPROC)gl_api._glBeginTransformFeedback_ptr)
#define glEndTransformFeedback ((PFNGLENDTRANSFORMFEEDBACKPROC)gl_api._glEndTransformFeedback_ptr)
#define glBindBufferRange ((PFNGLBINDBUFFERRANGEPROC)gl_api._glBindBufferRange_ptr)
#define glBindBufferBase ((PFNGLBINDBUFFERBASEPROC)gl_api._glBindBufferBase_ptr)
#define glTransformFeedbackVaryings ((PFNGLTRANSFORMFEEDBACKVARYINGSPROC)gl_api._glTransformFeedbackVaryings_ptr)
#define glGetTransformFeedbackVarying ((PFNGLGETTRANSFORMFEEDBACKVARYINGPROC)gl_api._glGetTransformFeedbackVarying_ptr)
#define glClampColor ((PFNGLCLAMPCOLORPROC)gl_api._glClampColor_ptr)
#define glBeginConditionalRender ((PFNGLBEGINCONDITIONALRENDERPROC)gl_api._glBeginConditionalRender_ptr)
#define glEndConditionalRender ((PFNGLENDCONDITIONALRENDERPROC)gl_api._glEndConditionalRender_ptr)
#define glVertexAttribIPointer ((PFNGLVERTEXATTRIBIPOINTERPROC)gl_api._glVertexAttribIPointer_ptr)
#define glGetVertexAttribIiv ((PFNGLGETVERTEXATTRIBIIVPROC)gl_api._glGetVertexAttribIiv_ptr)
#define glGetVertexAttribIuiv ((PFNGLGETVERTEXATTRIBIUIVPROC)gl_api._glGetVertexAttribIuiv_ptr)
#define glVertexAttribI1i ((PFNGLVERTEXATTRIBI1IPROC)gl_api._glVertexAttribI1i_ptr)
#define glVertexAttribI2i ((PFNGLVERTEXATTRIBI2IPROC)gl_api._glVertexAttribI2i_ptr)
#define glVertexAttribI3i ((PFNGLVERTEXATTRIBI3IPROC)gl_api._glVertexAttribI3i_ptr)
#define glVertexAttribI4i ((PFNGLVERTEXATTRIBI4IPROC)gl_api._glVertexAttribI4i_ptr)
#define glVertexAttribI1ui ((PFNGLVERTEXATTRIBI1UIPROC)gl_api._glVertexAttribI1ui_ptr)
#define glVertexAttribI2ui ((PFNGLVERTEXATTRIBI2UIPROC)gl_api._glVertexAttribI2ui_ptr)
#define glVertexAttribI3ui ((PFNGLVERTEXATTRIBI3UIPROC)gl_api._glVertexAttribI3ui_ptr)
#define glVertexAttribI4ui ((PFNGLVERTEXATTRIBI4UIPROC)gl_api._glVertexAttribI4ui_ptr)
#define glVertexAttribI1iv ((PFNGLVERTEXATTRIBI1IVPROC)gl_api._glVertexAttribI1iv_ptr)
#define glVertexAttribI2iv ((PFNGLVERTEXATTRIBI2IVPROC)gl_api._glVertexAttribI2iv_ptr)
#define glVertexAttribI3iv ((PFNGLVERTEXATTRIBI3IVPROC)gl_api._glVertexAttribI3iv_ptr)
#define glVertexAttribI4iv ((PFNGLVERTEXATTRIBI4IVPROC)gl_api._glVertexAttribI4iv_ptr)
#define glVertexAttribI1uiv ((PFNGLVERTEXATTRIBI1UIVPROC)gl_api._glVertexAttribI1uiv_ptr)
#define glVertexAttribI2uiv ((PFNGLVERTEXATTRIBI2UIVPROC)gl_api._glVertexAttribI2uiv_ptr)
#define glVertexAttribI3uiv ((PFNGLVERTEXATTRIBI3UIVPROC)gl_api._glVertexAttribI3uiv_ptr)
#define glVertexAttribI4uiv ((PFNGLVERTEXATTRIBI4UIVPROC)gl_api._glVertexAttribI4uiv_ptr)
#define glVertexAttribI4bv ((PFNGLVERTEXATTRIBI4BVPROC)gl_api._glVertexAttribI4bv_ptr)
#define glVertexAttribI4sv ((PFNGLVERTEXATTRIBI4SVPROC)gl_api._glVertexAttribI4sv_ptr)
#define glVertexAttribI4ubv ((PFNGLVERTEXATTRIBI4UBVPROC)gl_api._glVertexAttribI4ubv_ptr)
#define glVertexAttribI4usv ((PFNGLVERTEXATTRIBI4USVPROC)gl_api._glVertexAttribI4usv_ptr)
#define glGetUniformuiv ((PFNGLGETUNIFORMUIVPROC)gl_api._glGetUniformuiv_ptr)
#define glBindFragDataLocation ((PFNGLBINDFRAGDATALOCATIONPROC)gl_api._glBindFragDataLocation_ptr)
#define glGetFragDataLocation ((PFNGLGETFRAGDATALOCATIONPROC)gl_api._glGetFragDataLocation_ptr)
#define glUniform1ui ((PFNGLUNIFORM1UIPROC)gl_api._glUniform1ui_ptr)
#define glUniform2ui ((PFNGLUNIFORM2UIPROC)gl_api._glUniform2ui_ptr)
#define glUniform3ui ((PFNGLUNIFORM3UIPROC)gl_api._glUniform3ui_ptr)
#define glUniform4ui ((PFNGLUNIFORM4UIPROC)gl_api._glUniform4ui_ptr)
#define glUniform1uiv ((PFNGLUNIFORM1UIVPROC)gl_api._glUniform1uiv_ptr)
#define glUniform2uiv ((PFNGLUNIFORM2UIVPROC)gl_api._glUniform2uiv_ptr)
#define glUniform3uiv ((PFNGLUNIFORM3UIVPROC)gl_api._glUniform3uiv_ptr)
#define glUniform4uiv ((PFNGLUNIFORM4UIVPROC)gl_api._glUniform4uiv_ptr)
#define glTexParameterIiv ((PFNGLTEXPARAMETERIIVPROC)gl_api._glTexParameterIiv_ptr)
#define glTexParameterIuiv ((PFNGLTEXPARAMETERIUIVPROC)gl_api._glTexParameterIuiv_ptr)
#define glGetTexParameterIiv ((PFNGLGETTEXPARAMETERIIVPROC)gl_api._glGetTexParameterIiv_ptr)
#define glGetTexParameterIuiv ((PFNGLGETTEXPARAMETERIUIVPROC)gl_api._glGetTexParameterIuiv_ptr)
#define glClearBufferiv ((PFNGLCLEARBUFFERIVPROC)gl_api._glClearBufferiv_ptr)
#define glClearBufferuiv ((PFNGLCLEARBUFFERUIVPROC)gl_api._glClearBufferuiv_ptr)
#define glClearBufferfv ((PFNGLCLEARBUFFERFVPROC)gl_api._glClearBufferfv_ptr)
#define glClearBufferfi ((PFNGLCLEARBUFFERFIPROC)gl_api._glClearBufferfi_ptr)
#define glGetStringi ((PFNGLGETSTRINGIPROC)gl_api._glGetStringi_ptr)
#define glIsRenderbuffer ((PFNGLISRENDERBUFFERPROC)gl_api._glIsRenderbuffer_ptr)
#define glBindRenderbuffer ((PFNGLBINDRENDERBUFFERPROC)gl_api._glBindRenderbuffer_ptr)
#define glDeleteRenderbuffers ((PFNGLDELETERENDERBUFFERSPROC)gl_api._glDeleteRenderbuffers_ptr)
#define glGenRenderbuffers ((PFNGLGENRENDERBUFFERSPROC)gl_api._glGenRenderbuffers_ptr)
#define glRenderbufferStorage ((PFNGLRENDERBUFFERSTORAGEPROC)gl_api._glRenderbufferStorage_ptr)
#define glGetRenderbufferParameteriv ((PFNGLGETRENDERBUFFERPARAMETERIVPROC)gl_api._glGetRenderbufferParameteriv_ptr)
#define glIsFramebuffer ((PFNGLISFRAMEBUFFERPROC)gl_api._glIsFramebuffer_ptr)
#define glBindFramebuffer ((PFNGLBINDFRAMEBUFFERPROC)gl_api._glBindFramebuffer_ptr)
#define glDeleteFramebuffers ((PFNGLDELETEFRAMEBUFFERSPROC)gl_api._glDeleteFramebuffers_ptr)
#define glGenFramebuffers ((PFNGLGENFRAMEBUFFERSPROC)gl_api._glGenFramebuffers_ptr)
#define glCheckFramebufferStatus ((PFNGLCHECKFRAMEBUFFERSTATUSPROC)gl_api._glCheckFramebufferStatus_ptr)
#define glFramebufferTexture1D ((PFNGLFRAMEBUFFERTEXTURE1DPROC)gl_api._glFramebufferTexture1D_ptr)
#define glFramebufferTexture2D ((PFNGLFRAMEBUFFERTEXTURE2DPROC)gl_api._glFramebufferTexture2D_ptr)
#define glFramebufferTexture3D ((PFNGLFRAMEBUFFERTEXTURE3DPROC)gl_api._glFramebufferTexture3D_ptr)
#define glFramebufferRenderbuffer ((PFNGLFRAMEBUFFERRENDERBUFFERPROC)gl_api._glFramebufferRenderbuffer_ptr)
#define glGetFramebufferAttachmentParameteriv ((PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)gl_api._glGetFramebufferAttachmentParameteriv_ptr)
#define glGenerateMipmap ((PFNGLGENERATEMIPMAPPROC)gl_api._glGenerateMipmap_ptr)
#define glBlitFramebuffer ((PFNGLBLITFRAMEBUFFERPROC)gl_api._glBlitFramebuffer_ptr)
#define glRenderbufferStorageMultisample ((PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)gl_api._glRenderbufferStorageMultisample_ptr)
#define glFramebufferTextureLayer ((PFNGLFRAMEBUFFERTEXTURELAYERPROC)gl_api._glFramebufferTextureLayer_ptr)
#define glMapBufferRange ((PFNGLMAPBUFFERRANGEPROC)gl_api._glMapBufferRange_ptr)
#define glFlushMappedBufferRange ((PFNGLFLUSHMAPPEDBUFFERRANGEPROC)gl_api._glFlushMappedBufferRange_ptr)
#define glBindVertexArray ((PFNGLBINDVERTEXARRAYPROC)gl_api._glBindVertexArray_ptr)
#define glDeleteVertexArrays ((PFNGLDELETEVERTEXARRAYSPROC)gl_api._glDeleteVertexArrays_ptr)
#define glGenVertexArrays ((PFNGLGENVERTEXARRAYSPROC)gl_api._glGenVertexArrays_ptr)
#define glIsVertexArray ((PFNGLISVERTEXARRAYPROC)gl_api._glIsVertexArray_ptr)
#define glDrawArraysInstanced ((PFNGLDRAWARRAYSINSTANCEDPROC)gl_api._glDrawArraysInstanced_ptr)
#define glDrawElementsInstanced ((PFNGLDRAWELEMENTSINSTANCEDPROC)gl_api._glDrawElementsInstanced_ptr)
#define glTexBuffer ((PFNGLTEXBUFFERPROC)gl_api._glTexBuffer_ptr)
#define glPrimitiveRestartIndex ((PFNGLPRIMITIVERESTARTINDEXPROC)gl_api._glPrimitiveRestartIndex_ptr)
#define glCopyBufferSubData ((PFNGLCOPYBUFFERSUBDATAPROC)gl_api._glCopyBufferSubData_ptr)
#define glGetUniformIndices ((PFNGLGETUNIFORMINDICESPROC)gl_api._glGetUniformIndices_ptr)
#define glGetActiveUniformsiv ((PFNGLGETACTIVEUNIFORMSIVPROC)gl_api._glGetActiveUniformsiv_ptr)
#define glGetActiveUniformName ((PFNGLGETACTIVEUNIFORMNAMEPROC)gl_api._glGetActiveUniformName_ptr)
#define glGetUniformBlockIndex ((PFNGLGETUNIFORMBLOCKINDEXPROC)gl_api._glGetUniformBlockIndex_ptr)
#define glGetActiveUniformBlockiv ((PFNGLGETACTIVEUNIFORMBLOCKIVPROC)gl_api._glGetActiveUniformBlockiv_ptr)
#define glGetActiveUniformBlockName ((PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC)gl_api._glGetActiveUniformBlockName_ptr)
#define glUniformBlockBinding ((PFNGLUNIFORMBLOCKBINDINGPROC)gl_api._glUniformBlockBinding_ptr)
#define glGetInteger64i_v ((PFNGLGETINTEGER64I_VPROC)gl_api._glGetInteger64i_v_ptr)
#define glGetBufferParameteri64v ((PFNGLGETBUFFERPARAMETERI64VPROC)gl_api._glGetBufferParameteri64v_ptr)
#define glFramebufferTexture ((PFNGLFRAMEBUFFERTEXTUREPROC)gl_api._glFramebufferTexture_ptr)
#define glDrawElementsBaseVertex ((PFNGLDRAWELEMENTSBASEVERTEXPROC)gl_api._glDrawElementsBaseVertex_ptr)
#define glDrawRangeElementsBaseVertex ((PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC)gl_api._glDrawRangeElementsBaseVertex_ptr)
#define glDrawElementsInstancedBaseVertex ((PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC)gl_api._glDrawElementsInstancedBaseVertex_ptr)
#define glMultiDrawElementsBaseVertex ((PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC)gl_api._glMultiDrawElementsBaseVertex_ptr)
#define glProvokingVertex ((PFNGLPROVOKINGVERTEXPROC)gl_api._glProvokingVertex_ptr)
#define glFenceSync ((PFNGLFENCESYNCPROC)gl_api._glFenceSync_ptr)
#define glIsSync ((PFNGLISSYNCPROC)gl_api._glIsSync_ptr)
#define glDeleteSync ((PFNGLDELETESYNCPROC)gl_api._glDeleteSync_ptr)
#define glClientWaitSync ((PFNGLCLIENTWAITSYNCPROC)gl_api._glClientWaitSync_ptr)
#define glWaitSync ((PFNGLWAITSYNCPROC)gl_api._glWaitSync_ptr)
#define glGetInteger64v ((PFNGLGETINTEGER64VPROC)gl_api._glGetInteger64v_ptr)
#define glGetSynciv ((PFNGLGETSYNCIVPROC)gl_api._glGetSynciv_ptr)
#define glTexImage2DMultisample ((PFNGLTEXIMAGE2DMULTISAMPLEPROC)gl_api._glTexImage2DMultisample_ptr)
#define glTexImage3DMultisample ((PFNGLTEXIMAGE3DMULTISAMPLEPROC)gl_api._glTexImage3DMultisample_ptr)
#define glGetMultisamplefv ((PFNGLGETMULTISAMPLEFVPROC)gl_api._glGetMultisamplefv_ptr)
#define glSampleMaski ((PFNGLSAMPLEMASKIPROC)gl_api._glSampleMaski_ptr)
#define glVertexAttribDivisor ((PFNGLVERTEXATTRIBDIVISORPROC)gl_api._glVertexAttribDivisor_ptr)
#define glBindFragDataLocationIndexed ((PFNGLBINDFRAGDATALOCATIONINDEXEDPROC)gl_api._glBindFragDataLocationIndexed_ptr)
#define glGetFragDataIndex ((PFNGLGETFRAGDATAINDEXPROC)gl_api._glGetFragDataIndex_ptr)
#define glGenSamplers ((PFNGLGENSAMPLERSPROC)gl_api._glGenSamplers_ptr)
#define glDeleteSamplers ((PFNGLDELETESAMPLERSPROC)gl_api._glDeleteSamplers_ptr)
#define glIsSampler ((PFNGLISSAMPLERPROC)gl_api._glIsSampler_ptr)
#define glBindSampler ((PFNGLBINDSAMPLERPROC)gl_api._glBindSampler_ptr)
#define glSamplerParameteri ((PFNGLSAMPLERPARAMETERIPROC)gl_api._glSamplerParameteri_ptr)
#define glSamplerParameteriv ((PFNGLSAMPLERPARAMETERIVPROC)gl_api._glSamplerParameteriv_ptr)
#define glSamplerParameterf ((PFNGLSAMPLERPARAMETERFPROC)gl_api._glSamplerParameterf_ptr)
#define glSamplerParameterfv ((PFNGLSAMPLERPARAMETERFVPROC)gl_api._glSamplerParameterfv_ptr)
#define glSamplerParameterIiv ((PFNGLSAMPLERPARAMETERIIVPROC)gl_api._glSamplerParameterIiv_ptr)
#define glSamplerParameterIuiv ((PFNGLSAMPLERPARAMETERIUIVPROC)gl_api._glSamplerParameterIuiv_ptr)
#define glGetSamplerParameteriv ((PFNGLGETSAMPLERPARAMETERIVPROC)gl_api._glGetSamplerParameteriv_ptr)
#define glGetSamplerParameterIiv ((PFNGLGETSAMPLERPARAMETERIIVPROC)gl_api._glGetSamplerParameterIiv_ptr)
#define glGetSamplerParameterfv ((PFNGLGETSAMPLERPARAMETERFVPROC)gl_api._glGetSamplerParameterfv_ptr)
#define glGetSamplerParameterIuiv ((PFNGLGETSAMPLERPARAMETERIUIVPROC)gl_api._glGetSamplerParameterIuiv_ptr)
#define glQueryCounter ((PFNGLQUERYCOUNTERPROC)gl_api._glQueryCounter_ptr)
#define glGetQueryObjecti64v ((PFNGLGETQUERYOBJECTI64VPROC)gl_api._glGetQueryObjecti64v_ptr)
#define glGetQueryObjectui64v ((PFNGLGETQUERYOBJECTUI64VPROC)gl_api._glGetQueryObjectui64v_ptr)
#define glVertexAttribP1ui ((PFNGLVERTEXATTRIBP1UIPROC)gl_api._glVertexAttribP1ui_ptr)
#define glVertexAttribP1uiv ((PFNGLVERTEXATTRIBP1UIVPROC)gl_api._glVertexAttribP1uiv_ptr)
#define glVertexAttribP2ui ((PFNGLVERTEXATTRIBP2UIPROC)gl_api._glVertexAttribP2ui_ptr)
#define glVertexAttribP2uiv ((PFNGLVERTEXATTRIBP2UIVPROC)gl_api._glVertexAttribP2uiv_ptr)
#define glVertexAttribP3ui ((PFNGLVERTEXATTRIBP3UIPROC)gl_api._glVertexAttribP3ui_ptr)
#define glVertexAttribP3uiv ((PFNGLVERTEXATTRIBP3UIVPROC)gl_api._glVertexAttribP3uiv_ptr)
#define glVertexAttribP4ui ((PFNGLVERTEXATTRIBP4UIPROC)gl_api._glVertexAttribP4ui_ptr)
#define glVertexAttribP4uiv ((PFNGLVERTEXATTRIBP4UIVPROC)gl_api._glVertexAttribP4uiv_ptr)
#define glMinSampleShading ((PFNGLMINSAMPLESHADINGPROC)gl_api._glMinSampleShading_ptr)
#define glBlendEquationi ((PFNGLBLENDEQUATIONIPROC)gl_api._glBlendEquationi_ptr)
#define glBlendEquationSeparatei ((PFNGLBLENDEQUATIONSEPARATEIPROC)gl_api._glBlendEquationSeparatei_ptr)
#define glBlendFunci ((PFNGLBLENDFUNCIPROC)gl_api._glBlendFunci_ptr)
#define glBlendFuncSeparatei ((PFNGLBLENDFUNCSEPARATEIPROC)gl_api._glBlendFuncSeparatei_ptr)
#define glDrawArraysIndirect ((PFNGLDRAWARRAYSINDIRECTPROC)gl_api._glDrawArraysIndirect_ptr)
#define glDrawElementsIndirect ((PFNGLDRAWELEMENTSINDIRECTPROC)gl_api._glDrawElementsIndirect_ptr)
#define glUniform1d ((PFNGLUNIFORM1DPROC)gl_api._glUniform1d_ptr)
#define glUniform2d ((PFNGLUNIFORM2DPROC)gl_api._glUniform2d_ptr)
#define glUniform3d ((PFNGLUNIFORM3DPROC)gl_api._glUniform3d_ptr)
#define glUniform4d ((PFNGLUNIFORM4DPROC)gl_api._glUniform4d_ptr)
#define glUniform1dv ((PFNGLUNIFORM1DVPROC)gl_api._glUniform1dv_ptr)
#define glUniform2dv ((PFNGLUNIFORM2DVPROC)gl_api._glUniform2dv_ptr)
#define glUniform3dv ((PFNGLUNIFORM3DVPROC)gl_api._glUniform3dv_ptr)
#define glUniform4dv ((PFNGLUNIFORM4DVPROC)gl_api._glUniform4dv_ptr)
#define glUniformMatrix2dv ((PFNGLUNIFORMMATRIX2DVPROC)gl_api._glUniformMatrix2dv_ptr)
#define glUniformMatrix3dv ((PFNGLUNIFORMMATRIX3DVPROC)gl_api._glUniformMatrix3dv_ptr)
#define glUniformMatrix4dv ((PFNGLUNIFORMMATRIX4DVPROC)gl_api._glUniformMatrix4dv_ptr)
#define glUniformMatrix2x3dv ((PFNGLUNIFORMMATRIX2X3DVPROC)gl_api._glUniformMatrix2x3dv_ptr)
#define glUniformMatrix2x4dv ((PFNGLUNIFORMMATRIX2X4DVPROC)gl_api._glUniformMatrix2x4dv_ptr)
#define glUniformMatrix3x2dv ((PFNGLUNIFORMMATRIX3X2DVPROC)gl_api._glUniformMatrix3x2dv_ptr)
#define glUniformMatrix3x4dv ((PFNGLUNIFORMMATRIX3X4DVPROC)gl_api._glUniformMatrix3x4dv_ptr)
#define glUniformMatrix4x2dv ((PFNGLUNIFORMMATRIX4X2DVPROC)gl_api._glUniformMatrix4x2dv_ptr)
#define glUniformMatrix4x3dv ((PFNGLUNIFORMMATRIX4X3DVPROC)gl_api._glUniformMatrix4x3dv_ptr)
#define glGetUniformdv ((PFNGLGETUNIFORMDVPROC)gl_api._glGetUniformdv_ptr)
#define glGetSubroutineUniformLocation ((PFNGLGETSUBROUTINEUNIFORMLOCATIONPROC)gl_api._glGetSubroutineUniformLocation_ptr)
#define glGetSubroutineIndex ((PFNGLGETSUBROUTINEINDEXPROC)gl_api._glGetSubroutineIndex_ptr)
#define glGetActiveSubroutineUniformiv ((PFNGLGETACTIVESUBROUTINEUNIFORMIVPROC)gl_api._glGetActiveSubroutineUniformiv_ptr)
#define glGetActiveSubroutineUniformName ((PFNGLGETACTIVESUBROUTINEUNIFORMNAMEPROC)gl_api._glGetActiveSubroutineUniformName_ptr)
#define glGetActiveSubroutineName ((PFNGLGETACTIVESUBROUTINENAMEPROC)gl_api._glGetActiveSubroutineName_ptr)
#define glUniformSubroutinesuiv ((PFNGLUNIFORMSUBROUTINESUIVPROC)gl_api._glUniformSubroutinesuiv_ptr)
#define glGetUniformSubroutineuiv ((PFNGLGETUNIFORMSUBROUTINEUIVPROC)gl_api._glGetUniformSubroutineuiv_ptr)
#define glGetProgramStageiv ((PFNGLGETPROGRAMSTAGEIVPROC)gl_api._glGetProgramStageiv_ptr)
#define glPatchParameteri ((PFNGLPATCHPARAMETERIPROC)gl_api._glPatchParameteri_ptr)
#define glPatchParameterfv ((PFNGLPATCHPARAMETERFVPROC)gl_api._glPatchParameterfv_ptr)
#define glBindTransformFeedback ((PFNGLBINDTRANSFORMFEEDBACKPROC)gl_api._glBindTransformFeedback_ptr)
#define glDeleteTransformFeedbacks ((PFNGLDELETETRANSFORMFEEDBACKSPROC)gl_api._glDeleteTransformFeedbacks_ptr)
#define glGenTransformFeedbacks ((PFNGLGENTRANSFORMFEEDBACKSPROC)gl_api._glGenTransformFeedbacks_ptr)
#define glIsTransformFeedback ((PFNGLISTRANSFORMFEEDBACKPROC)gl_api._glIsTransformFeedback_ptr)
#define glPauseTransformFeedback ((PFNGLPAUSETRANSFORMFEEDBACKPROC)gl_api._glPauseTransformFeedback_ptr)
#define glResumeTransformFeedback ((PFNGLRESUMETRANSFORMFEEDBACKPROC)gl_api._glResumeTransformFeedback_ptr)
#define glDrawTransformFeedback ((PFNGLDRAWTRANSFORMFEEDBACKPROC)gl_api._glDrawTransformFeedback_ptr)
#define glDrawTransformFeedbackStream ((PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC)gl_api._glDrawTransformFeedbackStream_ptr)
#define glBeginQueryIndexed ((PFNGLBEGINQUERYINDEXEDPROC)gl_api._glBeginQueryIndexed_ptr)
#define glEndQueryIndexed ((PFNGLENDQUERYINDEXEDPROC)gl_api._glEndQueryIndexed_ptr)
#define glGetQueryIndexediv ((PFNGLGETQUERYINDEXEDIVPROC)gl_api._glGetQueryIndexediv_ptr)
#define glReleaseShaderCompiler ((PFNGLRELEASESHADERCOMPILERPROC)gl_api._glReleaseShaderCompiler_ptr)
#define glShaderBinary ((PFNGLSHADERBINARYPROC)gl_api._glShaderBinary_ptr)
#define glGetShaderPrecisionFormat ((PFNGLGETSHADERPRECISIONFORMATPROC)gl_api._glGetShaderPrecisionFormat_ptr)
#define glDepthRangef ((PFNGLDEPTHRANGEFPROC)gl_api._glDepthRangef_ptr)
#define glClearDepthf ((PFNGLCLEARDEPTHFPROC)gl_api._glClearDepthf_ptr)
#define glGetProgramBinary ((PFNGLGETPROGRAMBINARYPROC)gl_api._glGetProgramBinary_ptr)
#define glProgramBinary ((PFNGLPROGRAMBINARYPROC)gl_api._glProgramBinary_ptr)
#define glProgramParameteri ((PFNGLPROGRAMPARAMETERIPROC)gl_api._glProgramParameteri_ptr)
#define glUseProgramStages ((PFNGLUSEPROGRAMSTAGESPROC)gl_api._glUseProgramStages_ptr)
#define glActiveShaderProgram ((PFNGLACTIVESHADERPROGRAMPROC)gl_api._glActiveShaderProgram_ptr)
#define glCreateShaderProgramv ((PFNGLCREATESHADERPROGRAMVPROC)gl_api._glCreateShaderProgramv_ptr)
#define glBindProgramPipeline ((PFNGLBINDPROGRAMPIPELINEPROC)gl_api._glBindProgramPipeline_ptr)
#define glDeleteProgramPipelines ((PFNGLDELETEPROGRAMPIPELINESPROC)gl_api._glDeleteProgramPipelines_ptr)
#define glGenProgramPipelines ((PFNGLGENPROGRAMPIPELINESPROC)gl_api._glGenProgramPipelines_ptr)
#define glIsProgramPipeline ((PFNGLISPROGRAMPIPELINEPROC)gl_api._glIsProgramPipeline_ptr)
#define glGetProgramPipelineiv ((PFNGLGETPROGRAMPIPELINEIVPROC)gl_api._glGetProgramPipelineiv_ptr)
#define glProgramUniform1i ((PFNGLPROGRAMUNIFORM1IPROC)gl_api._glProgramUniform1i_ptr)
#define glProgramUniform1iv ((PFNGLPROGRAMUNIFORM1IVPROC)gl_api._glProgramUniform1iv_ptr)
#define glProgramUniform1f ((PFNGLPROGRAMUNIFORM1FPROC)gl_api._glProgramUniform1f_ptr)
#define glProgramUniform1fv ((PFNGLPROGRAMUNIFORM1FVPROC)gl_api._glProgramUniform1fv_ptr)
#define glProgramUniform1d ((PFNGLPROGRAMUNIFORM1DPROC)gl_api._glProgramUniform1d_ptr)
#define glProgramUniform1dv ((PFNGLPROGRAMUNIFORM1DVPROC)gl_api._glProgramUniform1dv_ptr)
#define glProgramUniform1ui ((PFNGLPROGRAMUNIFORM1UIPROC)gl_api._glProgramUniform1ui_ptr)
#define glProgramUniform1uiv ((PFNGLPROGRAMUNIFORM1UIVPROC)gl_api._glProgramUniform1uiv_ptr)
#define glProgramUniform2i ((PFNGLPROGRAMUNIFORM2IPROC)gl_api._glProgramUniform2i_ptr)
#define glProgramUniform2iv ((PFNGLPROGRAMUNIFORM2IVPROC)gl_api._glProgramUniform2iv_ptr)
#define glProgramUniform2f ((PFNGLPROGRAMUNIFORM2FPROC)gl_api._glProgramUniform2f_ptr)
#define glProgramUniform2fv ((PFNGLPROGRAMUNIFORM2FVPROC)gl_api._glProgramUniform2fv_ptr)
#define glProgramUniform2d ((PFNGLPROGRAMUNIFORM2DPROC)gl_api._glProgramUniform2d_ptr)
#define glProgramUniform2dv ((PFNGLPROGRAMUNIFORM2DVPROC)gl_api._glProgramUniform2dv_ptr)
#define glProgramUniform2ui ((PFNGLPROGRAMUNIFORM2UIPROC)gl_api._glProgramUniform2ui_ptr)
#define glProgramUniform2uiv ((PFNGLPROGRAMUNIFORM2UIVPROC)gl_api._glProgramUniform2uiv_ptr)
#define glProgramUniform3i ((PFNGLPROGRAMUNIFORM3IPROC)gl_api._glProgramUniform3i_ptr)
#define glProgramUniform3iv ((PFNGLPROGRAMUNIFORM3IVPROC)gl_api._glProgramUniform3iv_ptr)
#define glProgramUniform3f ((PFNGLPROGRAMUNIFORM3FPROC)gl_api._glProgramUniform3f_ptr)
#define glProgramUniform3fv ((PFNGLPROGRAMUNIFORM3FVPROC)gl_api._glProgramUniform3fv_ptr)
#define glProgramUniform3d ((PFNGLPROGRAMUNIFORM3DPROC)gl_api._glProgramUniform3d_ptr)
#define glProgramUniform3dv ((PFNGLPROGRAMUNIFORM3DVPROC)gl_api._glProgramUniform3dv_ptr)
#define glProgramUniform3ui ((PFNGLPROGRAMUNIFORM3UIPROC)gl_api._glProgramUniform3ui_ptr)
#define glProgramUniform3uiv ((PFNGLPROGRAMUNIFORM3UIVPROC)gl_api._glProgramUniform3uiv_ptr)
#define glProgramUniform4i ((PFNGLPROGRAMUNIFORM4IPROC)gl_api._glProgramUniform4i_ptr)
#define glProgramUniform4iv ((PFNGLPROGRAMUNIFORM4IVPROC)gl_api._glProgramUniform4iv_ptr)
#define glProgramUniform4f ((PFNGLPROGRAMUNIFORM4FPROC)gl_api._glProgramUniform4f_ptr)
#define glProgramUniform4fv ((PFNGLPROGRAMUNIFORM4FVPROC)gl_api._glProgramUniform4fv_ptr)
#define glProgramUniform4d ((PFNGLPROGRAMUNIFORM4DPROC)gl_api._glProgramUniform4d_ptr)
#define glProgramUniform4dv ((PFNGLPROGRAMUNIFORM4DVPROC)gl_api._glProgramUniform4dv_ptr)
#define glProgramUniform4ui ((PFNGLPROGRAMUNIFORM4UIPROC)gl_api._glProgramUniform4ui_ptr)
#define glProgramUniform4uiv ((PFNGLPROGRAMUNIFORM4UIVPROC)gl_api._glProgramUniform4uiv_ptr)
#define glProgramUniformMatrix2fv ((PFNGLPROGRAMUNIFORMMATRIX2FVPROC)gl_api._glProgramUniformMatrix2fv_ptr)
#define glProgramUniformMatrix3fv ((PFNGLPROGRAMUNIFORMMATRIX3FVPROC)gl_api._glProgramUniformMatrix3fv_ptr)
#define glProgramUniformMatrix4fv ((PFNGLPROGRAMUNIFORMMATRIX4FVPROC)gl_api._glProgramUniformMatrix4fv_ptr)
#define glProgramUniformMatrix2dv ((PFNGLPROGRAMUNIFORMMATRIX2DVPROC)gl_api._glProgramUniformMatrix2dv_ptr)
#define glProgramUniformMatrix3dv ((PFNGLPROGRAMUNIFORMMATRIX3DVPROC)gl_api._glProgramUniformMatrix3dv_ptr)
#define glProgramUniformMatrix4dv ((PFNGLPROGRAMUNIFORMMATRIX4DVPROC)gl_api._glProgramUniformMatrix4dv_ptr)
#define glProgramUniformMatrix2x3fv ((PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC)gl_api._glProgramUniformMatrix2x3fv_ptr)
#define glProgramUniformMatrix3x2fv ((PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC)gl_api._glProgramUniformMatrix3x2fv_ptr)
#define glProgramUniformMatrix2x4fv ((PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC)gl_api._glProgramUniformMatrix2x4fv_ptr)
#define glProgramUniformMatrix4x2fv ((PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC)gl_api._glProgramUniformMatrix4x2fv_ptr)
#define glProgramUniformMatrix3x4fv ((PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC)gl_api._glProgramUniformMatrix3x4fv_ptr)
#define glProgramUniformMatrix4x3fv ((PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC)gl_api._glProgramUniformMatrix4x3fv_ptr)
#define glProgramUniformMatrix2x3dv ((PFNGLPROGRAMUNIFORMMATRIX2X3DVPROC)gl_api._glProgramUniformMatrix2x3dv_ptr)
#define glProgramUniformMatrix3x2dv ((PFNGLPROGRAMUNIFORMMATRIX3X2DVPROC)gl_api._glProgramUniformMatrix3x2dv_ptr)
#define glProgramUniformMatrix2x4dv ((PFNGLPROGRAMUNIFORMMATRIX2X4DVPROC)gl_api._glProgramUniformMatrix2x4dv_ptr)
#define glProgramUniformMatrix4x2dv ((PFNGLPROGRAMUNIFORMMATRIX4X2DVPROC)gl_api._glProgramUniformMatrix4x2dv_ptr)
#define glProgramUniformMatrix3x4dv ((PFNGLPROGRAMUNIFORMMATRIX3X4DVPROC)gl_api._glProgramUniformMatrix3x4dv_ptr)
#define glProgramUniformMatrix4x3dv ((PFNGLPROGRAMUNIFORMMATRIX4X3DVPROC)gl_api._glProgramUniformMatrix4x3dv_ptr)
#define glValidateProgramPipeline ((PFNGLVALIDATEPROGRAMPIPELINEPROC)gl_api._glValidateProgramPipeline_ptr)
#define glGetProgramPipelineInfoLog ((PFNGLGETPROGRAMPIPELINEINFOLOGPROC)gl_api._glGetProgramPipelineInfoLog_ptr)
#define glVertexAttribL1d ((PFNGLVERTEXATTRIBL1DPROC)gl_api._glVertexAttribL1d_ptr)
#define glVertexAttribL2d ((PFNGLVERTEXATTRIBL2DPROC)gl_api._glVertexAttribL2d_ptr)
#define glVertexAttribL3d ((PFNGLVERTEXATTRIBL3DPROC)gl_api._glVertexAttribL3d_ptr)
#define glVertexAttribL4d ((PFNGLVERTEXATTRIBL4DPROC)gl_api._glVertexAttribL4d_ptr)
#define glVertexAttribL1dv ((PFNGLVERTEXATTRIBL1DVPROC)gl_api._glVertexAttribL1dv_ptr)
#define glVertexAttribL2dv ((PFNGLVERTEXATTRIBL2DVPROC)gl_api._glVertexAttribL2dv_ptr)
#define glVertexAttribL3dv ((PFNGLVERTEXATTRIBL3DVPROC)gl_api._glVertexAttribL3dv_ptr)
#define glVertexAttribL4dv ((PFNGLVERTEXATTRIBL4DVPROC)gl_api._glVertexAttribL4dv_ptr)
#define glVertexAttribLPointer ((PFNGLVERTEXATTRIBLPOINTERPROC)gl_api._glVertexAttribLPointer_ptr)
#define glGetVertexAttribLdv ((PFNGLGETVERTEXATTRIBLDVPROC)gl_api._glGetVertexAttribLdv_ptr)
#define glViewportArrayv ((PFNGLVIEWPORTARRAYVPROC)gl_api._glViewportArrayv_ptr)
#define glViewportIndexedf ((PFNGLVIEWPORTINDEXEDFPROC)gl_api._glViewportIndexedf_ptr)
#define glViewportIndexedfv ((PFNGLVIEWPORTINDEXEDFVPROC)gl_api._glViewportIndexedfv_ptr)
#define glScissorArrayv ((PFNGLSCISSORARRAYVPROC)gl_api._glScissorArrayv_ptr)
#define glScissorIndexed ((PFNGLSCISSORINDEXEDPROC)gl_api._glScissorIndexed_ptr)
#define glScissorIndexedv ((PFNGLSCISSORINDEXEDVPROC)gl_api._glScissorIndexedv_ptr)
#define glDepthRangeArrayv ((PFNGLDEPTHRANGEARRAYVPROC)gl_api._glDepthRangeArrayv_ptr)
#define glDepthRangeIndexed ((PFNGLDEPTHRANGEINDEXEDPROC)gl_api._glDepthRangeIndexed_ptr)
#define glGetFloati_v ((PFNGLGETFLOATI_VPROC)gl_api._glGetFloati_v_ptr)
#define glGetDoublei_v ((PFNGLGETDOUBLEI_VPROC)gl_api._glGetDoublei_v_ptr)
#define glCopyImageSubData ((PFNGLCOPYIMAGESUBDATAPROC)gl_api._glCopyImageSubData_ptr)

#endif

#endif
