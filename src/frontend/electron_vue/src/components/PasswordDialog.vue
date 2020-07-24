<template>
  <div class="password-dialog">
    <novo-form-field :title="$t('common.password')">
      <input
        ref="password"
        type="password"
        v-model="password"
        @keydown="validatePasswordOnEnter"
        :class="computedStatus"
      />
    </novo-form-field>
    <novo-button-section class="buttons">
      <template v-slot:right>
        <button @click="validatePassword" :disabled="isButtonDisabled">
          {{ $t("buttons.unlock") }}
        </button>
      </template>
    </novo-button-section>
  </div>
</template>

<script>
import UnityBackend from "../unity/UnityBackend";
import EventBus from "../EventBus";

export default {
  name: "PasswordDialog",
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
      if (UnityBackend.UnlockWallet(this.password)) {
        UnityBackend.LockWallet();
        this.$store.dispatch({
          type: "SET_PASSWORD",
          password: this.password
        });
        EventBus.$emit("close-dialog");
      } else {
        this.isPasswordInvalid = true;
      }
    }
  }
};
</script>
