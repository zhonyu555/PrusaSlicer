#import <Cocoa/Cocoa.h>

@interface DownloaderMessageHandlerMac : NSObject

-(instancetype) init;
-(void) add_observer:(NSString *)version;
-(void) message_update:(NSNotification *)note;
-(void) bring_forward;
@end
