// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

#import "DBBalanceRecord.h"
#import "DBMutationRecord.h"
#import "DBTransactionRecord.h"
#import <Foundation/Foundation.h>


/** Interface to receive wallet level events */
@protocol DBIWalletListener

- (void)notifyBalanceChange:(nonnull DBBalanceRecord *)newBalance;

/**
 * Notification of new mutations.
 * If selfCommitted it is due to a call to performPaymentToRecipient, else it is because of a transaction
 * reached us in another way. In general this will be because we received funds from someone, hower there are
 * also cases where funds is send from our wallet while !selfCommitted (for example by a linked desktop wallet
 * or another wallet instance using the same keys as ours).
 *
 * Note that no notifyNewMutation events will fire until after 'notifySyncDone'
 * Therefore it is necessary to first fetch the full mutation history before starting to listen for this event.
 */
- (void)notifyNewMutation:(nonnull DBMutationRecord *)mutation
            selfCommitted:(BOOL)selfCommitted;

/**
 * Notification that an existing transaction/mutation  has updated
 *
 * Note that no notifyUpdatedTransaction events will fire until after 'notifySyncDone'
 * Therefore it is necessary to first fetch the full mutation history before starting to listen for this event.
 */
- (void)notifyUpdatedTransaction:(nonnull DBTransactionRecord *)transaction;

@end
