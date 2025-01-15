/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#ifndef SDL_cocoawindow_h_
#define SDL_cocoawindow_h_

#import <Cocoa/Cocoa.h>

#ifdef SDL_VIDEO_OPENGL_EGL
#include "../SDL_egl_c.h"
#endif

@class SDL_WindowData;

typedef enum
{
    PENDING_OPERATION_NONE,
    PENDING_OPERATION_ENTER_FULLSCREEN,
    PENDING_OPERATION_LEAVE_FULLSCREEN,
    PENDING_OPERATION_MINIMIZE
} PendingWindowOperation;

@interface Cocoa_WindowListener : NSResponder <NSWindowDelegate> {
    /* SDL_WindowData owns this Listener and has a strong reference to it.
     * To avoid reference cycles, we could have either a weak or an
     * unretained ref to the WindowData. */
    __weak SDL_WindowData *_data;
    BOOL observingVisible;
    BOOL wasCtrlLeft;
    BOOL wasVisible;
    BOOL isFullscreenSpace;
    BOOL inFullscreenTransition;
    PendingWindowOperation pendingWindowOperation;
    BOOL isMoving;
    NSInteger focusClickPending;
    int pendingWindowWarpX, pendingWindowWarpY;
    BOOL isDragAreaRunning;
    NSTimer *liveResizeTimer;
}

-(BOOL) isTouchFromTrackpad:(NSEvent *)theEvent;
-(void) listen:(SDL_WindowData *) data;
-(void) pauseVisibleObservation;
-(void) resumeVisibleObservation;
-(BOOL) setFullscreenSpace:(BOOL) state;
-(BOOL) isInFullscreenSpace;
-(BOOL) isInFullscreenSpaceTransition;
-(void) addPendingWindowOperation:(PendingWindowOperation) operation;
-(void) close;

-(BOOL) isMoving;
-(BOOL) isMovingOrFocusClickPending;
-(void) setFocusClickPending:(NSInteger) button;
-(void) clearFocusClickPending:(NSInteger) button;
-(void) setPendingMoveX:(int)x Y:(int)y;
-(void) windowDidFinishMoving;
-(void) onMovingOrFocusClickPendingStateCleared;

/* Window delegate functionality */
-(BOOL) windowShouldClose:(id) sender;
-(void) windowDidExpose:(NSNotification *) aNotification;
-(void) onLiveResizeTimerFire:(id) sender;
-(void) windowWillStartLiveResize:(NSNotification *)aNotification;
-(void) windowDidEndLiveResize:(NSNotification *)aNotification;
-(void) windowDidMove:(NSNotification *) aNotification;
-(void) windowDidResize:(NSNotification *) aNotification;
-(void) windowDidMiniaturize:(NSNotification *) aNotification;
-(void) windowDidDeminiaturize:(NSNotification *) aNotification;
-(void) windowDidBecomeKey:(NSNotification *) aNotification;
-(void) windowDidResignKey:(NSNotification *) aNotification;
-(void) windowDidChangeBackingProperties:(NSNotification *) aNotification;
-(void) windowDidChangeScreenProfile:(NSNotification *) aNotification;
-(void) windowDidChangeScreen:(NSNotification *) aNotification;
-(void) windowWillEnterFullScreen:(NSNotification *) aNotification;
-(void) windowDidEnterFullScreen:(NSNotification *) aNotification;
-(void) windowWillExitFullScreen:(NSNotification *) aNotification;
-(void) windowDidExitFullScreen:(NSNotification *) aNotification;
-(NSApplicationPresentationOptions)window:(NSWindow *)window willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)proposedOptions;

/* See if event is in a drag area, toggle on window dragging. */
-(BOOL) processHitTest:(NSEvent *)theEvent;

/* Window event handling */
-(void) mouseDown:(NSEvent *) theEvent;
-(void) rightMouseDown:(NSEvent *) theEvent;
-(void) otherMouseDown:(NSEvent *) theEvent;
-(void) mouseUp:(NSEvent *) theEvent;
-(void) rightMouseUp:(NSEvent *) theEvent;
-(void) otherMouseUp:(NSEvent *) theEvent;
-(void) mouseMoved:(NSEvent *) theEvent;
-(void) mouseDragged:(NSEvent *) theEvent;
-(void) rightMouseDragged:(NSEvent *) theEvent;
-(void) otherMouseDragged:(NSEvent *) theEvent;
-(void) scrollWheel:(NSEvent *) theEvent;
-(void) touchesBeganWithEvent:(NSEvent *) theEvent;
-(void) touchesMovedWithEvent:(NSEvent *) theEvent;
-(void) touchesEndedWithEvent:(NSEvent *) theEvent;
-(void) touchesCancelledWithEvent:(NSEvent *) theEvent;

/* Touch event handling */
-(void) handleTouches:(NSTouchPhase) phase withEvent:(NSEvent*) theEvent;

@end
/* *INDENT-ON* */

@class SDLOpenGLContext;

@interface SDL_WindowData : NSObject
    @property (nonatomic) SDL_Window *window;
    @property (nonatomic) NSWindow *nswindow;
    @property (nonatomic) NSView *sdlContentView;
#ifdef SDL_VIDEO_OPENGL_CGL
    @property (nonatomic) NSMutableArray *nscontexts;
#endif
    @property (nonatomic) SDL_bool inWindowFullscreenTransition;
    @property (nonatomic) NSInteger window_number;
    @property (nonatomic) NSInteger flash_request;
    @property (nonatomic) Cocoa_WindowListener *listener;
#ifdef SDL_VIDEO_OPENGL_EGL
    @property (nonatomic) EGLSurface egl_surface;
#endif
@end

extern int Cocoa_CreateSDLWindow(_THIS, SDL_Window * window);
// extern int Cocoa_CreateSDLWindowFrom(_THIS, SDL_Window * window, const void *data);
extern void Cocoa_SetWindowTitle(SDL_Window * window);
extern void Cocoa_SetWindowIcon(SDL_Window * window, SDL_Surface * icon);
extern void Cocoa_SetWindowPosition(SDL_Window * window);
extern void Cocoa_SetWindowSize(SDL_Window * window);
extern void Cocoa_SetWindowMinimumSize(SDL_Window * window);
extern void Cocoa_SetWindowMaximumSize(SDL_Window * window);
extern void Cocoa_GetWindowSizeInPixels(SDL_Window * window, int *w, int *h);
extern int Cocoa_SetWindowOpacity(SDL_Window * window, float opacity);
extern void Cocoa_ShowWindow(SDL_Window * window);
extern void Cocoa_HideWindow(SDL_Window * window);
extern void Cocoa_RaiseWindow(SDL_Window * window);
extern void Cocoa_MaximizeWindow(SDL_Window * window);
extern void Cocoa_MinimizeWindow(SDL_Window * window);
extern void Cocoa_RestoreWindow(SDL_Window * window);
extern void Cocoa_SetWindowBordered(SDL_Window * window, SDL_bool bordered);
extern void Cocoa_SetWindowResizable(SDL_Window * window, SDL_bool resizable);
extern void Cocoa_SetWindowAlwaysOnTop(SDL_Window * window, SDL_bool on_top);
extern void Cocoa_SetWindowFullscreen(SDL_Window * window, SDL_VideoDisplay * display, SDL_bool fullscreen);
extern int Cocoa_SetWindowGammaRamp(SDL_Window * window, const Uint16 * ramp);
extern void* Cocoa_GetWindowICCProfile(SDL_Window * window, size_t * size);
// extern int Cocoa_GetWindowDisplayIndex(SDL_Window * window);
extern int Cocoa_GetWindowGammaRamp(SDL_Window * window, Uint16 * ramp);
extern void Cocoa_SetWindowMouseRect(SDL_Window * window);
extern void Cocoa_SetWindowMouseGrab(SDL_Window * window, SDL_bool grabbed);
extern void Cocoa_DestroyWindow(SDL_Window * window);
extern SDL_bool Cocoa_GetWindowWMInfo(SDL_Window * window, struct SDL_SysWMinfo *info);
extern int Cocoa_SetWindowHitTest(SDL_Window *window, SDL_bool enabled);
extern void Cocoa_AcceptDragAndDrop(SDL_Window * window, SDL_bool accept);
extern int Cocoa_FlashWindow(SDL_Window * window, SDL_FlashOperation operation);

extern NSView *Cocoa_GetWindowView(SDL_Window * window);

#endif /* SDL_cocoawindow_h_ */

/* vi: set ts=4 sw=4 expandtab: */
