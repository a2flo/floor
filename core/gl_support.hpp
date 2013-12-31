/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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
#include "core/cpp_headers.hpp"

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
