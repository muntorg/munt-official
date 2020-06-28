<template>
  <div class="change-password-view">
    <h2>
      <span v-if="current === 1">{{ $t("setup.enter_your_password") }}</span>
      <span v-else>{{ $t("setup.choose_password") }}</span>
    </h2>

    <!-- step 1: Enter old password -->
    <novo-form-field v-if="current === 1" :title="$t('common.password')">
      <input
        ref="passwordold"
        type="password"
        v-model="passwordold"
        @keydown="validatePasswordOnEnter"
        :class="passwordOldStatus"
      />
    </novo-form-field>

    <!-- step 2: enter new password -->
    <div v-else>
      <novo-form-field :title="$t('common.password')">
        <input ref="password1" type="password" v-model="password1" />
      </novo-form-field>
      <novo-form-field :title="$t('setup.repeat_password')">
        <input
          type="password"
          v-model="password2"
          :class="password2Status"
          @keydown="validatePasswordRepeatOnEnter"
        />
      </novo-form-field>
    </div>

    <novo-button-section>
      <button v-if="current === 1" @click="nextStep" :disabled="isNextDisabled">
        {{ $t("buttons.next") }}
      </button>
      <button v-if="current === 2" @click="nextStep" :disabled="isNextDisabled">
        {{ $t("buttons.change_password") }}
      </button>
    </novo-button-section>
  </div>
</template>

<script>
import UnityBackend from "../../unity/UnityBackend";

export default {
  data() {
    return {
      current: 1,
      passwordold: "",
      password1: "",
      password2: "",
      isPasswordInvalid: false
    };
  },
  computed: {
    passwordOldStatus() {
      return this.isPasswordInvalid ? "error" : "";
    },
    password2Status() {
      if (this.password2.length === 0) return "";
      if (this.password2.length > this.password1.length) return "error";
      return this.password1.indexOf(this.password2) === 0 ? "" : "error";
    },
    passwordsValidated() {
      if (this.password1 === null || this.password1.length < 6) return false;
      if (
        this.password2 === null ||
        this.password2.length < this.password1.length
      )
        return false;

      return this.password1 === this.password2;
    },
    isNextDisabled() {
      switch (this.current) {
        case 1:
          return this.passwordold.trim().length === 0;
        case 2:
          return this.passwordsValidated === false;
      }
      return true;
    }
  },
  mounted() {
    this.$refs.passwordold.focus();
  },
  methods: {
    nextStep() {
      switch (this.current) {
        case 1:
          this.validatePassword();
          break;
        case 2:
          if (UnityBackend.ChangePassword(this.passwordold, this.password2)) {
            this.$router.push({ name: "wallet" });
          }
          break;
      }
    },
    validatePasswordOnEnter() {
      this.isPasswordInvalid = false;
      if (event.keyCode === 13) this.validatePassword();
    },
    validatePasswordRepeatOnEnter() {
      if (event.keyCode === 13 && this.passwordsValidated) this.nextStep();
    },
    validatePassword() {
      if (UnityBackend.UnlockWallet(this.passwordold)) {
        UnityBackend.LockWallet();
        this.current++;
        this.$nextTick(() => {
          this.$refs.password1.focus();
        });
      } else {
        this.isPasswordInvalid = true;
      }
    }
  }
};
</script>
