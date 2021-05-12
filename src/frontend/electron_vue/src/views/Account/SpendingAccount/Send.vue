<template>
  <div class="send-view flex-col">
    <portal to="sidebar-right-title">
      {{ $t("buttons.send") }}
    </portal>

    <div class="main">
      <input
        v-model="amount"
        ref="amount"
        type="number"
        step="0.01"
        placeholder="0.00"
        :class="amountClass"
        min="0"
        :max="maxAmount"
        @change="isAmountInvalid = false"
      />
      <input
        v-model="address"
        type="text"
        :placeholder="$t('send_coins.enter_coins_address')"
        :class="addressClass"
        @keydown="isAddressInvalid = false"
      />
      <input
        v-model="label"
        type="text"
        :placeholder="$t('send_coins.enter_label')"
      />

      <input
        v-model="password"
        type="password"
        v-show="walletPassword === null"
        :placeholder="$t('common.enter_your_password')"
        :class="passwordClass"
        @keydown="onPasswordKeydown"
      />
    </div>

    <div class="buttons">
      <button @click="sellCoins" class="sell-coins" :disabled="sellDisabled">
        {{ $t("buttons.sell_coins") }}
      </button>
      <button @click="trySend" class="send-coins" :disabled="disableSendButton">
        {{ $t("buttons.send") }}
      </button>
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import {
  LibraryController,
  AccountsController
} from "../../../unity/Controllers";
import { BackendUtilities } from "@/unity/Controllers";

export default {
  name: "Send",
  data() {
    return {
      amount: null,
      maxAmount: null,
      address: null,
      label: null,
      password: null,
      isAmountInvalid: false,
      isAddressInvalid: false,
      isPasswordInvalid: false,
      sellDisabled: false
    };
  },
  computed: {
    ...mapState("wallet", ["walletPassword"]),
    computedPassword() {
      return this.walletPassword ? this.walletPassword : this.password || "";
    },
    amountClass() {
      return this.isAmountInvalid ? "error" : "";
    },
    addressClass() {
      return this.isAddressInvalid ? "error" : "";
    },
    passwordClass() {
      return this.isPasswordInvalid ? "error" : "";
    },
    hasErrors() {
      return (
        this.isAmountInvalid || this.isAddressInvalid || this.isPasswordInvalid
      );
    },
    disableSendButton() {
      if (isNaN(parseFloat(this.amount))) return true;
      if (this.address === null || this.address.trim().length === 0)
        return true;
      if (this.computedPassword.trim().length === 0) return true;
      return false;
    }
  },
  created() {
    this.maxAmount =
      Math.floor(
        AccountsController.GetActiveAccountBalance().availableExcludingLocked /
          1000000
      ) / 100;
  },
  mounted() {
    this.$refs.amount.focus();
  },
  methods: {
    async sellCoins() {
      try {
        this.sellDisabled = true;
        let url = await BackendUtilities.GetSellSessionUrl();
        if (!url) {
          url = "https://florin.org/sell";
        }
        window.open(url, "sell-florin");
      } finally {
        this.sellDisabled = false;
      }
    },
    onPasswordKeydown() {
      this.isPasswordInvalid = false;
    },
    trySend() {
      /*
       todo:
        - replace amount input by custom amount input (this one is too basic)
        - improve notifications / messages on success and error
       */

      // validate amount
      let accountBalance = AccountsController.GetActiveAccountBalance();
      if (accountBalance.availableExcludingLocked / 100000000 < this.amount) {
        this.isAmountInvalid = true;
      }

      // validate address
      this.isAddressInvalid = !LibraryController.IsValidNativeAddress(
        this.address
      );

      // wallet needs to be unlocked to make a payment
      if (LibraryController.UnlockWallet(this.computedPassword) === false) {
        this.isPasswordInvalid = true;
      }

      if (this.hasErrors) return;

      // create payment request
      var request = {
        valid: true,
        address: this.address,
        label: this.label || "",
        desc: "",
        amount: this.amount * 100000000
      };

      // try to make the payment
      let result = LibraryController.PerformPaymentToRecipient(request, false);
      if (result === 0) {
        // payment succeeded
        this.amount = null;
        this.address = null;
        this.label = null;
        this.password = null;
      } else {
        // payment failed
        console.log("someting went wrong, but don't exactly know what.");
      }
      // lock the wallet again
      LibraryController.LockWallet();
    }
  }
};
</script>

<style lang="less" scoped>
.send-view {
  height: 100%;

  .main {
    flex: 1;
  }
}

input {
  border: 0;
  margin: 0 0 10px 0;
  font-style: normal;
  font-size: 14px;
}

.buttons {
  display: flex;
  flex-direction: row;
}

.send-coins {
  flex: 1;
  margin-left: 10px;
}
.sell-coins {
  flex: 0 0 150px;
}
</style>
