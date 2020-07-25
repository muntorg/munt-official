<template>
  <div class="send-novo">
    <div class="main">
      <input
        v-model="amount"
        ref="amount"
        type="number"
        placeholder="0.00"
        min="0"
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
      />
    </div>
    <button @click="trySend">{{ $t("buttons.send") }}</button>
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
      address: null,
      label: null,
      password: null,
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
    }
  },
  mounted() {
    this.$refs.amount.focus();
  },
  methods: {
    trySend() {
      /*
       todo:
        - validate amount
        - validate address
        - show success / error notification (after payment)
       */

      // wallet needs to be unlocked to make a payment
      if (UnityBackend.UnlockWallet(this.computedPassword) === false) {
        this.isPasswordInvalid = true;
        return; // invalid password
      }

      // create payment request
      var request = {
        valid: true,
        address: this.address,
        label: this.label,
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

  display: flex;
  flex-direction: column;

  & .main {
    flex: 1;
  }
}

input {
  border: 0;
  margin: 0 0 10px 0;
  font-style: normal;
  font-weight: 400;
  font-size: 14px;
}

button {
  width: 100%;
}
</style>
