#import "InstanceSend.hpp"
#import "InstanceSendMac.h"

@implementation DownloaderMessageHandlerMac

-(instancetype) init
{
	self = [super init];
	return self;
}
-(void)add_observer:(NSString *)version_hash
{
	//NSLog(@"adding observer");
	//NSString *nsver = @"OtherPrusaSlicerInstanceMessage" + version_hash;
	NSString *nsver = [NSString stringWithFormat: @"%@%@", @"OtherDownloaderInstanceMessage", version_hash];
	[[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(message_update:) name:nsver object:nil suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
}

-(void)message_update:(NSNotification *)msg
{
	[self bring_forward];
	//pass message  
	//Slic3r::GUI::wxGetApp().other_instance_message_handler()->handle_message(std::string([msg.userInfo[@"data"] UTF8String]));\
	// send event instead?
}

-(void) bring_forward
{
	//demiaturize all windows
	for(NSWindow* win in [NSApp windows])
	{
		if([win isMiniaturized])
		{
			[win deminiaturize:self];
		}
	}
	//bring window to front 
	[[NSApplication sharedApplication] activateIgnoringOtherApps : YES];
}

@end

namespace Downloader {

void send_message_mac(const std::string &msg, const std::string &version)
{
	NSString *nsmsg = [NSString stringWithCString:msg.c_str() encoding:[NSString defaultCStringEncoding]];
	//NSString *nsver = @"OtherPrusaSlicerInstanceMessage" + [NSString stringWithCString:version.c_str() encoding:[NSString defaultCStringEncoding]];
	NSString *nsver = [NSString stringWithCString:version.c_str() encoding:[NSString defaultCStringEncoding]];
	NSString *notifname = [NSString stringWithFormat: @"%@%@", @"OtherDownloaderInstanceMessage", nsver];
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:notifname object:nil userInfo:[NSDictionary dictionaryWithObject:nsmsg forKey:@"data"] deliverImmediately:YES];
}

void OtherDownloaderMessageHandler::register_for_messages(const std::string &version_hash)
{
	m_impl_osx = [[DownloaderMessageHandlerMac alloc] init];
	if(m_impl_osx) {
		NSString *nsver = [NSString stringWithCString:version_hash.c_str() encoding:[NSString defaultCStringEncoding]];
		[(id)m_impl_osx add_observer:nsver];
	}
}

void OtherDownloaderMessageHandler::unregister_for_messages()
{
	//NSLog(@"unreegistering other instance messages");
	if (m_impl_osx) {
        [(id)m_impl_osx release];
        m_impl_osx = nullptr;
    } else {
		NSLog(@"warning: unregister instance InstanceSend notifications not required");
	}
}

void OtherDownloaderMessageHandler::bring_instance_forward()
{
	if (m_impl_osx) {
		[(id)m_impl_osx bring_forward];
	}
}
}//namespace Downloader


