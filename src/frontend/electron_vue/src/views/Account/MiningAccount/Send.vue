<template>
  <div class="send-coins flex-col">
    <div class="main">
      <app-form-field>
        <input v-model="computedAmount" ref="amount" type="text" readonly />
      </app-form-field>
      <app-form-field title="send_coins.target_account">
        <select-list :options="fundingAccounts" :default="fundingAccount" v-model="fundingAccount" />
      </app-form-field>
      <app-form-field>
        <input
          v-model="password"
          type="password"
          v-show="walletPassword === null"
          :placeholder="$t('common.enter_your_password')"
          :class="passwordClass"
          @keydown="onPasswordKeydown"
        />
      </app-form-field>
    </div>
    <button @click="showConfirmation" :disabled="disableSendButton">
      {{ $t("buttons.send") }}
    </button>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import { formatMoneyForDisplay } from "../../../util.js";
import { LibraryController, AccountsController } from "../../../unity/Controllers";
import ConfirmTransactionDialog from "../SpendingAccount/ConfirmTransactionDialog";
import EventBus from "@/EventBus";

export default {
  name: "Send",
  data() {
    return {
      address: null,
      password: null,
      fundingAccount: null,
      isPasswordInvalid: false
    };
  },
  computed: {
    ...mapState("wallet", ["walletPassword"]),
    ...mapGetters("wallet", ["accounts", "account"]),
    computedAmount() {
      return formatMoneyForDisplay(this.account.spendable, false, 8);
    },
    computedPassword() {
      return this.walletPassword ? this.walletPassword : this.password || "";
    },
    passwordClass() {
      return this.isPasswordInvalid ? "error" : "";
    },
    fundingAccounts() {
      return this.accounts.filter(x => x.state === "Normal" && ["Desktop"].indexOf(x.type) !== -1);
    },
    hasErrors() {
      return this.isPasswordInvalid;
    },
    disableSendButton() {
      if (this.account.spendable <= 0) return true;
      if (this.computedPassword.trim().length === 0) return true;
      return false;
    }
  },
  mounted() {
    this.$refs.amount.focus();
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
      // use the original spendable amount to validate because when you convert the displayed amount back to monetary it can be higher than the original amount!
      const amount = this.account.spendable;

      // validate the address
      let address = AccountsController.GetReceiveAddress(this.fundingAccount.UUID);
      let addressInvalid = !LibraryController.IsValidNativeAddress(address);

      // validate password (confirmation dialog unlocks/locks when user confirms so don't leave it unlocked here)
      this.isPasswordInvalid = !this.isPasswordValid(this.computedPassword);

      // fixme: when address is invalid the user does not know what's wrong because we don't show an error
      if (addressInvalid || this.isPasswordInvalid) return;

      EventBus.$emit("show-dialog", {
        title: this.$t("send_coins.confirm_transaction"),
        component: ConfirmTransactionDialog,
        componentProps: {
          amount: amount,
          address: address,
          password: this.password,
          subtractFee: true
        },
        showButtons: false
      });
    },
    onTransactionSucceeded() {
      this.$router.push({ name: "account" });
    },
    isPasswordValid(password) {
      // validation can only be done by unlocking the wallet, but make sure to lock the wallet afterwards
      return LibraryController.UnlockWallet(password, 0);
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

button {
  width: 100%;
}
</style>
