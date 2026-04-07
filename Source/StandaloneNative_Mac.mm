// Cocoa-only translation unit — never include JuceHeader.h here.
// juce::Component typedef conflicts with Carbon/Cocoa's ComponentRecord* Component.
#import <Cocoa/Cocoa.h>

// Called from Standalone.mm after setVisible() so the NSWindow exists.
// Sets the window opaque with a solid kBg background, killing macOS's
// compositor gradient that it paints over custom JUCE title bars.
void nikaForceOpaqueWindow (void* nsViewHandle)
{
    NSView* view = (__bridge NSView*) nsViewHandle;
    if (view == nil) return;

    NSWindow* w = view.window;
    if (w == nil) return;

    [w setOpaque: YES];
    [w setBackgroundColor: [NSColor blackColor]];
    [w setTitlebarAppearsTransparent: YES];
}
