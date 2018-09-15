package com.gulden.jniunifiedbackend;

import com.gulden.unity_wallet.MainActivity;

public class GuldenUnifiedFrontendImpl extends GuldenUnifiedFrontend {
    public MainActivity activity;
    @Override public boolean notifySPVProgress(int startHeight, int progessHeight, int expectedHeight)
    {
        if (expectedHeight-startHeight == 0)
        {
            activity.updateSyncProgressPercent(0);
        }
        else {
            activity.updateSyncProgressPercent(((Float.valueOf(progessHeight) - startHeight) / (expectedHeight - startHeight)) * 100);
        }
        return true;
    }

    @Override
    public boolean notifyBalanceChange(BalanceRecord newBalance) {
        activity.updateBalance(newBalance.mAvailableIncludingLocked + newBalance.mImmatureIncludingLocked + newBalance.mUnconfirmedIncludingLocked);
        return false;
    }
}
