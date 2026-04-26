#pragma once
#include <windows.h>

// These are the WGL entry points that GD calls when setting up OpenGL.
// We implement them by routing to ANGLE's EGL context.

extern "C" {
    __declspec(dllexport) HGLRC WINAPI wgl_wglCreateContext(HDC hdc);
    __declspec(dllexport) BOOL  WINAPI wgl_wglDeleteContext(HGLRC hglrc);
    __declspec(dllexport) BOOL  WINAPI wgl_wglMakeCurrent(HDC hdc, HGLRC hglrc);
    __declspec(dllexport) HGLRC WINAPI wgl_wglGetCurrentContext();
    __declspec(dllexport) HDC   WINAPI wgl_wglGetCurrentDC();
    __declspec(dllexport) PROC  WINAPI wgl_wglGetProcAddress(LPCSTR name);
    __declspec(dllexport) BOOL  WINAPI wgl_wglShareLists(HGLRC a, HGLRC b);
    __declspec(dllexport) BOOL  WINAPI wgl_wglSwapBuffers(HDC hdc);
    __declspec(dllexport) BOOL  WINAPI wgl_wglSwapIntervalEXT(int interval);
    __declspec(dllexport) int   WINAPI wgl_wglChoosePixelFormat(HDC hdc, const PIXELFORMATDESCRIPTOR* ppfd);
    __declspec(dllexport) BOOL  WINAPI wgl_wglSetPixelFormat(HDC hdc, int format, const PIXELFORMATDESCRIPTOR* ppfd);
    __declspec(dllexport) int   WINAPI wgl_wglDescribePixelFormat(HDC hdc, int format, UINT size, LPPIXELFORMATDESCRIPTOR ppfd);
    __declspec(dllexport) int   WINAPI wgl_wglGetPixelFormat(HDC hdc);
}
