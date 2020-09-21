<template>
  <div class="send-novo flex-col">
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
      <novo-form-field :title="$t('send_novo.target_account')">
        <div class="selectfunding">
          <select v-model="fundingAccount">
            <option
              v-for="account in fundingAccounts"
              :key="account.UUID"
              :value="account"
              >{{ account.label }}</option
            >
          </select>
          <span class="selectarrow">
            <fa-icon :icon="['fal', 'chevron-down']" />
          </span>
        </div>
      </novo-form-field>

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
import {
  LibraryController,
  AccountsController
} from "../../../unity/Controllers";

export default {
  name: "SendNovo",
  data() {
    return {
      amount: null,
      maxAmount: null,
      address: null,
      password: null,
      fundingAccount: null,
      isAmountInvalid: false,
      isPasswordInvalid: false
    };
  },
  computed: {
    ...mapState("wallet", ["walletPassword"]),
    ...mapGetters("wallet", ["accounts"]),
    computedPassword() {
      return this.walletPassword ? this.walletPassword : this.password || "";
    },
    amountClass() {
      return this.isAmountInvalid ? "error" : "";
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
      return this.isAmountInvalid || this.isPasswordInvalid;
    },
    disableSendButton() {
      if (isNaN(parseFloat(this.amount))) return true;
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
    this.amount = this.maxAmount;
  },
  mounted() {
    this.$refs.amount.focus();
    if (this.fundingAccounts.length) {
      this.fundingAccount = this.fundingAccounts[0];
    }
  },
  methods: {
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
        amount: this.amount * 100000000
      };

      // try to make the payment
      let result = LibraryController.PerformPaymentToRecipient(request, true);
      if (result === 0) {
        // payment succeeded
        this.amount = null;
        this.address = null;
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
.selectfunding {
  position: relative;
  float: left;
  width: 100%;
}
.selectfunding select {
  background-color: rgba(255, 255, 255, 0);
  z-index: 999;
}
.selectarrow {
  position: absolute;
  top: 0;
  right: 10px;
  line-height: 40px;
  font-size: 12px;
  z-index: -1;
}

.send-novo {
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

button {
  width: 100%;
}
</style>
