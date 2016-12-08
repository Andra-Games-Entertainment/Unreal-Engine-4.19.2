// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#import <Cocoa/Cocoa.h>

@interface FUE4EditorServicesAppDelegate : NSObject <NSApplicationDelegate, NSFileManagerDelegate>
{
	NSWindow* Window;
}

- (IBAction)onCancelButtonPressed:(id)Sender;
- (IBAction)onOKButtonPressed:(id)Sender;

- (void)launchGameService:(NSPasteboard *)PBoard userData:(NSString *)UserData error:(NSString **)Error;
- (void)generateXcodeProjectService:(NSPasteboard *)PBoard userData:(NSString *)UserData error:(NSString **)Error;
- (void)switchUnrealEngineVersionService:(NSPasteboard *)PBoard userData:(NSString *)UserData error:(NSString **)Error;

@end
