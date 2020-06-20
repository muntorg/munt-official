<template>
  <div id="setup-container">
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
          <h4>{{ $t("setup.password") }}:</h4>
          <input
            ref="passwordold"
            type="password"
            v-model="passwordold"
            @keydown="onPasswordKeyDown"
            :class="{ error: isPasswordInvalid }"
          />
        </div>
      </div>
      <!-- step 2: enter new password -->
      <div v-else class="password">
        <div class="password-row">
          <h4>{{ $t("setup.password") }}:</h4>
          <input ref="password" type="password" v-model="password1" />
        </div>
        <div class="password-row">
          <h4>{{ $t("setup.repeat_password") }}:</h4>
          <input
            type="password"
            v-model="password2"
            @keydown="onPasswordRepeatKeyDown"
          />
        </div>
      </div>
    </div>
    <div class="steps-buttons wrapper">
      <button
        class="btn"
        v-if="current <= 2"
        @click="nextStep"
        :disabled="isNextDisabled()"
      >
        <span v-if="current < 2">{{ $t("buttons.next") }}</span>
        <span v-else>{{ $t("buttons.change_password") }}</span>
      </button>
    </div>
  </div>
</template>

<script>
import UnityBackend from "../../unity/UnityBackend";

export default {
  data() {
    return {
      current: 1,
      password1: null,
      password2: null,
      passwordold: null,
      isPasswordInvalid: false
    };
  },
  computed: {
    passwordsValidated() {
      if (this.password1 === null || this.password1.length < 6) return false;
      if (
        this.password2 === null ||
        this.password2.length < this.password1.length
      )
        return false;

      return this.password1 === this.password2;
    }
  },
  mounted() {
    this.$refs.passwordold.focus();
  },
  methods: {
    isNextDisabled() {
      switch (this.current) {
        case 1:
          return false;
        case 2:
          return this.passwordsValidated === false;
      }
      return true;
    },
    async nextStep() {
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
    onPasswordKeyDown() {
      this.isPasswordInvalid = false;
      if (event.keyCode === 13) this.validatePassword();
    },
    onPasswordRepeatKeyDown() {
      if (event.keyCode === 13 && this.passwordsValidated) this.nextStep();
    },
    validatePassword() {
      if (UnityBackend.UnlockWallet(this.passwordold)) {
        UnityBackend.LockWallet();
        this.current++;
        setTimeout(() => {
          this.$refs.password.focus();
        }, 100);
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

.steps-buttons {
  position: absolute;
  bottom: 20px;
  right: 0px;
}

.password {
  margin: 0 0 20px 0;
}

.password-row {
  margin: 0 0 20px 0;
}

.password input {
  height: 40px;
  width: 100%;
  font-style: normal;
  font-weight: 400;
  line-height: 22px;
  font-size: 16px;
  color: #000;
  padding: 10px;
  border: 1px solid #ccc;
  background-color: #fff;
  transition: all 0.3s;
  border-radius: 0px;
}

.password input:focus {
  color: #009572;
  border: 1px solid #009572;
}

input.error,
input:focus.error {
  color: var(--error-text-color, #dd3333);
  border-color: var(--error-color, #dd3333);
  background: var(--error-color, #fff);
}
</style>
