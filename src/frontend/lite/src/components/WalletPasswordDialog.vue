<template>
  <div class="wallet-password-dialog">
    <app-form-field title="common.password">
      <input ref="password" type="password" v-model="password" @keydown="validatePasswordOnEnter" :class="computedStatus" />
    </app-form-field>
    <app-button-section class="buttons">
      <button slot="right" @click="validatePassword" :disabled="isButtonDisabled">
        {{ $t("buttons.unlock") }}
      </button>
    </app-button-section>
  </div>
</template>

<script>
import { LibraryController } from "../unity/Controllers";
import EventBus from "../EventBus";

export default {
  name: "WalletPasswordDialog",
  props: {
    value: {
      type: String,
      default: null
    }
  },
  data() {
    return {
      password: "",
      isPasswordInvalid: false
    };
  },
  computed: {
    computedStatus() {
      return this.isPasswordInvalid ? "error" : "";
    },
    isButtonDisabled() {
      return this.password.trim().length === 0;
    }
  },
  mounted() {
    this.$refs.password.focus();
  },
  methods: {
    validatePasswordOnEnter() {
      this.isPasswordInvalid = false;
      if (event.keyCode === 13) this.validatePassword();
    },
    validatePassword() {
      if (LibraryController.UnlockWallet(this.password, 120)) {
        LibraryController.LockWallet();
        this.$store.dispatch("wallet/SET_WALLET_PASSWORD", this.password);
        EventBus.$emit("close-dialog");
      } else {
        this.isPasswordInvalid = true;
      }
    }
  }
};
</script>
