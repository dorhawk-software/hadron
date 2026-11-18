/* A window left of or above the main display has negative screen coordinates. Wine's server
 * scaled them with unsigned arithmetic (scale_dpi in server/user.h), so a point at -210 became
 * 44739032: the server's hit test found no window under a click, and another process read the
 * window's rect as garbage. Enterprise Architect on a monitor placed above a MacBook's display
 * took no clicks at all (docs/findings.md, "Clicks on a display left of or above the main one").
 *
 * A popup is put inside each display left of or above the main one, and one on the main display
 * as the control. It needs such a display: a window off every display is cropped away (Wine
 * crops a window to the virtual screen), so nothing could click it there anyway.
 *
 *   negative-coords.exe            PASS or FAIL, exit status 0 or 1; 2 with no display left of
 *                                  or above the main one, when nothing was tested
 *
 * Build: toolchains/llvm-mingw/bin/x86_64-w64-mingw32-clang -Os -o negative-coords.exe tests/win/negative-coords.c -luser32
 */
#include <windows.h>
#include <stdio.h>

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    return DefWindowProcA( hwnd, msg, wp, lp );
}

static int check( const char *what, LONG x, LONG y )
{
    WNDCLASSA wc = { .lpfnWndProc = proc, .hInstance = GetModuleHandleA( NULL ), .lpszClassName = "negative-coords" };
    HWND hwnd, found;
    POINT centre = { x + 100, y + 75 };
    RECT rect;
    int ok = 1;

    RegisterClassA( &wc );
    hwnd = CreateWindowExA( WS_EX_TOOLWINDOW, wc.lpszClassName, what, WS_POPUP, x, y, 200, 150,
                            NULL, NULL, wc.hInstance, NULL );
    SetWindowPos( hwnd, HWND_TOPMOST, x, y, 200, 150, SWP_NOACTIVATE | SWP_SHOWWINDOW );
    GetWindowRect( hwnd, &rect );
    if (rect.left != x || rect.top != y || rect.right != x + 200 || rect.bottom != y + 150)
    {
        printf( "FAIL: %s: the window's rect is %ld,%ld to %ld,%ld, not %ld,%ld to %ld,%ld\n", what,
                rect.left, rect.top, rect.right, rect.bottom, x, y, x + 200, y + 150 );
        ok = 0;
    }
    found = WindowFromPoint( centre );
    if (found != hwnd)
    {
        printf( "FAIL: %s: the window at %ld,%ld is %p, not the window there (%p)\n", what,
                centre.x, centre.y, found, hwnd );
        ok = 0;
    }
    if (ok) printf( "ok: %s\n", what );
    DestroyWindow( hwnd );
    return ok;
}

struct displays
{
    RECT rects[16];
    int count;
};

static BOOL CALLBACK add_display( HMONITOR monitor, HDC hdc, RECT *rect, LPARAM lp )
{
    struct displays *d = (struct displays *)lp;
    if (d->count < 16) d->rects[d->count++] = *rect;
    return TRUE;
}

int main( void )
{
    struct displays d = { 0 };
    char what[96];
    int i, ok = 1, tested = 0;

    EnumDisplayMonitors( NULL, NULL, add_display, (LPARAM)&d );
    for (i = 0; i < d.count; i++)
    {
        RECT *r = &d.rects[i];
        if (r->left >= 0 && r->top >= 0) continue;
        snprintf( what, sizeof(what), "the display at %ld,%ld to %ld,%ld", r->left, r->top, r->right, r->bottom );
        ok &= check( what, r->left + 50, r->top + 50 );
        tested++;
    }
    if (!tested)
    {
        printf( "no display left of or above the main one: nothing tested\n" );
        return 2;
    }
    ok &= check( "the main display (the control)", 200, 200 );
    printf( "%s\n", ok ? "PASS" : "FAIL" );
    return ok ? 0 : 1;
}
