// AntCloud Native additions, GPL-3.0-or-later, 2026.
#pragma once

#import <Cocoa/Cocoa.h>

#include <cmath>

// NSTextField wraps by default. The native statistics overlay needs to retain
// its complete one-line value as the stream view changes size, including long
// provider-time values. This field keeps the configured size when it fits and
// scales down only as far as the current width requires.
@interface AntCloudStatsTextField : NSTextField {
    CGFloat m_PreferredFontSize;
    CGFloat m_FittedFontSize;
    CGFloat m_LastFitWidth;
    BOOL m_FittingText;
}

- (void)configureWithFontSize:(CGFloat)fontSize;
- (void)fitStatsText;

@end

@implementation AntCloudStatsTextField

- (void)configureWithFontSize:(CGFloat)fontSize
{
    m_PreferredFontSize = fontSize;
    m_FittedFontSize = fontSize;
    m_LastFitWidth = 0;
    NSTextFieldCell* cell = (NSTextFieldCell*)self.cell;
    cell.usesSingleLineMode = YES;
    cell.wraps = NO;
    cell.scrollable = NO;
    cell.lineBreakMode = NSLineBreakByClipping;
    [self fitStatsText];
}

- (void)setStringValue:(NSString*)stringValue
{
    [super setStringValue:stringValue];
    [self fitStatsText];
}

- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
    [self fitStatsText];
}

- (void)fitStatsText
{
    if (m_FittingText || m_PreferredFontSize <= 0 || self.bounds.size.width <= 0) {
        return;
    }

    m_FittingText = YES;
    const CGFloat currentWidth = self.bounds.size.width;
    const CGFloat availableWidth = MAX(1.0, currentWidth - 4.0);
    NSFont* preferredFont = [NSFont messageFontOfSize:m_PreferredFontSize];
    const CGFloat preferredTextWidth =
        [self.stringValue sizeWithAttributes:@{NSFontAttributeName: preferredFont}].width;
    CGFloat fittedSize = m_PreferredFontSize;
    if (preferredTextWidth > availableWidth) {
        fittedSize = MAX(6.0, std::floor((m_PreferredFontSize * availableWidth / preferredTextWidth) * 2.0) / 2.0);
    }

    // Text changes every second. At a fixed width it may shrink for a longer
    // sample, but it does not repeatedly grow and shrink as values fluctuate.
    // A real width change recomputes from the preferred size.
    if (std::fabs(currentWidth - m_LastFitWidth) < 0.5) {
        fittedSize = MIN(m_FittedFontSize, fittedSize);
    }
    m_LastFitWidth = currentWidth;
    m_FittedFontSize = fittedSize;

    [super setFont:[NSFont messageFontOfSize:fittedSize]];
    m_FittingText = NO;
}

@end
