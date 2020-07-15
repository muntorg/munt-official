// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

#import "DBAccountRecord.h"
#import <Foundation/Foundation.h>
@protocol DBIAccountsListener;


/** C++ interface to control accounts */
@interface DBIAccountsController : NSObject

/** Register listener to be notified of account related events */
+ (void)setListener:(nullable id<DBIAccountsListener>)accountslistener;

/** Set the currently active account */
+ (BOOL)setActiveAccount:(nonnull NSString *)accountUUID;

/** Delete an account, account remains available in background but is hidden from user */
+ (BOOL)deleteAccount:(nonnull NSString *)accountUUID;

/** Create an account, possible types are (HD/Mobile/Witness/Mining/Legacy). Returns the UUID of the new account */
+ (nonnull NSString *)createAccount:(nonnull NSString *)accountName
                        accountType:(nonnull NSString *)accountType;

/** Rename an account */
+ (BOOL)renameAccount:(nonnull NSString *)accountUUID
       newAccountName:(nonnull NSString *)newAccountName;

/**
 * Purge an account, account is permenently removed from wallet (but may still reappear in some instances if it is an HD account and user recovers from phrase in future)
 * If it is a Legacy or imported witness key or similar account then it will be gone forever
 * Generally prefer 'deleteAccount' and use this with caution
 */
+ (BOOL)purgeAccount:(nonnull NSString *)accountUUID;

/** List all currently visible accounts in the walley */
+ (nonnull NSArray<DBAccountRecord *> *)listAccounts;

@end
