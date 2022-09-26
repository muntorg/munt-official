//
//  MuntCoreUtil.m
//  MuntCore
//
//  Created by Willem de Jonge on 03/04/2019.
//  Copyright © 2019-2022 Centure.com BV. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "DBILibraryController.h"

NSString* MuntCoreStaticFilterPath(BOOL testnet)
{
    NSBundle* fwBundle = [NSBundle bundleForClass:[DBILibraryController class]];
    NSString* filterFile = @"staticfiltercp";
    if (testnet)
        filterFile = [filterFile stringByAppendingString:@"testnet"];
    NSString* filterPath = [fwBundle pathForResource:filterFile ofType:NULL];
    return filterPath;
}

NSString* MuntCoreLicensePath(void)
{
    NSBundle* fwBundle = [NSBundle bundleForClass:[DBILibraryController class]];
    NSString* filePath = [fwBundle pathForResource:@"core-packages.licenses" ofType:NULL];
    return filePath;
}
