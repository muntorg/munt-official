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
    <button @click="trySend" :disabled="disableSendButton">
      {{ $t("buttons.send") }}
    </button>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import { displayToMonetary } from "../../../util.js";
import {
  LibraryController,
  AccountsController
} from "../../../unity/Controllers";
import EventBus from "../../../EventBus";

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

    this.amount = this.account.spendable.toFixed(2);
  },
  methods: {
    onPasswordKeydown() {
      this.isPasswordInvalid = false;
    },
    trySend() {
      /*
       todo:
        - improve notifications / messages on success and error
       */

      // wallet needs to be unlocked to make a payment
      if (LibraryController.UnlockWallet(this.computedPassword) === false) {
        this.isPasswordInvalid = true;
      }

      if (this.hasErrors) return;

      // create payment request
      var request = {
        valid: true,
        address: AccountsController.GetReceiveAddress(this.fundingAccount.UUID),
        label: "",
        desc: "",
        amount: displayToMonetary(this.amount)
      };

      // try to make the payment
      let result = LibraryController.PerformPaymentToRecipient(request, true);
      if (result === 0) {
        // payment succeeded
        EventBus.$emit("close-right-sidebar");
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
.send-coins {
  height: 100%;

  .main {
    flex: 1;
  }
}

input {
  background-color: #eee;
  border: 0;
  margin: 0 0 10px 0;
  font-style: normal;
  font-size: 14px;
}

.amount {
  cursor: default;
}

button {
  width: 100%;
}
</style>
