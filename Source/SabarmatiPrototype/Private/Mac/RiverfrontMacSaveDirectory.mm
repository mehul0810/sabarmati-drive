#include "CoreMinimal.h"
#include "Apple/PreAppleSystemHeaders.h"
#import <Foundation/NSArray.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#include "Apple/PostAppleSystemHeaders.h"

FString RiverfrontMacSaveDirectory()
{
    @autoreleasepool
    {
        NSURL* Root = [[[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory
                                                             inDomains:NSUserDomainMask] firstObject];
        if (!Root) return FString();
        NSURL* Folder = [Root URLByAppendingPathComponent:@"Sabarmati Drive/Saved/SaveGames" isDirectory:YES];
        return FString(UTF8_TO_TCHAR([[Folder path] UTF8String]));
    }
}
