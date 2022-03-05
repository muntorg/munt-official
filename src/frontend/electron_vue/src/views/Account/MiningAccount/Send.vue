<template>
  <div class="send-coins flex-col">
    <portal to="sidebar-right-title">
      {{ $t("buttons.send") }}
    </portal>

    <div class="main">
      <input
        class="amount"
        v-model="amount"
        ref="amount"
        type="text"
        readonly
      />
      <app-form-field :title="$t('send_coins.target_account')">
        <select-list
          :options="fundingAccounts"
          :default="fundingAccount"
          v-model="fundingAccount"
        />
      </app-form-field>

      <input
        v-model="password"
        type="password"
        v-show="walletPassword === null"
        :placeholder="$t('common.enter_your_password')"
        :class="passwordClass"
        @keydown="onPasswordKeydown"
      />
    </div>
    <button @click="showConfirmation" :disabled="disableSendButton">
      {{ $t("buttons.send") }}
    </button>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import { displayToMonetary, formatMoneyForDisplay } from "../../../util.js";
import {
  LibraryController,
  AccountsController
} from "../../../unity/Controllers";
import ConfirmTransactionDialog from "../SpendingAccount/ConfirmTransactionDialog";
import EventBus from "@/EventBus";

export default {
  name: "Send",
  data() {
    return {
      amount: null,
      address: null,
      password: null,
      fundingAccount: null,
      isPasswordInvalid: false
    };
  },
  computed: {
    ...mapState("wallet", ["walletPassword"]),
    ...mapGetters("wallet", ["accounts", "account"]),
    computedPassword() {
      return this.walletPassword ? this.walletPassword : this.password || "";
    },
    passwordClass() {
      return this.isPasswordInvalid ? "error" : "";
    },
    fundingAccounts() {
      return this.accounts.filter(
        x => x.state === "Normal" && ["Desktop"].indexOf(x.type) !== -1
      );
    },
    hasErrors() {
      return this.isPasswordInvalid;
    },
    disableSendButton() {
      if (isNaN(parseFloat(this.amount))) return true;
      if (this.computedPassword.trim().length === 0) return true;
      return false;
    }
  },
  mounted() {
    this.$refs.amount.focus();
    if (this.fundingAccounts.length) {
      this.fundingAccount = this.fundingAccounts[0];
    }

    this.amount = formatMoneyForDisplay(this.account.spendable);

    EventBus.$on("transaction-succeeded", this.onTransactionSucceeded);
  },
  beforeDestroy() {
    EventBus.$off("transaction-succeeded", this.onTransactionSucceeded);
  },
  methods: {
    onPasswordKeydown() {
      this.isPasswordInvalid = false;
    },
    showConfirmation() {
      /*
       todo:
        - replace amount input by custom amount input (this one is too basic)
        - improve notifications / messages on success and error
       */

      // validate amount
      let accountBalance = AccountsController.GetActiveAccountBalance();
      let amountInvalid =
        accountBalance.availableExcludingLocked <
        displayToMonetary(this.amount);
      // validate address
      let address = AccountsController.GetReceiveAddress(
        this.fundingAccount.UUID
      );
      let addressInvalid = !LibraryController.IsValidNativeAddress(address);

      // wallet needs to be unlocked to make a payment
      if (LibraryController.UnlockWallet(this.computedPassword) === false) {
        this.isPasswordInvalid = true;
      }

      if (amountInvalid || addressInvalid) return;

      EventBus.$emit("show-dialog", {
        title: this.$t("send_coins.confirm_transaction"),
        component: ConfirmTransactionDialog,
        componentProps: {
          amount: this.amount,
          address: address,
          password: this.password
        },
        showButtons: false
      });
    },
    onTransactionSucceeded() {
      this.$router.push({ name: "account" });
    }
  }
};
</script>

<style lang="less" scoped>
.send-coins {
  height: 100%;

  .main {
    flex: 1;
  }
}

.amount {
  cursor: default;
}

button {
  width: 100%;
}
</style>
