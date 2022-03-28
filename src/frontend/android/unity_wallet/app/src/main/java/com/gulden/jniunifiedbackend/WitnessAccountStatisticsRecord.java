// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

package com.gulden.jniunifiedbackend;

public final class WitnessAccountStatisticsRecord implements android.os.Parcelable {


    /*package*/ final String mRequestStatus;

    /*package*/ final String mAccountStatus;

    /*package*/ final long mAccountWeight;

    /*package*/ final long mAccountWeightAtCreation;

    /*package*/ final long mAccountParts;

    /*package*/ final long mAccountAmountLocked;

    /*package*/ final long mAccountAmountLockedAtCreation;

    /*package*/ final long mNetworkTipTotalWeight;

    /*package*/ final long mNetworkTotalWeightAtCreation;

    /*package*/ final long mAccountInitialLockPeriodInBlocks;

    /*package*/ final long mAccountRemainingLockPeriodInBlocks;

    /*package*/ final long mAccountExpectedWitnessPeriodInBlocks;

    /*package*/ final long mAccountEstimatedWitnessPeriodInBlocks;

    /*package*/ final long mAccountInitialLockCreationBlockHeight;

    /*package*/ final int mCompoundingPercent;

    public WitnessAccountStatisticsRecord(
            String requestStatus,
            String accountStatus,
            long accountWeight,
            long accountWeightAtCreation,
            long accountParts,
            long accountAmountLocked,
            long accountAmountLockedAtCreation,
            long networkTipTotalWeight,
            long networkTotalWeightAtCreation,
            long accountInitialLockPeriodInBlocks,
            long accountRemainingLockPeriodInBlocks,
            long accountExpectedWitnessPeriodInBlocks,
            long accountEstimatedWitnessPeriodInBlocks,
            long accountInitialLockCreationBlockHeight,
            int compoundingPercent) {
        this.mRequestStatus = requestStatus;
        this.mAccountStatus = accountStatus;
        this.mAccountWeight = accountWeight;
        this.mAccountWeightAtCreation = accountWeightAtCreation;
        this.mAccountParts = accountParts;
        this.mAccountAmountLocked = accountAmountLocked;
        this.mAccountAmountLockedAtCreation = accountAmountLockedAtCreation;
        this.mNetworkTipTotalWeight = networkTipTotalWeight;
        this.mNetworkTotalWeightAtCreation = networkTotalWeightAtCreation;
        this.mAccountInitialLockPeriodInBlocks = accountInitialLockPeriodInBlocks;
        this.mAccountRemainingLockPeriodInBlocks = accountRemainingLockPeriodInBlocks;
        this.mAccountExpectedWitnessPeriodInBlocks = accountExpectedWitnessPeriodInBlocks;
        this.mAccountEstimatedWitnessPeriodInBlocks = accountEstimatedWitnessPeriodInBlocks;
        this.mAccountInitialLockCreationBlockHeight = accountInitialLockCreationBlockHeight;
        this.mCompoundingPercent = compoundingPercent;
    }

    /** Success if request succeeded, otherwise an error message */
    public String getRequestStatus() {
        return mRequestStatus;
    }

    /** Current state of the witness account, one of: "empty", "empty_with_remainder", "pending", "witnessing", "ended", "expired", "emptying" */
    public String getAccountStatus() {
        return mAccountStatus;
    }

    /** Account weight */
    public long getAccountWeight() {
        return mAccountWeight;
    }

    /** Account weight when it was created */
    public long getAccountWeightAtCreation() {
        return mAccountWeightAtCreation;
    }

    /** How many parts the account weight is split up into */
    public long getAccountParts() {
        return mAccountParts;
    }

    /** Account amount currently locked */
    public long getAccountAmountLocked() {
        return mAccountAmountLocked;
    }

    /** Account amount locked when it was created */
    public long getAccountAmountLockedAtCreation() {
        return mAccountAmountLockedAtCreation;
    }

    /** Current network weight */
    public long getNetworkTipTotalWeight() {
        return mNetworkTipTotalWeight;
    }

    /** Network weight when account was created */
    public long getNetworkTotalWeightAtCreation() {
        return mNetworkTotalWeightAtCreation;
    }

    /** Account total lock period in blocks (from creation block) */
    public long getAccountInitialLockPeriodInBlocks() {
        return mAccountInitialLockPeriodInBlocks;
    }

    /** Account remaining lock period in blocks (from chain tip) */
    public long getAccountRemainingLockPeriodInBlocks() {
        return mAccountRemainingLockPeriodInBlocks;
    }

    /** How often the account is "expected" by the network to witness in order to not be kicked off */
    public long getAccountExpectedWitnessPeriodInBlocks() {
        return mAccountExpectedWitnessPeriodInBlocks;
    }

    /** How often the account is estimated to witness */
    public long getAccountEstimatedWitnessPeriodInBlocks() {
        return mAccountEstimatedWitnessPeriodInBlocks;
    }

    /** Height at which the account lock first entered the chain */
    public long getAccountInitialLockCreationBlockHeight() {
        return mAccountInitialLockCreationBlockHeight;
    }

    /** How much of the reward that this account earns is set to be compound */
    public int getCompoundingPercent() {
        return mCompoundingPercent;
    }

    @Override
    public String toString() {
        return "WitnessAccountStatisticsRecord{" +
                "mRequestStatus=" + mRequestStatus +
                "," + "mAccountStatus=" + mAccountStatus +
                "," + "mAccountWeight=" + mAccountWeight +
                "," + "mAccountWeightAtCreation=" + mAccountWeightAtCreation +
                "," + "mAccountParts=" + mAccountParts +
                "," + "mAccountAmountLocked=" + mAccountAmountLocked +
                "," + "mAccountAmountLockedAtCreation=" + mAccountAmountLockedAtCreation +
                "," + "mNetworkTipTotalWeight=" + mNetworkTipTotalWeight +
                "," + "mNetworkTotalWeightAtCreation=" + mNetworkTotalWeightAtCreation +
                "," + "mAccountInitialLockPeriodInBlocks=" + mAccountInitialLockPeriodInBlocks +
                "," + "mAccountRemainingLockPeriodInBlocks=" + mAccountRemainingLockPeriodInBlocks +
                "," + "mAccountExpectedWitnessPeriodInBlocks=" + mAccountExpectedWitnessPeriodInBlocks +
                "," + "mAccountEstimatedWitnessPeriodInBlocks=" + mAccountEstimatedWitnessPeriodInBlocks +
                "," + "mAccountInitialLockCreationBlockHeight=" + mAccountInitialLockCreationBlockHeight +
                "," + "mCompoundingPercent=" + mCompoundingPercent +
        "}";
    }


    public static final android.os.Parcelable.Creator<WitnessAccountStatisticsRecord> CREATOR
        = new android.os.Parcelable.Creator<WitnessAccountStatisticsRecord>() {
        @Override
        public WitnessAccountStatisticsRecord createFromParcel(android.os.Parcel in) {
            return new WitnessAccountStatisticsRecord(in);
        }

        @Override
        public WitnessAccountStatisticsRecord[] newArray(int size) {
            return new WitnessAccountStatisticsRecord[size];
        }
    };

    public WitnessAccountStatisticsRecord(android.os.Parcel in) {
        this.mRequestStatus = in.readString();
        this.mAccountStatus = in.readString();
        this.mAccountWeight = in.readLong();
        this.mAccountWeightAtCreation = in.readLong();
        this.mAccountParts = in.readLong();
        this.mAccountAmountLocked = in.readLong();
        this.mAccountAmountLockedAtCreation = in.readLong();
        this.mNetworkTipTotalWeight = in.readLong();
        this.mNetworkTotalWeightAtCreation = in.readLong();
        this.mAccountInitialLockPeriodInBlocks = in.readLong();
        this.mAccountRemainingLockPeriodInBlocks = in.readLong();
        this.mAccountExpectedWitnessPeriodInBlocks = in.readLong();
        this.mAccountEstimatedWitnessPeriodInBlocks = in.readLong();
        this.mAccountInitialLockCreationBlockHeight = in.readLong();
        this.mCompoundingPercent = in.readInt();
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(android.os.Parcel out, int flags) {
        out.writeString(this.mRequestStatus);
        out.writeString(this.mAccountStatus);
        out.writeLong(this.mAccountWeight);
        out.writeLong(this.mAccountWeightAtCreation);
        out.writeLong(this.mAccountParts);
        out.writeLong(this.mAccountAmountLocked);
        out.writeLong(this.mAccountAmountLockedAtCreation);
        out.writeLong(this.mNetworkTipTotalWeight);
        out.writeLong(this.mNetworkTotalWeightAtCreation);
        out.writeLong(this.mAccountInitialLockPeriodInBlocks);
        out.writeLong(this.mAccountRemainingLockPeriodInBlocks);
        out.writeLong(this.mAccountExpectedWitnessPeriodInBlocks);
        out.writeLong(this.mAccountEstimatedWitnessPeriodInBlocks);
        out.writeLong(this.mAccountInitialLockCreationBlockHeight);
        out.writeInt(this.mCompoundingPercent);
    }

}
