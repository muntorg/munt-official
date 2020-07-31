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
        step="1"
        placeholder="0.00"
        min="0"
        :max="maxAmount"
      />
      <input
        v-model="address"
        type="text"
        :placeholder="$t('send_novo.enter_novo_address')"
      />
      <input
        v-model="label"
        type="text"
        :placeholder="$t('send_novo.enter_label')"
      />

      <input
        v-model="password"
        type="password"
        v-show="walletPassword === null"
        :placeholder="$t('send_novo.enter_password')"
        :class="computedStatus"
        @keydown="onPasswordKeydown"
      />
    </div>
    <button @click="trySend" :disabled="disableSendButton">
      {{ $t("buttons.send") }}
    </button>
  </div>
</template>

<script>
import { mapState } from "vuex";
import UnityBackend from "../../../unity/UnityBackend";

export default {
  name: "SendNovo",
  data() {
    return {
      amount: null,
      maxAmount: null,
      address: null,
      label: null,
      password: null,
      isAmountInvalid: false,
      isAddressInvalid: false,
      isPasswordInvalid: false
    };
  },
  computed: {
    ...mapState(["walletPassword"]),
    computedPassword() {
      return this.walletPassword ? this.walletPassword : this.password;
    },
    computedStatus() {
      return this.isPasswordInvalid ? "error" : "";
    },
    isValidAddress() {
      if (this.address === null || this.address.trim().length === 0)
        return false;
      return UnityBackend.IsValidNativeAddress(this.address);
    },
    disableSendButton() {
      if (isNaN(parseFloat(this.amount))) return true;
      if (!this.isValidAddress) return true;
      if (this.computedPassword.trim().length === 0) return true;
      return false;
    }
  },
  created() {
    this.maxAmount =
      UnityBackend.GetActiveAccountBalance().availableExcludingLocked /
      100000000;
  },
  mounted() {
    this.$refs.amount.focus();
  },
  methods: {
    onPasswordKeydown() {
      this.isPasswordInvalid = false;
    },
    trySend() {
      /*
       todo:
        - validate amount
        - validate address
        - show success / error notification (after payment)
       */
      let accountBalance = UnityBackend.GetActiveAccountBalance();
      console.log(accountBalance.availableExcludingLocked);

      // wallet needs to be unlocked to make a payment
      if (UnityBackend.UnlockWallet(this.computedPassword) === false) {
        this.isPasswordInvalid = true;
        return; // invalid password
      }
      // create payment request
      var request = {
        valid: true,
        address: this.address,
        label: this.label || "",
        desc: "",
        amount: this.amount * 100000000
      };

      // try to make the payment
      let result = UnityBackend.PerformPaymentToRecipient(request, false);
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
      UnityBackend.LockWallet();
    }
  }
};
</script>

<style lang="less" scoped>
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
