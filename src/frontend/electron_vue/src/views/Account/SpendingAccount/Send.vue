<template>
  <div class="send-view flex-col">
    <portal v-if="UIConfig.showSidebar" to="sidebar-right-title">
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
      <button @click="clearInput" class="clear" :disabled="disableClearButton">
        {{ $t("buttons.clear") }}
      </button>
      <button
        @click="showConfirmation"
        class="send-coins"
        :disabled="disableSendButton"
      >
        {{ $t("buttons.send") }}
      </button>
      <button @click="sellCoins" class="sell-coins" :disabled="sellDisabled">
        {{ $t("buttons.sell_coins") }}
      </button>
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { LibraryController, AccountsController } from "@/unity/Controllers";
import ConfirmTransactionDialog from "./ConfirmTransactionDialog";
import EventBus from "@/EventBus";
import { BackendUtilities } from "@/unity/Controllers";
import UIConfig from "../../../../ui-config.json";

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
      sellDisabled: false,
      UIConfig: UIConfig
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
    disableClearButton() {
      if (this.amount !== null && !isNaN(parseFloat(this.amount))) return false;
      if (this.address !== null && this.address.length > 0) return false;
      if (this.password !== null && this.password.length > 0) return false;
      return true;
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
    EventBus.$on("transaction-succeeded", this.onTransactionSucceeded);
  },
  beforeDestroy() {
    EventBus.$off("transaction-succeeded", this.onTransactionSucceeded);
  },
  methods: {
    async sellCoins() {
      try {
        this.sellDisabled = true;
        let url = await BackendUtilities.GetSellSessionUrl();
        if (!url) {
          url = "https://gulden.com/sell";
        }
        window.open(url, "sell-gulden");
      } finally {
        this.sellDisabled = false;
      }
    },
    onPasswordKeydown() {
      this.isPasswordInvalid = false;
    },
    clearInput() {
      this.amount = null;
      this.address = null;
      this.password = null;
      this.label = null;
      this.$refs.amount.focus();
    },
    showConfirmation() {
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

      EventBus.$emit("show-dialog", {
        title: this.$t("send_coins.confirm_transaction"),
        component: ConfirmTransactionDialog,
        componentProps: {
          amount: this.amount,
          address: this.address,
          password: this.password
        },
        showButtons: false
      });
    },
    onTransactionSucceeded() {
      this.$router.push({ name: "transactions" });
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

input[type="number"]::-webkit-inner-spin-button {
  -webkit-appearance: none;
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
  flex-wrap: wrap;
  justify-content: center;
  align-items: center;

  & > .clear {
    width: 150px;
    height: 40px;
    margin-bottom: 5px;
  }
  & > .clear:not([disabled]) {
    height: 40px;
    line-height: 39px;
    background-color: #fff;
    border: 1px solid var(--primary-color);
    color: var(--primary-color);
  }
  & > .send-coins {
    margin: 0 15px 0 15px;
    min-width: 150px;
    width: calc(100% - 170px - 30px - 30px - 170px);
    margin-bottom: 5px;
  }
  & > .sell-coins {
    width: 150px;
    margin-bottom: 5px;
  }
}
</style>
