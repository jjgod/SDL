//   SDLMain.m (for Maelstrom) - main entry point for our Cocoa-ized SDL app 
//   By Darrell Walisser - dwaliss1@purdue.edu

#include "SDL.h"
#import "SDLMain.h"
#import <sys/param.h> // for MAXPATHLEN
#import <sys/wait.h>  // for waitpid
#import <unistd.h>
#import <pthread.h>   // for tweaking thread scheduling

#define THREAD_MAIN 0

static int    gArgc = 0;
static char  *gArgv[20];
static int      pid = -1;

#define ADD_ARG(x)                                \
    {                                             \
        assert (gArgc < 20);                      \
        gArgv[gArgc] = strdup (x);                \
        gArgc++;                                  \
    }
        
void cleanup () {
    if (pid != -1)
        kill (pid, SIGTERM);
}

@implementation SDLMain

- (IBAction)cancel:(id)sender
{
    [ NSApp abortModal ];
    [ window close ];
    exit (0);
}

- (IBAction)startGame:(id)sender
{
    // extract settings, add them to arguments array
    if ( [ fullscreen intValue ] == 1 ) {
        ADD_ARG("-fullscreen");
    }
    
    // enable realtime scheduling to get more CPU time.
    if ( [ realtime intValue ] == 1 ) {
        int policy;
        struct sched_param param;
        pthread_t thread = pthread_self ();
        pthread_getschedparam (thread, &policy, &param);
        policy = SCHED_RR;
        param.sched_priority = 47;
        pthread_setschedparam (thread, policy, &param);
        pthread_getschedparam (thread, &policy, &param);
    }
    
    if ( [ worldScores intValue ] == 1 ) {
        ADD_ARG("-netscores");
    }
    
    if ( [ joinGame intValue ] == 1 ) {
        char *storage[1024];
        char *buffer = (char*)storage;
        
        ADD_ARG("-player");
        sprintf (buffer, "%d", [ playerNumber intValue ] );
        ADD_ARG(buffer);
        ADD_ARG("-server");
        sprintf (buffer, "%d@%s", [ numberOfPlayers intValue ],
            [ [ netAddress stringValue ] cString ]);
        ADD_ARG(buffer);
        
        if ( [ playDeathmatch intValue ] == 1 ) {
            ADD_ARG("-deathmatch");
            sprintf (buffer, "%d", [ fragCount intValue ] );
            ADD_ARG(buffer);
        }
    }
    
    [ NSApp abortModal ];
    [ window close ];
}

- (IBAction)startServer:(id)sender
{
    static int started = 0;
    
    if ( ! started ) {
        int newPid = fork ();
        if ( newPid == 0 ) {
            char *args[2] = { "Maelstrom_Server", NULL };
            char path[MAXPATHLEN];
            getcwd (path, MAXPATHLEN);
            strcat (path, "/Maelstrom_Server");
            execvp (path, args);
            fprintf (stderr, "could not start server\n");
        }
        else {
            pid = newPid;
            if ( waitpid (pid, NULL, WNOHANG) == 0 ) {
                started = 1;
                [ sender setTitle:@"Stop Server" ];
            }
        }
    }
    else {
        if ( kill (pid, SIGTERM) == 0) {
            started = 0;
            [ sender setTitle:@"Start Server" ];
            pid = -1;
        }
    }
}

- (void) quit:(id)sender
{
    	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

- (void) toggleFullscreen:(id)sender
{
    
}

- (void)threadMain:(id)sender
{
    SDL_main (gArgc, gArgv);
    exit(0);
}

// Called when the Cocoa app is finished initializing and
// Cocoa event loop has just begun, so call SDL_main here

- (void) applicationDidFinishLaunching: (NSNotification *) note
{
    char parentdir[MAXPATHLEN];
    char *c;
        
    strcpy ( parentdir, gArgv[0] );
    
    c = (char*) parentdir;
    while (*c != '\0')     // go to end
        c++;
    while (*c != '/')      // back up to parent
        c--;
    
    *c = '\0';             // cut off last part (binary name)
            
    assert ( chdir (parentdir) == 0 );   // chdir to the binary app's parent
#if DEBUG        
    assert ( chdir ("../../../") == 0 );    // chdir to the .app's parent
#else
    assert ( chdir ("../Resources") == 0 ); // store game data in .app bundle Resources
#endif    
    
    assert ( getcwd(parentdir, MAXPATHLEN) == parentdir );
    strcat (parentdir, "/Maelstrom.app");
    gArgv[0] = strdup (parentdir);
    gArgc = 1;
    
    atexit (cleanup);
    
    [ NSApp runModalForWindow:window ];

#if THREAD_MAIN
// broken, some functions are not thread safe in Cocoa/Quartz calls
    [ NSThread detachNewThreadSelector:@selector(threadMain:) toTarget:self withObject:nil ];
#else
    SDL_main (gArgc, gArgv);
    exit(0);
#endif
}
@end

#ifdef main
#undef main
#endif

// main entry point to executible - should *not* be SDL_main!
int main (int argc, char **argv) {
    int i;
    for (i = 0; i < argc; i++)
        ADD_ARG (argv[i]);
        
    NSApplicationMain (argc, argv);
    return 0;
}