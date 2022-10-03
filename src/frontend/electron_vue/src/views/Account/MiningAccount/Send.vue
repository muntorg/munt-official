<template>
  <div class="send-coins flex-col">
    <div class="main">
      <app-form-field>
        <input v-model="computedAmount" ref="amount" type="text" readonly />
      </app-form-field>
      <app-form-field title="send_coins.target_account">
        <select-list :options="fundingAccounts" :default="fundingAccount" v-model="fundingAccount" />
      </app-form-field>
    </div>
    <button @click="showConfirmation" :disabled="disableSendButton">
      {{ $t("buttons.send") }}
    </button>
  </div>
</template>

<script>
import { mapGetters } from "vuex";
import { formatMoneyForDisplay } from "../../../util.js";
import { LibraryController, AccountsController } from "../../../unity/Controllers";
import ConfirmTransactionDialog from "../SpendingAccount/ConfirmTransactionDialog";
import EventBus from "@/EventBus";

export default {
  name: "Send",
  data() {
    return {
      address: null,
      fundingAccount: null
    };
  },
  computed: {
    ...mapGetters("wallet", ["accounts", "account"]),
    computedAmount() {
      return formatMoneyForDisplay(this.account.spendable, false, 8);
    },
    fundingAccounts() {
      return this.accounts.filter(x => x.state === "Normal" && ["Desktop"].indexOf(x.type) !== -1);
    },
    disableSendButton() {
      if (this.account.spendable <= 0) return true;
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
    showConfirmation() {
      // use the original spendable amount to validate because when you convert the displayed amount back to monetary it can be higher than the original amount!
      const amount = this.account.spendable;

      // validate the address
      let address = AccountsController.GetReceiveAddress(this.fundingAccount.UUID);
      let addressInvalid = !LibraryController.IsValidNativeAddress(address);

      // fixme: when address is invalid the user does not know what's wrong because we don't show an error
      if (addressInvalid) return;

      EventBus.$emit("show-dialog", {
        title: this.$t("send_coins.confirm_transaction"),
        component: ConfirmTransactionDialog,
        componentProps: {
          amount: amount,
          address: address,
          subtractFee: true
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

button {
  width: 100%;
}
</style>
