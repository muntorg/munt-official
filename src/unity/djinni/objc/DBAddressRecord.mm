// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

#import "DBAddressRecord.h"


@implementation DBAddressRecord

- (nonnull instancetype)initWithAddress:(nonnull NSString *)address
                                   name:(nonnull NSString *)name
                            description:(nonnull NSString *)description
                                purpose:(nonnull NSString *)purpose
{
    if (self = [super init]) {
        _address = [address copy];
        _name = [name copy];
        _description = [description copy];
        _purpose = [purpose copy];
    }
    return self;
}

+ (nonnull instancetype)addressRecordWithAddress:(nonnull NSString *)address
                                            name:(nonnull NSString *)name
                                     description:(nonnull NSString *)description
                                         purpose:(nonnull NSString *)purpose
{
    return [(DBAddressRecord*)[self alloc] initWithAddress:address
                                                      name:name
                                               description:description
                                                   purpose:purpose];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@ %p address:%@ name:%@ description:%@ purpose:%@>", self.class, (void *)self, self.address, self.name, self.description, self.purpose];
}

@end
