<template>
  <div class="change-password-view">
    <div class="section">
      <div class="back">
        <router-link :to="{ name: current === 1 ? 'settings' : 'wallet' }">
          <fa-icon :icon="['fal', 'long-arrow-left']" />
          <span> {{ $t("buttons.back") }}</span>
        </router-link>
      </div>

      <h2>
        <span v-if="current === 1">{{ $t("setup.enter_your_password") }}</span>
        <span v-else>{{ $t("setup.choose_password") }}</span>
      </h2>

      <!-- step 1: Enter old password -->
      <div v-if="current === 1" class="password">
        <div class="password-row">
          <h4>{{ $t("common.password") }}:</h4>
          <input
            ref="passwordold"
            type="password"
            v-model="passwordold"
            @keydown="validatePasswordOnEnter"
            :class="passwordOldStatus"
          />
        </div>
      </div>

      <!-- step 2: enter new password -->
      <div v-else class="password">
        <div class="password-row">
          <h4>{{ $t("common.password") }}:</h4>
          <input ref="password1" type="password" v-model="password1" />
        </div>
        <div class="password-row">
          <h4>{{ $t("setup.repeat_password") }}:</h4>
          <input
            type="password"
            v-model="password2"
            :class="password2Status"
            @keydown="validatePasswordRepeatOnEnter"
          />
        </div>
      </div>

      <div class="button-wrapper">
        <button
          class="btn"
          v-if="current <= 2"
          @click="nextStep"
          :disabled="isNextDisabled"
        >
          <span v-if="current < 2">{{ $t("buttons.next") }}</span>
          <span v-else>{{ $t("buttons.change_password") }}</span>
        </button>
      </div>
    </div>
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

<style lang="less" scoped>
.back a {
  padding: 4px 8px;
  margin: 0 0 0 -8px;
}
.back {
  margin-bottom: 20px;
}

.back a:hover {
  background-color: #f5f5f5;
}

.button-wrapper {
  margin: 10px 0 0 0;
  float: right;
}

.password {
  margin: 0 0 20px 0;
}

.password-row {
  margin: 0 0 20px 0;
}
</style>
