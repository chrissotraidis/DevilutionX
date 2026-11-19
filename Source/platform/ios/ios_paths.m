#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "ios_paths.h"

#ifdef __cplusplus
extern "C" {
#endif

int IOSDeviceIsIPad()
{
    return UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad;
}

char *IOSGetPrefPath()
{
	@autoreleasepool {
		NSArray *array = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);

		if ([array count] > 0) {
			NSString *str = [array objectAtIndex:0];
			str = [str stringByAppendingString:@"/"];
			const char *base = [str fileSystemRepresentation];

			char *copy = malloc(strlen(base) + 1);
			strcpy(copy, base);
			return copy;
		}

		return "";
	}
}

#ifdef __cplusplus
}
#endif
