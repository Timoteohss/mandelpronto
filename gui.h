#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0



int setup(int width, int height,
        Display **display, Window *win, GC *gc,
        long *min_color, long *max_color) 
{
    int x = 0, y = 0;
    int border_width = 4;
    int disp_width, disp_height;
    int screen;

    char *window_name = "Mandelbrot", *disp_name = NULL;
    long valuemask = 0;
    XGCValues values;

    long white, black;

    XEvent report;

    if ( ( *display = XOpenDisplay (disp_name) ) == NULL )
    {
        fprintf(stderr, "Cannot connect to X server %s\n",
                XDisplayName(disp_name));
                return EXIT_FAILURE;
    }

    screen = DefaultScreen (*display);
    disp_width = DisplayWidth (*display, screen);
    disp_height = DisplayHeight (*display, screen);
    *win = XCreateSimpleWindow (*display, RootWindow (*display, screen),
            x, y, width, height, border_width,
            BlackPixel (*display, screen),
            WhitePixel (*display, screen));
    XStoreName(*display, *win, window_name);
    *gc = XCreateGC (*display, *win, valuemask, &values);
    white = WhitePixel (*display, screen);
    black = BlackPixel (*display, screen);
    XSetBackground (*display, *gc, white);
    XSetForeground (*display, *gc, black);
    XMapWindow (*display, *win);
    XSync (*display, False);

    *min_color = (white < black) ? black : white;
    *max_color = (black < white) ? black : white;

    fprintf(stderr, "Press any key to start\n");
    fflush(stderr);

    XSelectInput(*display, *win, KeyPressMask);
    XNextEvent(*display,&report);

    return EXIT_SUCCESS;
}