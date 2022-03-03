<template>
  <div class="confirm-transaction-dialog">
    <div class="tx-amount">{{ computedAmount }}</div>
    <div class="tx-fee">{{ computedFee }}</div>
    <div class="tx-to">
      <fa-icon :icon="['far', 'long-arrow-down']" />
    </div>
    <div class="tx-address">{{ address }}</div>
    <button @click="confirm" class="button">
      {{ $t("buttons.confirm") }}
    </button>
  </div>
</template>

<script>
import EventBus from "@/EventBus";
import { formatMoneyForDisplay, displayToMonetary } from "../../../util.js";
import { LibraryController } from "@/unity/Controllers";

export default {
  name: "ConfirmTransactionDialog",
  props: {
    amount: null,
    address: null,
    password: null
  },
  computed: {
    computedRequest() {
      return {
        valid: true,
        address: this.address,
        label: "",
        desc: "",
        amount: displayToMonetary(this.amount)
      };
    },
    computedAmount() {
      return `${this.amount} ${this.$t("common.ticker_symbol")}`;
    },
    computedFee() {
      let fee = formatMoneyForDisplay(
        LibraryController.FeeForRecipient(this.computedRequest)
      );
      return `+ ${fee} ${this.$t("common.ticker_symbol")} FEE`;
    }
  },
  methods: {
    confirm() {
      LibraryController.UnlockWallet(this.password);

      // try to make the payment
      let result = LibraryController.PerformPaymentToRecipient(
        this.computedRequest,
        false
      );

      if (result !== 0) {
        // payment failed, log an error. have to make this more robust
        console.error(result);
      }

      // lock the wallet again
      LibraryController.LockWallet();

      EventBus.$emit("transaction-succeeded");
      EventBus.$emit("close-dialog");
    }
  }
};
</script>

<style lang="less" scoped>
.confirm-transaction-dialog {
  text-align: center;

  & > h4 {
    margin: 20px 0 0 0;
  }
}
.tx-amount {
  font-size: 1.6em;
  font-weight: 600;
  margin: 0 0 10px 0;
}
.tx-fee {
  font-size: 0.9em;
}
.tx-to {
  margin: 20px 0 10px 0;
  font-size: 1.6em;
}
.tx-address {
  padding: 10px 0 40px 0;
  font-weight: 500;
}
.button {
  width: 100%;
}
</style>
