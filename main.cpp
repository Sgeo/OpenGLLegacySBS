#include <stdio.h>
#include <windows.h>
#include <GL/gl.h>

#include "vcpkg\\installed\\x86-windows\\include\\detours\\detours.h"

static void (WINAPI * trueGlViewport)(GLint, GLint, GLsizei, GLsizei) = glViewport;
static void (WINAPI * trueGlScissor)(GLint, GLint, GLsizei, GLsizei) = glScissor;
static void (WINAPI * trueGlMatrixMode)(GLenum) = glMatrixMode;
static void (WINAPI * trueGlDrawArrays)(GLenum mode, GLint first, GLsizei count) = glDrawArrays;
static void (WINAPI * trueGlDrawElements)(GLenum mode, GLsizei count, GLenum type, const void * indices) = glDrawElements;
static void (WINAPI * trueGlBegin)(GLenum mode) = glBegin;
static void (WINAPI * trueGlEnd)() = glEnd;

static void (WINAPI * trueGlDrawArraysEXT)(GLenum mode, GLint first, GLsizei count) = NULL;
static PROC (WINAPI * trueWglGetProcAddress)(LPCSTR) = wglGetProcAddress;

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

void leftSide(void) {
    trueGlViewport(currentViewport.x, currentViewport.y, currentViewport.width/2, currentViewport.height);
    trueGlScissor(currentScissor.x, currentScissor.y, currentScissor.width/2, currentScissor.height);
    trueGlMatrixMode(GL_PROJECTION); // The legacy matrix manipulation functions (e.g. glTranslatef and glMultMatrix) apply before the matrix on the stack
                                     // Thus, to apply to the result of modelmatrix, we need to apply before the projection matrix.
    glPushMatrix();
    glTranslatef(0.03, 0, 0);
}

void rightSide(void) {
    glPopMatrix();
    trueGlViewport(currentViewport.x + currentViewport.width/2, currentViewport.y, currentViewport.width/2, currentViewport.height);
    trueGlScissor(currentScissor.x + currentScissor.width/2, currentScissor.y, currentScissor.width/2, currentScissor.height);
    glPushMatrix();
    glTranslatef(-0.03, 0, 0);
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
    leftSide();
    trueGlDrawArrays(mode, first, count);
    rightSide();
    trueGlDrawArrays(mode, first, count);
    neutral();
}

void WINAPI hookedGlDrawArraysEXT(GLenum mode, GLint first, GLsizei count) {
    leftSide();
    trueGlDrawArraysEXT(mode, first, count);
    rightSide();
    trueGlDrawArraysEXT(mode, first, count);
    neutral();
}

void WINAPI hookedGlDrawElements(GLenum mode, GLsizei count, GLenum type, const void * indices) {
    leftSide();
    trueGlDrawElements(mode, count, type, indices);
    rightSide();
    trueGlDrawElements(mode, count, type, indices);
    neutral();
}

void WINAPI hookedGlBegin(GLenum mode) {
    if(!currentBeginEndList) {
        currentBeginEndList = glGenLists(1);
    }
    glNewList(currentBeginEndList, GL_COMPILE);
    trueGlBegin(mode);
}

void WINAPI hookedGlEnd() {
    trueGlEnd();
    glEndList();
    
    leftSide();
    glCallList(currentBeginEndList);
    rightSide();
    glCallList(currentBeginEndList);
    neutral();
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
        DetourAttach(&(PVOID&)trueWglGetProcAddress, hookedWglGetProcAddress);
        DetourAttach(&(PVOID&)trueGlBegin, hookedGlBegin);
        DetourAttach(&(PVOID&)trueGlEnd, hookedGlEnd);
        error = DetourTransactionCommit();

        if (error == NO_ERROR) {
            printf("ogldet" DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
                   " Detoured glFinish().\n");
        }
        else {
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
        DetourDetach(&(PVOID&)trueWglGetProcAddress, hookedWglGetProcAddress);
        DetourDetach(&(PVOID&)trueGlBegin, hookedGlBegin);
        DetourDetach(&(PVOID&)trueGlEnd, hookedGlEnd);
        error = DetourTransactionCommit();

        printf("ogldet" DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
               " Removed detour glFinish() (result=%d)\n", error);
        fflush(stdout);
    }

    return TRUE;
}