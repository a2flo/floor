/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2013 Florian Ziesche
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

#include "gl_support.hpp"
#include "core/logger.h"

// no need to get any fbo function pointers if glDrawPixels is used
#if !defined(OCLRASTER_USE_DRAW_PIXELS)

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

void init_gl_funcs() {
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
	if(_glIsRenderbuffer_ptr == nullptr) oclr_error("couldn't get function pointer to \"glIsRenderbuffer\"!");
	if(_glBindRenderbuffer_ptr == nullptr) oclr_error("couldn't get function pointer to \"glBindRenderbuffer\"!");
	if(_glDeleteRenderbuffers_ptr == nullptr) oclr_error("couldn't get function pointer to \"glDeleteRenderbuffers\"!");
	if(_glGenRenderbuffers_ptr == nullptr) oclr_error("couldn't get function pointer to \"glGenRenderbuffers\"!");
	if(_glRenderbufferStorage_ptr == nullptr) oclr_error("couldn't get function pointer to \"glRenderbufferStorage\"!");
	if(_glGetRenderbufferParameteriv_ptr == nullptr) oclr_error("couldn't get function pointer to \"glGetRenderbufferParameteriv\"!");
	if(_glIsFramebuffer_ptr == nullptr) oclr_error("couldn't get function pointer to \"glIsFramebuffer\"!");
	if(_glBindFramebuffer_ptr == nullptr) oclr_error("couldn't get function pointer to \"glBindFramebuffer\"!");
	if(_glDeleteFramebuffers_ptr == nullptr) oclr_error("couldn't get function pointer to \"glDeleteFramebuffers\"!");
	if(_glGenFramebuffers_ptr == nullptr) oclr_error("couldn't get function pointer to \"glGenFramebuffers\"!");
	if(_glCheckFramebufferStatus_ptr == nullptr) oclr_error("couldn't get function pointer to \"glCheckFramebufferStatus\"!");
	if(_glFramebufferTexture1D_ptr == nullptr) oclr_error("couldn't get function pointer to \"glFramebufferTexture1D\"!");
	if(_glFramebufferTexture2D_ptr == nullptr) oclr_error("couldn't get function pointer to \"glFramebufferTexture2D\"!");
	if(_glFramebufferTexture3D_ptr == nullptr) oclr_error("couldn't get function pointer to \"glFramebufferTexture3D\"!");
	if(_glFramebufferRenderbuffer_ptr == nullptr) oclr_error("couldn't get function pointer to \"glFramebufferRenderbuffer\"!");
	if(_glGetFramebufferAttachmentParameteriv_ptr == nullptr) oclr_error("couldn't get function pointer to \"glGetFramebufferAttachmentParameteriv\"!");
	if(_glGenerateMipmap_ptr == nullptr) oclr_error("couldn't get function pointer to \"glGenerateMipmap\"!");
	if(_glBlitFramebuffer_ptr == nullptr) oclr_error("couldn't get function pointer to \"glBlitFramebuffer\"!");
}
#else
void init_gl_funcs() {}
#endif

#endif
