//
//  GuldenCoreUtil.m
//  GuldenCore
//
//  Created by Willem de Jonge on 03/04/2019.
//  Copyright © 2019 Gulden.com BV. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "GuldenCore/DBGuldenUnifiedBackend.h"

NSString* GuldenCoreStaticFilterPath()
{
    NSBundle* fwBundle = [NSBundle bundleForClass:[DBGuldenUnifiedBackend class]];
    NSString* filterPath = [fwBundle pathForResource:@"staticfiltercp" ofType:NULL];
    return filterPath;
}
