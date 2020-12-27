#include "XR.hpp"
#include <GL/gl.h>

MyXrManager::MyXrManager(HDC hdc, HGLRC hglrc) {
    try {
        xr::InstanceCreateInfo instanceCreateInfo;
        instanceCreateInfo.applicationInfo = xr::ApplicationInfo("OpenGL 1.x OpenXR Injector");
        instanceCreateInfo.applicationInfo.apiVersion = xr::Version{XR_MAKE_VERSION(1, 0, 11)};
        std::vector<const char*> extensions;
        extensions.push_back("XR_KHR_opengl_enable");
        instanceCreateInfo.enabledExtensionCount = extensions.size();
        instanceCreateInfo.enabledExtensionNames = extensions.data();
        MessageBoxA(NULL, "Before CreateInstance", "XR", 0);
        m_Instance = xr::createInstance(instanceCreateInfo);
        m_Dispatch = xr::DispatchLoaderDynamic::createFullyPopulated(m_Instance);
        MessageBoxA(NULL, "Before GetSystem", "XR", 0);
        xr::SystemGetInfo systemGetInfo(xr::FormFactor::HeadMountedDisplay);
        m_System = m_Instance.getSystem(systemGetInfo, m_Dispatch);
        MessageBoxA(NULL, "Before Get Graphics Requirements", "XR", 0);

        auto requirements = m_Instance.getOpenGLGraphicsRequirementsKHR(m_System, m_Dispatch);
        char opengl_reporting[255];
        sprintf(opengl_reporting, "Minimum OpenGL: %i.%i.%i", requirements.minApiVersionSupported.major() , requirements.minApiVersionSupported.minor(), requirements.minApiVersionSupported.patch());
        MessageBoxA(NULL, opengl_reporting, "XR", 0); 
        const char *glversion = (const char*)glGetString(GL_VERSION);
        MessageBoxA(NULL, glversion, "OpenGL version", 0);
        xr::GraphicsBindingOpenGLWin32KHR graphicsBinding(hdc, hglrc);
        xr::SessionCreateInfo sessionCreateInfo({}, m_System);
        sessionCreateInfo.next = &graphicsBinding;
        m_Instance.createSession(sessionCreateInfo);
        MessageBoxA(NULL, "Created session!", "XR", 0);
    } catch(xr::exceptions::SystemError e) {
        MessageBoxA(NULL, "ERROR", "XR", 0);
        MessageBoxA(NULL, e.what(), "XR", 0);
        return;
    }

}