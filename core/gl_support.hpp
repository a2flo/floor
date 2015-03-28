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

#ifndef __FLOOR_GL_SUPPORT_HPP__
#define __FLOOR_GL_SUPPORT_HPP__

#if defined(__APPLE__)

#if !defined(FLOOR_IOS)
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

extern "C" {
extern void glDrawPixels (GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
}
#else

#define CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE 0x10000000

#if defined(PLATFORM_X32) // arm7, arm7s (gl es 2.0 only hardware)
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>

// gl es compat
#define GL_RENDERBUFFER_SAMPLES GL_RENDERBUFFER_SAMPLES_APPLE
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_APPLE
#define GL_MAX_SAMPLES GL_MAX_SAMPLES_APPLE
#define GL_READ_FRAMEBUFFER GL_READ_FRAMEBUFFER_APPLE
#define GL_DRAW_FRAMEBUFFER GL_DRAW_FRAMEBUFFER_APPLE
#define GL_DRAW_FRAMEBUFFER_BINDING GL_DRAW_FRAMEBUFFER_BINDING_APPLE
#define GL_READ_FRAMEBUFFER_BINDING GL_READ_FRAMEBUFFER_BINDING_APPLE

#define GL_RED GL_RED_EXT
#define GL_RG GL_RG_EXT

#define GL_R16F GL_R16F_EXT
#define GL_RG16F GL_RG16F_EXT
#define GL_RGB16F GL_RGB16F_EXT
#define GL_RGBA16F GL_RGBA16F_EXT

#define GL_R8 GL_R8_EXT
#define GL_RG8 GL_RG8_EXT
#define GL_RGB8 GL_RGB8_OES
#define GL_RGBA8 GL_RGBA8_OES

#define GL_DEPTH_COMPONENT24 GL_DEPTH_COMPONENT24_OES
#define GL_DEPTH_STENCIL GL_DEPTH_STENCIL_OES
#define GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_OES

#define GL_HALF_FLOAT GL_HALF_FLOAT_OES

#define GL_TEXTURE_COMPARE_MODE GL_TEXTURE_COMPARE_MODE_EXT
#define GL_TEXTURE_COMPARE_FUNC GL_TEXTURE_COMPARE_FUNC_EXT
#define GL_COMPARE_REF_TO_TEXTURE GL_COMPARE_REF_TO_TEXTURE_EXT

#define GL_TEXTURE_MAX_LEVEL GL_TEXTURE_MAX_LEVEL_APPLE

#define glRenderbufferStorageMultisample glRenderbufferStorageMultisampleAPPLE
#define glResolveMultisampleFramebuffer glResolveMultisampleFramebufferAPPLE
#define glInvalidateFramebuffer glDiscardFramebufferEXT

#define glBindVertexArray glBindVertexArrayOES
#define glDeleteVertexArrays glDeleteVertexArraysOES
#define glGenVertexArrays glGenVertexArraysOES
#define glIsVertexArray glIsVertexArrayOES

#define glGetBufferPointerv glGetBufferPointervOES
#define glMapBufferRange glMapBufferRangeEXT
#define glFlushMappedBufferRange glFlushMappedBufferRangeEXT
#define glUnmapBuffer glUnmapBufferOES
#define GL_VERTEX_ARRAY_BINDING GL_VERTEX_ARRAY_BINDING_OES

#define GL_DEPTH_STENCIL GL_DEPTH_STENCIL_OES
#define GL_UNSIGNED_INT_24_8 GL_UNSIGNED_INT_24_8_OES
#define GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_OES
// TODO: check if this actually works
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A

// unsupported
#define GL_TEXTURE_2D_ARRAY GL_TEXTURE_2D

#else // arm64 (gl es 3.0 hardware)
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>
// NOTE: opengl es 3.0 supports all of the above (except for glResolveMultisampleFramebuffer which was replaced by glBlitFramebuffer)
#endif

// generic redefines (apply to both es 2.0 and 3.0)
#define glClearDepth glClearDepthf

#endif

// we only need to get opengl functions pointers on windows, linux, *bsd, ...
#else
#include <floor/core/cpp_headers.hpp>

#if defined(MINGW)
#define GL3_PROTOTYPES
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#if !defined(WIN_UNIXENV)
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

//
void init_gl_funcs();

//
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

// OpenGL 1.5
OGL_API extern PFNGLGENQUERIESPROC _glGenQueries_ptr;
OGL_API extern PFNGLDELETEQUERIESPROC _glDeleteQueries_ptr;
OGL_API extern PFNGLISQUERYPROC _glIsQuery_ptr;
OGL_API extern PFNGLBEGINQUERYPROC _glBeginQuery_ptr;
OGL_API extern PFNGLENDQUERYPROC _glEndQuery_ptr;
OGL_API extern PFNGLGETQUERYIVPROC _glGetQueryiv_ptr;
OGL_API extern PFNGLGETQUERYOBJECTIVPROC _glGetQueryObjectiv_ptr;
OGL_API extern PFNGLGETQUERYOBJECTUIVPROC _glGetQueryObjectuiv_ptr;
OGL_API extern PFNGLBINDBUFFERPROC _glBindBuffer_ptr;
OGL_API extern PFNGLDELETEBUFFERSPROC _glDeleteBuffers_ptr;
OGL_API extern PFNGLGENBUFFERSPROC _glGenBuffers_ptr;
OGL_API extern PFNGLISBUFFERPROC _glIsBuffer_ptr;
OGL_API extern PFNGLBUFFERDATAPROC _glBufferData_ptr;
OGL_API extern PFNGLBUFFERSUBDATAPROC _glBufferSubData_ptr;
OGL_API extern PFNGLGETBUFFERSUBDATAPROC _glGetBufferSubData_ptr;
OGL_API extern PFNGLMAPBUFFERPROC _glMapBuffer_ptr;
OGL_API extern PFNGLUNMAPBUFFERPROC _glUnmapBuffer_ptr;
OGL_API extern PFNGLGETBUFFERPARAMETERIVPROC _glGetBufferParameteriv_ptr;
OGL_API extern PFNGLGETBUFFERPOINTERVPROC _glGetBufferPointerv_ptr;

// OpenGL 2.0
OGL_API extern PFNGLBLENDEQUATIONSEPARATEPROC _glBlendEquationSeparate_ptr;
OGL_API extern PFNGLDRAWBUFFERSPROC _glDrawBuffers_ptr;
OGL_API extern PFNGLSTENCILOPSEPARATEPROC _glStencilOpSeparate_ptr;
OGL_API extern PFNGLSTENCILFUNCSEPARATEPROC _glStencilFuncSeparate_ptr;
OGL_API extern PFNGLSTENCILMASKSEPARATEPROC _glStencilMaskSeparate_ptr;
OGL_API extern PFNGLATTACHSHADERPROC _glAttachShader_ptr;
OGL_API extern PFNGLBINDATTRIBLOCATIONPROC _glBindAttribLocation_ptr;
OGL_API extern PFNGLCOMPILESHADERPROC _glCompileShader_ptr;
OGL_API extern PFNGLCREATEPROGRAMPROC _glCreateProgram_ptr;
OGL_API extern PFNGLCREATESHADERPROC _glCreateShader_ptr;
OGL_API extern PFNGLDELETEPROGRAMPROC _glDeleteProgram_ptr;
OGL_API extern PFNGLDELETESHADERPROC _glDeleteShader_ptr;
OGL_API extern PFNGLDETACHSHADERPROC _glDetachShader_ptr;
OGL_API extern PFNGLDISABLEVERTEXATTRIBARRAYPROC _glDisableVertexAttribArray_ptr;
OGL_API extern PFNGLENABLEVERTEXATTRIBARRAYPROC _glEnableVertexAttribArray_ptr;
OGL_API extern PFNGLGETACTIVEATTRIBPROC _glGetActiveAttrib_ptr;
OGL_API extern PFNGLGETACTIVEUNIFORMPROC _glGetActiveUniform_ptr;
OGL_API extern PFNGLGETATTACHEDSHADERSPROC _glGetAttachedShaders_ptr;
OGL_API extern PFNGLGETATTRIBLOCATIONPROC _glGetAttribLocation_ptr;
OGL_API extern PFNGLGETPROGRAMIVPROC _glGetProgramiv_ptr;
OGL_API extern PFNGLGETPROGRAMINFOLOGPROC _glGetProgramInfoLog_ptr;
OGL_API extern PFNGLGETSHADERIVPROC _glGetShaderiv_ptr;
OGL_API extern PFNGLGETSHADERINFOLOGPROC _glGetShaderInfoLog_ptr;
OGL_API extern PFNGLGETSHADERSOURCEPROC _glGetShaderSource_ptr;
OGL_API extern PFNGLGETUNIFORMLOCATIONPROC _glGetUniformLocation_ptr;
OGL_API extern PFNGLGETUNIFORMFVPROC _glGetUniformfv_ptr;
OGL_API extern PFNGLGETUNIFORMIVPROC _glGetUniformiv_ptr;
OGL_API extern PFNGLGETVERTEXATTRIBDVPROC _glGetVertexAttribdv_ptr;
OGL_API extern PFNGLGETVERTEXATTRIBFVPROC _glGetVertexAttribfv_ptr;
OGL_API extern PFNGLGETVERTEXATTRIBIVPROC _glGetVertexAttribiv_ptr;
OGL_API extern PFNGLGETVERTEXATTRIBPOINTERVPROC _glGetVertexAttribPointerv_ptr;
OGL_API extern PFNGLISPROGRAMPROC _glIsProgram_ptr;
OGL_API extern PFNGLISSHADERPROC _glIsShader_ptr;
OGL_API extern PFNGLLINKPROGRAMPROC _glLinkProgram_ptr;
OGL_API extern PFNGLSHADERSOURCEPROC _glShaderSource_ptr;
OGL_API extern PFNGLUSEPROGRAMPROC _glUseProgram_ptr;
OGL_API extern PFNGLUNIFORM1FPROC _glUniform1f_ptr;
OGL_API extern PFNGLUNIFORM2FPROC _glUniform2f_ptr;
OGL_API extern PFNGLUNIFORM3FPROC _glUniform3f_ptr;
OGL_API extern PFNGLUNIFORM4FPROC _glUniform4f_ptr;
OGL_API extern PFNGLUNIFORM1IPROC _glUniform1i_ptr;
OGL_API extern PFNGLUNIFORM2IPROC _glUniform2i_ptr;
OGL_API extern PFNGLUNIFORM3IPROC _glUniform3i_ptr;
OGL_API extern PFNGLUNIFORM4IPROC _glUniform4i_ptr;
OGL_API extern PFNGLUNIFORM1FVPROC _glUniform1fv_ptr;
OGL_API extern PFNGLUNIFORM2FVPROC _glUniform2fv_ptr;
OGL_API extern PFNGLUNIFORM3FVPROC _glUniform3fv_ptr;
OGL_API extern PFNGLUNIFORM4FVPROC _glUniform4fv_ptr;
OGL_API extern PFNGLUNIFORM1IVPROC _glUniform1iv_ptr;
OGL_API extern PFNGLUNIFORM2IVPROC _glUniform2iv_ptr;
OGL_API extern PFNGLUNIFORM3IVPROC _glUniform3iv_ptr;
OGL_API extern PFNGLUNIFORM4IVPROC _glUniform4iv_ptr;
OGL_API extern PFNGLUNIFORMMATRIX2FVPROC _glUniformMatrix2fv_ptr;
OGL_API extern PFNGLUNIFORMMATRIX3FVPROC _glUniformMatrix3fv_ptr;
OGL_API extern PFNGLUNIFORMMATRIX4FVPROC _glUniformMatrix4fv_ptr;
OGL_API extern PFNGLVALIDATEPROGRAMPROC _glValidateProgram_ptr;
OGL_API extern PFNGLVERTEXATTRIB1DPROC _glVertexAttrib1d_ptr;
OGL_API extern PFNGLVERTEXATTRIB1DVPROC _glVertexAttrib1dv_ptr;
OGL_API extern PFNGLVERTEXATTRIB1FPROC _glVertexAttrib1f_ptr;
OGL_API extern PFNGLVERTEXATTRIB1FVPROC _glVertexAttrib1fv_ptr;
OGL_API extern PFNGLVERTEXATTRIB1SPROC _glVertexAttrib1s_ptr;
OGL_API extern PFNGLVERTEXATTRIB1SVPROC _glVertexAttrib1sv_ptr;
OGL_API extern PFNGLVERTEXATTRIB2DPROC _glVertexAttrib2d_ptr;
OGL_API extern PFNGLVERTEXATTRIB2DVPROC _glVertexAttrib2dv_ptr;
OGL_API extern PFNGLVERTEXATTRIB2FPROC _glVertexAttrib2f_ptr;
OGL_API extern PFNGLVERTEXATTRIB2FVPROC _glVertexAttrib2fv_ptr;
OGL_API extern PFNGLVERTEXATTRIB2SPROC _glVertexAttrib2s_ptr;
OGL_API extern PFNGLVERTEXATTRIB2SVPROC _glVertexAttrib2sv_ptr;
OGL_API extern PFNGLVERTEXATTRIB3DPROC _glVertexAttrib3d_ptr;
OGL_API extern PFNGLVERTEXATTRIB3DVPROC _glVertexAttrib3dv_ptr;
OGL_API extern PFNGLVERTEXATTRIB3FPROC _glVertexAttrib3f_ptr;
OGL_API extern PFNGLVERTEXATTRIB3FVPROC _glVertexAttrib3fv_ptr;
OGL_API extern PFNGLVERTEXATTRIB3SPROC _glVertexAttrib3s_ptr;
OGL_API extern PFNGLVERTEXATTRIB3SVPROC _glVertexAttrib3sv_ptr;
OGL_API extern PFNGLVERTEXATTRIB4NBVPROC _glVertexAttrib4Nbv_ptr;
OGL_API extern PFNGLVERTEXATTRIB4NIVPROC _glVertexAttrib4Niv_ptr;
OGL_API extern PFNGLVERTEXATTRIB4NSVPROC _glVertexAttrib4Nsv_ptr;
OGL_API extern PFNGLVERTEXATTRIB4NUBPROC _glVertexAttrib4Nub_ptr;
OGL_API extern PFNGLVERTEXATTRIB4NUBVPROC _glVertexAttrib4Nubv_ptr;
OGL_API extern PFNGLVERTEXATTRIB4NUIVPROC _glVertexAttrib4Nuiv_ptr;
OGL_API extern PFNGLVERTEXATTRIB4NUSVPROC _glVertexAttrib4Nusv_ptr;
OGL_API extern PFNGLVERTEXATTRIB4BVPROC _glVertexAttrib4bv_ptr;
OGL_API extern PFNGLVERTEXATTRIB4DPROC _glVertexAttrib4d_ptr;
OGL_API extern PFNGLVERTEXATTRIB4DVPROC _glVertexAttrib4dv_ptr;
OGL_API extern PFNGLVERTEXATTRIB4FPROC _glVertexAttrib4f_ptr;
OGL_API extern PFNGLVERTEXATTRIB4FVPROC _glVertexAttrib4fv_ptr;
OGL_API extern PFNGLVERTEXATTRIB4IVPROC _glVertexAttrib4iv_ptr;
OGL_API extern PFNGLVERTEXATTRIB4SPROC _glVertexAttrib4s_ptr;
OGL_API extern PFNGLVERTEXATTRIB4SVPROC _glVertexAttrib4sv_ptr;
OGL_API extern PFNGLVERTEXATTRIB4UBVPROC _glVertexAttrib4ubv_ptr;
OGL_API extern PFNGLVERTEXATTRIB4UIVPROC _glVertexAttrib4uiv_ptr;
OGL_API extern PFNGLVERTEXATTRIB4USVPROC _glVertexAttrib4usv_ptr;
OGL_API extern PFNGLVERTEXATTRIBPOINTERPROC _glVertexAttribPointer_ptr;

//
OGL_API extern PFNGLISRENDERBUFFERPROC _glIsRenderbuffer_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLBINDRENDERBUFFERPROC _glBindRenderbuffer_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLDELETERENDERBUFFERSPROC _glDeleteRenderbuffers_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLGENRENDERBUFFERSPROC _glGenRenderbuffers_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLRENDERBUFFERSTORAGEPROC _glRenderbufferStorage_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLGETRENDERBUFFERPARAMETERIVPROC _glGetRenderbufferParameteriv_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLISFRAMEBUFFERPROC _glIsFramebuffer_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLBINDFRAMEBUFFERPROC _glBindFramebuffer_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLDELETEFRAMEBUFFERSPROC _glDeleteFramebuffers_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLGENFRAMEBUFFERSPROC _glGenFramebuffers_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLCHECKFRAMEBUFFERSTATUSPROC _glCheckFramebufferStatus_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLFRAMEBUFFERTEXTURE1DPROC _glFramebufferTexture1D_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLFRAMEBUFFERTEXTURE2DPROC _glFramebufferTexture2D_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLFRAMEBUFFERTEXTURE3DPROC _glFramebufferTexture3D_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLFRAMEBUFFERRENDERBUFFERPROC _glFramebufferRenderbuffer_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC _glGetFramebufferAttachmentParameteriv_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLGENERATEMIPMAPPROC _glGenerateMipmap_ptr; // ARB_framebuffer_object
OGL_API extern PFNGLBLITFRAMEBUFFERPROC _glBlitFramebuffer_ptr; // ARB_framebuffer_object

#define glIsRenderbuffer ((PFNGLISRENDERBUFFERPROC)_glIsRenderbuffer_ptr)
#define glBindRenderbuffer ((PFNGLBINDRENDERBUFFERPROC)_glBindRenderbuffer_ptr)
#define glDeleteRenderbuffers ((PFNGLDELETERENDERBUFFERSPROC)_glDeleteRenderbuffers_ptr)
#define glGenRenderbuffers ((PFNGLGENRENDERBUFFERSPROC)_glGenRenderbuffers_ptr)
#define glRenderbufferStorage ((PFNGLRENDERBUFFERSTORAGEPROC)_glRenderbufferStorage_ptr)
#define glGetRenderbufferParameteriv ((PFNGLGETRENDERBUFFERPARAMETERIVPROC)_glGetRenderbufferParameteriv_ptr)
#define glIsFramebuffer ((PFNGLISFRAMEBUFFERPROC)_glIsFramebuffer_ptr)
#define glBindFramebuffer ((PFNGLBINDFRAMEBUFFERPROC)_glBindFramebuffer_ptr)
#define glDeleteFramebuffers ((PFNGLDELETEFRAMEBUFFERSPROC)_glDeleteFramebuffers_ptr)
#define glGenFramebuffers ((PFNGLGENFRAMEBUFFERSPROC)_glGenFramebuffers_ptr)
#define glCheckFramebufferStatus ((PFNGLCHECKFRAMEBUFFERSTATUSPROC)_glCheckFramebufferStatus_ptr)
#define glFramebufferTexture1D ((PFNGLFRAMEBUFFERTEXTURE1DPROC)_glFramebufferTexture1D_ptr)
#define glFramebufferTexture2D ((PFNGLFRAMEBUFFERTEXTURE2DPROC)_glFramebufferTexture2D_ptr)
#define glFramebufferTexture3D ((PFNGLFRAMEBUFFERTEXTURE3DPROC)_glFramebufferTexture3D_ptr)
#define glFramebufferRenderbuffer ((PFNGLFRAMEBUFFERRENDERBUFFERPROC)_glFramebufferRenderbuffer_ptr)
#define glGetFramebufferAttachmentParameteriv ((PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)_glGetFramebufferAttachmentParameteriv_ptr)
#define glGenerateMipmap ((PFNGLGENERATEMIPMAPPROC)_glGenerateMipmap_ptr)
#define glBlitFramebuffer ((PFNGLBLITFRAMEBUFFERPROC)_glBlitFramebuffer_ptr)

#endif

#endif
