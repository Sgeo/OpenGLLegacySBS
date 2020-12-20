#include <stdio.h>
#include <windows.h>
#include <GL/gl.h>

//#include "vcpkg\\installed\\x86-windows\\include\\glm\\glm.hpp"
//#include "vcpkg\\installed\\x86-windows\\include\\glm\\gtc\\type_ptr.hpp"

#include <detours/detours.h>

#include <openxr/openxr.hpp>

//#define OPENGLIDE

static void (WINAPI * trueGlViewport)(GLint, GLint, GLsizei, GLsizei) = glViewport;
static void (WINAPI * trueGlScissor)(GLint, GLint, GLsizei, GLsizei) = glScissor;
static void (WINAPI * trueGlMatrixMode)(GLenum) = glMatrixMode;
static void (WINAPI * trueGlDrawArrays)(GLenum mode, GLint first, GLsizei count) = glDrawArrays;
static void (WINAPI * trueGlDrawElements)(GLenum mode, GLsizei count, GLenum type, const void * indices) = glDrawElements;
static void (WINAPI * trueGlCallList)(GLuint list) = glCallList;
static void (WINAPI * trueGlCallLists)(GLsizei n, GLenum type, const void * lists) = glCallLists;
static void (WINAPI * trueGlBegin)(GLenum mode) = glBegin;
static void (WINAPI * trueGlEnd)() = glEnd;
static void (WINAPI * trueGlNewList)(GLuint list, GLenum mode) = glNewList;
static void (WINAPI * trueGlEndList)() = glEndList;

static void (WINAPI * trueGlDrawArraysEXT)(GLenum mode, GLint first, GLsizei count) = NULL;
static PROC (WINAPI * trueWglGetProcAddress)(LPCSTR) = wglGetProcAddress;

#ifdef OPENGLIDE
static void (WINAPI * trueGlVertex3fv)(const GLfloat *v) = glVertex3fv;
#endif

float SEPARATION = 0.03;
float CONVERGENCE = 0.0;

float leftEyeMatrixData[] = {
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    SEPARATION * CONVERGENCE, 0.0, 1.0, 0.0,
    SEPARATION, 0.0, 0.0, 1.0
};

//glm::mat4 leftEyeMatrix = glm::make_mat4(leftEyeMatrixData);

float rightEyeMatrixData[] = {
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    -SEPARATION * CONVERGENCE, 0.0, 1.0, 0.0,
    -SEPARATION, 0.0, 0.0, 1.0
};

#ifdef OPENGLIDE
float reprojectMatrixData[] = {
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 1.0,
    0.0, 0.0, 0.0, 0.0
};
#endif

//glm::mat4 rightEyeMatrix = glm::make_mat4(rightEyeMatrixData);

typedef struct Viewport {
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
} Viewport;

typedef struct Scissor {
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
} Scissor;

static Viewport currentViewport;

static Scissor currentScissor;

static GLenum currentMatrixMode;

static GLuint currentBeginEndList;

static GLenum currentListMode = 0;
static GLuint currentList = 0;


void lazyInitViewport() {
    if(!currentViewport.width) {
        GLint viewport[] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, viewport);
        currentViewport.x = viewport[0];
        currentViewport.y = viewport[1];
        currentViewport.width = viewport[2];
        currentViewport.height = viewport[3];
    }
}

void leftSide(void) {
    lazyInitViewport();
    trueGlViewport(currentViewport.x, currentViewport.y, currentViewport.width/2, currentViewport.height);
    trueGlScissor(currentScissor.x, currentScissor.y, currentScissor.width/2, currentScissor.height);
    trueGlMatrixMode(GL_PROJECTION); // The legacy matrix manipulation functions (e.g. glTranslatef and glMultMatrix) apply before the matrix on the stack
                                     // Thus, to apply to the result of modelmatrix, we need to apply before the projection matrix.
    glPushMatrix();
    #ifdef OPENGLIDE
    glMultMatrixf(reprojectMatrixData);
    #endif
    glMultMatrixf(leftEyeMatrixData);
}

void rightSide(void) {
    glPopMatrix();
    trueGlViewport(currentViewport.x + currentViewport.width/2, currentViewport.y, currentViewport.width/2, currentViewport.height);
    trueGlScissor(currentScissor.x + currentScissor.width/2, currentScissor.y, currentScissor.width/2, currentScissor.height);
    glPushMatrix();
    #ifdef OPENGLIDE
    glMultMatrixf(reprojectMatrixData);
    #endif
    glMultMatrixf(rightEyeMatrixData);
}


void neutral(void) {
    glPopMatrix();
    trueGlMatrixMode(currentMatrixMode);
    trueGlViewport(currentViewport.x, currentViewport.y, currentViewport.width, currentViewport.height);
    trueGlScissor(currentScissor.x, currentScissor.y, currentScissor.width, currentScissor.height);
}

void WINAPI hookedGlViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    currentViewport.x = x;
    currentViewport.y = y;
    currentViewport.width = width;
    currentViewport.height = height;
    trueGlViewport(x, y, width, height);
}

void WINAPI hookedGlScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    currentScissor.x = x;
    currentScissor.y = y;
    currentScissor.width = width;
    currentScissor.height = height;
    trueGlScissor(x, y, width, height);
}

void WINAPI hookedGlMatrixMode(GLenum mode) {
    currentMatrixMode = mode;
    trueGlMatrixMode(mode);
}

void WINAPI hookedGlDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if(!currentListMode) {
        leftSide();
        trueGlDrawArrays(mode, first, count);
        rightSide();
        trueGlDrawArrays(mode, first, count);
        neutral();
    } else {
        trueGlDrawArrays(mode, first, count);
    }
}

void WINAPI hookedGlDrawArraysEXT(GLenum mode, GLint first, GLsizei count) {
    if(!currentListMode) {
        leftSide();
        trueGlDrawArraysEXT(mode, first, count);
        rightSide();
        trueGlDrawArraysEXT(mode, first, count);
        neutral();
    } else {
        trueGlDrawArraysEXT(mode, first, count);
    }
}

void WINAPI hookedGlDrawElements(GLenum mode, GLsizei count, GLenum type, const void * indices) {
    if(!currentListMode) {
        leftSide();
        trueGlDrawElements(mode, count, type, indices);
        rightSide();
        trueGlDrawElements(mode, count, type, indices);
        neutral();
    } else {
        trueGlDrawElements(mode, count, type, indices);
    }

}

void WINAPI hookedGlCallList(GLuint list) {
    if(!currentListMode) {
        leftSide();
        trueGlCallList(list);
        rightSide();
        trueGlCallList(list);
        neutral();
    } else {
        trueGlCallList(list);
    }
}

void WINAPI hookedGlCallLists(GLsizei n, GLenum type, const void * lists) {
    if(!currentListMode) {
        leftSide();
        trueGlCallLists(n, type, lists);
        rightSide();
        trueGlCallLists(n, type, lists);
        neutral();
    } else {
        trueGlCallLists(n, type, lists);
    }
}

void WINAPI hookedGlBegin(GLenum mode) {
    if(!currentBeginEndList) {
        currentBeginEndList = glGenLists(1);
    }
    if(!currentListMode) {
        glGetError();
        trueGlNewList(currentBeginEndList, GL_COMPILE);
        GLenum error = glGetError();
        if(error) {
            MessageBoxA(NULL, "Error!", "Hook", 0);
        }
    }
    trueGlBegin(mode);
}

void WINAPI hookedGlEnd() {
    trueGlEnd();
    if(!currentListMode) {
        trueGlEndList();
        glCallList(currentBeginEndList); // Call faked glCallList to have it do stereo
    }
}

void WINAPI hookedGlNewList(GLuint list, GLenum mode) {
    currentList = list;
    currentListMode = mode;
    trueGlNewList(list, GL_COMPILE);
}

void WINAPI hookedGlEndList() {
    trueGlEndList();
    int execute = currentListMode == GL_COMPILE_AND_EXECUTE;
    currentList = 0;
    currentListMode = 0;
    if(execute) {
        glCallList(currentList);
    }
}



PROC WINAPI hookedWglGetProcAddress(LPCSTR procName) {
    //MessageBoxA(NULL, procName, "wglGetProcAddress", 0);
    if(strcmp("glDrawArraysEXT", procName) == 0) {
        trueGlDrawArraysEXT = (void (WINAPI *)(GLenum, GLint, GLsizei))trueWglGetProcAddress("glDrawArraysEXT");
        return (PROC)hookedGlDrawArraysEXT;
    } else {
        return trueWglGetProcAddress(procName);
    }
}

#ifdef OPENGLIDE
void WINAPI hookedGlVertex3fv(const GLfloat *v) {
    //printf("vertex: %f, %f, %f\n", v[0], v[1], v[2]);
    GLfloat mine[3] = {v[0], v[1], v[2]};
    if(v[2]) {
        mine[0] *= v[2];
        mine[1] *= v[2];
        mine[2] *= v[2];
    }
    trueGlVertex3fv(mine);
}
#endif

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
    LONG error;
    (void)hinst;
    (void)reserved;
    

    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    if (dwReason == DLL_PROCESS_ATTACH) {
        DetourRestoreAfterWith();

        printf("ogldet" DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
               " Starting.\n");
        fflush(stdout);

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)trueGlViewport, hookedGlViewport);
        DetourAttach(&(PVOID&)trueGlScissor, hookedGlScissor);
        DetourAttach(&(PVOID&)trueGlMatrixMode, hookedGlMatrixMode);
        DetourAttach(&(PVOID&)trueGlDrawArrays, hookedGlDrawArrays);
        DetourAttach(&(PVOID&)trueGlDrawElements, hookedGlDrawElements);
        DetourAttach(&(PVOID&)trueGlCallList, hookedGlCallList);
        DetourAttach(&(PVOID&)trueGlCallLists, hookedGlCallLists);
        DetourAttach(&(PVOID&)trueWglGetProcAddress, hookedWglGetProcAddress);
        DetourAttach(&(PVOID&)trueGlBegin, hookedGlBegin);
        DetourAttach(&(PVOID&)trueGlEnd, hookedGlEnd);
        DetourAttach(&(PVOID&)trueGlNewList, hookedGlNewList);
        DetourAttach(&(PVOID&)trueGlEndList, hookedGlEndList);
        #ifdef OPENGLIDE
        DetourAttach(&(PVOID&)trueGlVertex3fv, hookedGlVertex3fv);
        #endif
        error = DetourTransactionCommit();

        if (error == NO_ERROR) {
            MessageBoxA(NULL, "Successfully hooked", "Hook", 0);
            printf("ogldet" DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
                   " Detoured glFinish().\n");
        }
        else {
            //MessageBoxA(NULL, "Hook unsuccessful", "Hook", 0);
            printf("ogldet" DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
                   " Error detouring glFinish(): %d\n", error);
        }
    }
    else if (dwReason == DLL_PROCESS_DETACH) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)trueGlViewport, hookedGlViewport);
        DetourDetach(&(PVOID&)trueGlScissor, hookedGlScissor);
        DetourDetach(&(PVOID&)trueGlMatrixMode, hookedGlMatrixMode);
        DetourDetach(&(PVOID&)trueGlDrawArrays, hookedGlDrawArrays);
        DetourDetach(&(PVOID&)trueGlDrawElements, hookedGlDrawElements);
        DetourDetach(&(PVOID&)trueGlCallList, hookedGlCallList);
        DetourDetach(&(PVOID&)trueGlCallLists, hookedGlCallLists);
        DetourDetach(&(PVOID&)trueWglGetProcAddress, hookedWglGetProcAddress);
        DetourDetach(&(PVOID&)trueGlBegin, hookedGlBegin);
        DetourDetach(&(PVOID&)trueGlEnd, hookedGlEnd);
        DetourDetach(&(PVOID&)trueGlNewList, hookedGlNewList);
        DetourDetach(&(PVOID&)trueGlEndList, hookedGlEndList);
        #ifdef OPENGLIDE
        DetourDetach(&(PVOID&)trueGlVertex3fv, hookedGlVertex3fv);
        #endif
        error = DetourTransactionCommit();

        printf("ogldet" DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
               " Removed detour glFinish() (result=%d)\n", error);
        fflush(stdout);
    }

    return TRUE;
}