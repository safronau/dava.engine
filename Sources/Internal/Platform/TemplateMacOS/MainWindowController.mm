/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/


#import "MainWindowController.h"
#include "CorePlatformMacOS.h"
#include "Platform/DeviceInfo.h"
#include "Render/2D/Systems/RenderSystem2D.h"

extern void FrameworkDidLaunched();
extern void FrameworkWillTerminate();


namespace DAVA 
{
	int Core::Run(int argc, char *argv[], AppHandle handle)
	{
		NSAutoreleasePool * globalPool = 0;
		globalPool = [[NSAutoreleasePool alloc] init];
		DAVA::CoreMacOSPlatform * core = new DAVA::CoreMacOSPlatform();
		core->SetCommandLine(argc, argv);
		core->CreateSingletons();

		[[NSApplication sharedApplication] setDelegate:(id<NSApplicationDelegate>)[[[MainWindowController alloc] init] autorelease]];
		
		int retVal = NSApplicationMain(argc,  (const char **) argv);
        // This method never returns, so release code transfered to termination message 
        // - (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
        // core->ReleaseSingletons() is called from there
        
		[globalPool release];
		globalPool = 0;
		return retVal;
	}
	
	int Core::RunCmdTool(int argc, char *argv[], AppHandle handle)
	{
		NSAutoreleasePool * globalPool = 0;
		globalPool = [[NSAutoreleasePool alloc] init];
		DAVA::CoreMacOSPlatform * core = new DAVA::CoreMacOSPlatform();
		core->SetCommandLine(argc, argv);
		core->EnableConsoleMode();
		core->CreateSingletons();
		
        Logger::Instance()->EnableConsoleMode();
        
		FrameworkDidLaunched();
		FrameworkWillTerminate();
		
		core->ReleaseSingletons();

		[globalPool release];
		globalPool = 0;
		return 0;
	}
}

@interface MainWindowController ()
- (BOOL) isAnimating;
- (void) startAnimation;
- (void) stopAnimation;
- (void) toggleAnimation;

- (void) startAnimationTimer;
- (void) stopAnimationTimer;
- (void) animationTimerFired:(NSTimer *)timer;

- (void)windowWillMiniaturize:(NSNotification *)notification;
- (void)windowDidMiniaturize:(NSNotification *)notification;
- (void)windowDidDeminiaturize:(NSNotification *)notification;
@end

@implementation MainWindowController


static MainWindowController * mainWindowController = nil;

/* This code disabled for now and left for the future
 */
namespace DAVA 
{
	Vector2 CoreMacOSPlatform::GetMousePosition()
	{
		NSPoint p = [mainWindowController->mainWindow mouseLocationOutsideOfEventStream]; //[NSEvent locationInWindow]; 
		p = [mainWindowController->openGLView convertPointFromBacking: p];

        Vector2 mouseLocation;
		mouseLocation.x = p.x;
		mouseLocation.y = VirtualCoordinatesSystem::Instance()->GetPhysicalScreenSize().dy - p.y;
		// mouseLocation.y = 
		return mouseLocation;
	}
	
	void* CoreMacOSPlatform::GetOpenGLView()
	{
		return mainWindowController->openGLView;
	}
}

- (id)init
{
	if (self = [super init])
	{
		mainWindowController = self;
		openGLView = nil;
		mainWindow = nil;
		isAnimating = false;
		animationTimer = nil;
		timeBefore = 0;
		core = 0;

	}
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

-(void)createWindows
{
    NSRect fullscreenRect = [[NSScreen mainScreen] frame];
	
	FrameworkDidLaunched();
    RenderManager::Create(Core::RENDERER_OPENGL_ES_2_0);
    	KeyedArchive * options = DAVA::Core::Instance()->GetOptions();
    
	int32 width = options->GetInt32("width", 800);
    int32 height = width * fullscreenRect.size.height / fullscreenRect.size.width;
	
    openGLView = [[OpenGLView alloc]initWithFrame: NSMakeRect(0, 0, width, height)];
    
    String title = options->GetString("title", "[set application title using core options property 'title']");
    
    NSUInteger wStyle = NSTitledWindowMask + NSMiniaturizableWindowMask + NSClosableWindowMask + NSResizableWindowMask;
    NSRect wRect = NSMakeRect((fullscreenRect.size.width - width) / 2, (fullscreenRect.size.height - height) / 2, width, height);
    mainWindow = [[NSWindow alloc] initWithContentRect:wRect styleMask:wStyle backing:NSBackingStoreBuffered defer:FALSE];
    [mainWindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
    [mainWindow setDelegate:self];
    [mainWindow setContentView: openGLView];
    
    NSRect rect;
    rect.origin.x = 0;
    rect.origin.y = 0;
    rect.size.width = width;
    rect.size.height = height;
    
    [mainWindow setContentSize:rect.size];
    [openGLView setFrame: rect];
    
	core = Core::GetApplicationCore();
    RenderManager::Instance()->DetectRenderingCapabilities();
    RenderSystem2D::Instance()->Init();

	// start animation
	isAnimating = NO;
	
	currFPS = RenderManager::Instance()->GetFPS();
    [self startAnimation];

	// make window main
	[mainWindow makeKeyAndOrderFront:nil];
	[mainWindow setTitle:[NSString stringWithFormat:@"%s", title.c_str()]];
	[mainWindow setAcceptsMouseMovedEvents:YES];
}

- (void)windowWillMiniaturize:(NSNotification *)notification
{
}

- (void)windowDidMiniaturize:(NSNotification *)notification
{
    [self OnSuspend];
}

- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    [self OnResume];
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    fullScreen = true;
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    fullScreen = false;
}

-(bool) getFullScreen
{
    return fullScreen;
}

- (void) setFullScreen:(bool)_fullScreen
{
    if(fullScreen != _fullScreen)
    {
        if(_fullScreen)
        {
            if(floor(NSAppKitVersionNumber) >= NSAppKitVersionNumber10_7)
            {
                [mainWindowController->mainWindow toggleFullScreen: nil];
            }
            else
            {
                [openGLView enterFullScreenMode:[NSScreen mainScreen] withOptions:nil];
                fullScreen = true;
            }
        }
        else
        {
            if(floor(NSAppKitVersionNumber) >= NSAppKitVersionNumber10_7)
            {
                [mainWindowController->mainWindow toggleFullScreen: nil];
            }
            else
            {
                [openGLView exitFullScreenModeWithOptions:nil];
                fullScreen = false;
            }
        }
    }
}

- (void) keyDown:(NSEvent *)event
{
	[openGLView keyDown:event];
}

- (void) keyUp:(NSEvent *)event
{
	[openGLView keyUp:event];
}

- (void) flagsChanged :(NSEvent *)event
{
	[openGLView flagsChanged:event];
}

- (void)mouseDown:(NSEvent *)theEvent
{
	[openGLView mouseDown:theEvent];
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    [openGLView scrollWheel:theEvent];
}

- (void)mouseMoved:(NSEvent *)theEvent
{
	[openGLView mouseMoved:theEvent];
}

- (void)mouseUp:(NSEvent *)theEvent
{
	[openGLView mouseUp:theEvent];
}

- (void)mouseDragged:(NSEvent *)theEvent
{
	[openGLView mouseDragged:theEvent];
}

- (void)mouseEntered:(NSEvent *)theEvent
{
	NSLog(@"mouse ENTERED");
}
- (void)mouseExited:(NSEvent *)theEvent
{
	NSLog(@"mouse EXITED");
}
- (void)rightMouseDown:(NSEvent *)theEvent
{
	[openGLView rightMouseDown:theEvent];
}

- (void)rightMouseDragged:(NSEvent *)theEvent
{
	[openGLView rightMouseDragged:theEvent];
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
	[openGLView rightMouseUp:theEvent];
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
	[openGLView otherMouseDown:theEvent];
}

- (void)otherMouseDragged:(NSEvent *)theEvent
{
	[openGLView otherMouseDragged:theEvent];
}

- (void)otherMouseUp:(NSEvent *)theEvent
{
	[openGLView otherMouseUp:theEvent];
}

- (BOOL) isAnimating
{
    return isAnimating;
}

- (void) startAnimation
{
    if (!isAnimating) 
	{
        isAnimating = YES;
        if (![self getFullScreen])
		{
            [self startAnimationTimer];
        }
    }
}

- (void) stopAnimation
{
    if (isAnimating) 
	{
        if (animationTimer != nil) 
		{
            [self stopAnimationTimer];
        }
        isAnimating = NO;
    }
}

- (void) toggleAnimation
{
    if ([self isAnimating]) 
	{
        [self stopAnimation];
    } else {
        [self startAnimation];
    }
}

- (void) startAnimationTimer
{
    if (animationTimer == nil) 
	{
        animationTimer = [[NSTimer scheduledTimerWithTimeInterval:1.0f / currFPS target:self selector:@selector(animationTimerFired:) userInfo:nil repeats:YES] retain];
    }
}

- (void) stopAnimationTimer
{
    if (animationTimer != nil) 
	{
        [animationTimer invalidate];
        [animationTimer release];
        animationTimer = nil;
    }
}

- (void) animationTimerFired:(NSTimer *)timer
{
//	NSLog(@"anim timer fired: %@", openGLView);
    [openGLView setNeedsDisplay:YES];
	if (currFPS != RenderManager::Instance()->GetFPS())
	{
		currFPS = RenderManager::Instance()->GetFPS();
		[self stopAnimationTimer];
		[self startAnimationTimer];
	}
}

- (void) switchFullscreenTimerFired:(NSTimer *)timer
{
	//[openGLView disableTrackingArea];
	//[self switchToFullScreen];
	//[timer invalidate];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	NSLog(@"[CoreMacOSPlatform] Application did finish launching");	
    
    [self OnResume];
    
    DAVA::Cursor * activeCursor = RenderManager::Instance()->GetCursor();
    if (activeCursor)
    {
        NSCursor * cursor = (NSCursor*)activeCursor->GetMacOSXCursor();
        [cursor set];
    }
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification
{
//	[NSMenu setMenuBarVisible: NO];
//	[NSMenu setMenuBarVisible: YES];
	[self createWindows];
	NSLog(@"[CoreMacOSPlatform] Application will finish launching: %@", [[NSBundle mainBundle] bundlePath]);
	Core::Instance()->SystemAppStarted();
}

- (void)applicationWillBecomeActive:(NSNotification *)aNotification
{
	NSLog(@"[CoreMacOSPlatform] Application will become active");
}

- (void)applicationDidBecomeActive:(NSNotification *)aNotification
{
	NSLog(@"[CoreMacOSPlatform] Application did become active");

    [self OnResume];
}

- (void)applicationDidResignActive:(NSNotification *)aNotification
{
	NSLog(@"[CoreMacOSPlatform] Application did resign active");

    [self OnSuspend];
}

- (void)applicationDidChangeScreenParameters:(NSNotification *)aNotification
{
	NSLog(@"[CoreMacOSPlatform] Application did change screen params");
}

- (void)applicationDidHide:(NSNotification *)aNotification
{
	NSLog(@"[CoreMacOSPlatform] Application did hide");
}

- (void)applicationDidUnhide:(NSNotification *)aNotification
{
	NSLog(@"[CoreMacOSPlatform] Application did unhide");
}

- (void)windowWillClose:(NSNotification *)notification
{
	[[NSApplication sharedApplication] terminate: nil];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    mainWindowController->openGLView.willQuit = true;
    
	Core::Instance()->SystemAppFinished();
	FrameworkWillTerminate();
    Core::Instance()->ReleaseSingletons();

	NSLog(@"[CoreMacOSPlatform] Application terminate");
	return NSTerminateNow;
}


- (void)OnSuspend
{
    if(core)
    {
        core->OnSuspend();
    }
    else 
    {
        Core::Instance()->SetIsActive(false);
    }
}

- (void)OnResume
{
    if(core)
    {
        core->OnResume();
    }
    else 
    {
        Core::Instance()->SetIsActive(true);
    }
}




@end

namespace DAVA 
{

Core::eScreenMode CoreMacOSPlatform::GetScreenMode()
{
    return ([mainWindowController getFullScreen]) ? Core::MODE_FULLSCREEN : Core::MODE_WINDOWED;
}

void CoreMacOSPlatform::ToggleFullscreen()
{
    if (GetScreenMode() == Core::MODE_FULLSCREEN) // check if we try to switch mode
    {
        [mainWindowController setFullScreen:false];
    }
    else
    {
        [mainWindowController setFullScreen:true];
    }
}

void CoreMacOSPlatform::SwitchScreenToMode(eScreenMode screenMode)
{
    if (screenMode == Core::MODE_FULLSCREEN)
    {
        [mainWindowController setFullScreen:true];
    }
    else if (screenMode == Core::MODE_WINDOWED)
    {
        [mainWindowController setFullScreen:false];
    }
}

void CoreMacOSPlatform::Quit()
{
	mainWindowController->openGLView.willQuit = true;
	[[NSApplication sharedApplication] terminate: nil];
}
	
};
