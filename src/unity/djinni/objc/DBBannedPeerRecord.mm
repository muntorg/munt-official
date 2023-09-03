// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

#import "DBBannedPeerRecord.h"


@implementation DBBannedPeerRecord

- (nonnull instancetype)initWithAddress:(nonnull NSString *)address
                            bannedUntil:(int64_t)bannedUntil
                             bannedFrom:(int64_t)bannedFrom
                                 reason:(nonnull NSString *)reason
{
    if (self = [super init]) {
        _address = [address copy];
        _bannedUntil = bannedUntil;
        _bannedFrom = bannedFrom;
        _reason = [reason copy];
    }
    return self;
}

+ (nonnull instancetype)bannedPeerRecordWithAddress:(nonnull NSString *)address
                                        bannedUntil:(int64_t)bannedUntil
                                         bannedFrom:(int64_t)bannedFrom
                                             reason:(nonnull NSString *)reason
{
    return [(DBBannedPeerRecord*)[self alloc] initWithAddress:address
                                                  bannedUntil:bannedUntil
                                                   bannedFrom:bannedFrom
                                                       reason:reason];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@ %p address:%@ bannedUntil:%@ bannedFrom:%@ reason:%@>", self.class, (void *)self, self.address, @(self.bannedUntil), @(self.bannedFrom), self.reason];
}

@end