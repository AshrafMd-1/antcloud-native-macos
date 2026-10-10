// AntCloud Native additions, GPL-3.0-or-later, 2026.
#include "appkit_stats_test.h"
#include "streaming/video/ffmpeg-renderers/antcloudstatstextfield.h"
#include "streaming/video/antcloudstats.h"

#import <Cocoa/Cocoa.h>

#include <limits>

bool runAppKitStatsTextFieldFixture(QString* failure)
{
    @autoreleasepool {
        AntCloudStatsTextField* field = [[AntCloudStatsTextField alloc]
            initWithFrame:NSMakeRect(0, 0, 1200, 40)];
        [field setBezeled:NO];
        [field setDrawsBackground:NO];
        [field setEditable:NO];
        [field setSelectable:NO];
        [field configureWithFontSize:28.0];

        char text[200];
        AntCloudStats::format({32, 120, 3, 60, 10, 7212}, text, sizeof(text));
        NSString* stats = [NSString stringWithUTF8String:text];
        [field setStringValue:stats];
        const CGFloat wideFontSize = field.font.pointSize;
        if (![field.stringValue isEqualToString:stats] || wideFontSize != 28.0 ||
            ((NSTextFieldCell*)field.cell).wraps) {
            if (failure) *failure = QStringLiteral("wide statistics field changed or wrapped its text");
            [field release];
            return false;
        }

        [field setFrameSize:NSMakeSize(600, 40)];
        const CGFloat narrowFontSize = field.font.pointSize;
        const CGFloat narrowTextWidth = [field.stringValue sizeWithAttributes:@{NSFontAttributeName: field.font}].width;
        if (![field.stringValue isEqualToString:stats] || narrowFontSize >= wideFontSize ||
            narrowTextWidth > field.bounds.size.width - 4.0) {
            if (failure) *failure = QStringLiteral("narrow statistics field did not fit the complete line");
            [field release];
            return false;
        }

        AntCloudStats::format({9, 120, 0, 60, 2, -1}, text, sizeof(text));
        NSString* updated = [NSString stringWithUTF8String:text];
        [field setStringValue:updated];
        if (![field.stringValue isEqualToString:updated] || field.font.pointSize != narrowFontSize) {
            if (failure) *failure = QStringLiteral("statistics update changed text integrity or caused font fidgeting");
            [field release];
            return false;
        }

        AntCloudStats::format({9, 120, 0, 60, 2, std::numeric_limits<int>::max()}, text, sizeof(text));
        NSString* maximum = [NSString stringWithUTF8String:text];
        [field setStringValue:maximum];
        if (![field.stringValue isEqualToString:maximum]) {
            if (failure) *failure = QStringLiteral("statistics field did not retain maximum provider time");
            [field release];
            return false;
        }

        [field setFrameSize:NSMakeSize(1200, 40)];
        if (field.font.pointSize != wideFontSize) {
            if (failure) *failure = QStringLiteral("statistics field did not restore its preferred size");
            [field release];
            return false;
        }

        [field release];
        return true;
    }
}
