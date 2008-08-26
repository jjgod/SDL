#import <Cocoa/Cocoa.h>

@interface SDLMain : NSObject
{
    IBOutlet id fragCount;
    IBOutlet id fullscreen;
    IBOutlet id joinGame;
    IBOutlet id netAddress;
    IBOutlet id numberOfPlayers;
    IBOutlet id playDeathmatch;
    IBOutlet id playerNumber;
    IBOutlet id realtime;
    IBOutlet id window;
    IBOutlet id worldScores;
}
- (IBAction)cancel:(id)sender;
- (IBAction)quit:(id)sender;
- (IBAction)startGame:(id)sender;
- (IBAction)startServer:(id)sender;
- (IBAction)toggleFullscreen:(id)sender;
@end
