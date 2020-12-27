#ifndef H_XR
#define H_XR

#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_WIN32
//#define OPENXR_HPP_NO_EXCEPTIONS
#include <windows.h>
#include <openxr/openxr.hpp>

class MyXrManager {
    private:
    xr::DispatchLoaderDynamic m_Dispatch;
    xr::Instance m_Instance;
    xr::Session m_Session;
    xr::SystemId m_System;

    public:
    MyXrManager(HDC, HGLRC);
};

#endif