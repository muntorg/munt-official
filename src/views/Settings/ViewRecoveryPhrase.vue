<template>
  <div id="setup-container">
    
    <div class="section">
      <div class="back">
        <router-link :to="{ name: current === 1 ? 'settings' : 'wallet' }">
            <fa-icon :icon="['fal', 'long-arrow-left']" />
            <span> Back</span>
        </router-link>
      </div>
      <h2>
        <span v-if="current === 1">{{ "Enter your password" }}</span>
        <span v-else>{{ $t("setup.step1.header") }}</span>
      </h2>
    
      <!-- step 1: Enter password -->
      <div class="password" v-if="current === 1">
        <div class="password-row">
          <h4>{{ $t("setup.step3.password") }}:</h4>
          <input ref="password" type="password" v-model="password" @keydown="onPasswordKeyDown" :class="{ error: isPasswordInvalid }" />
        </div>
      </div>

      <!-- step 2: Show recovery phrase -->
      <div v-else>
        <div class="phrase">
          {{ recoveryPhrase }}
        </div>
      </div>
    </div>
    
    <div class="steps-buttons wrapper">
      <button class="btn" v-if="current === 1" @click="nextStep">
        {{ $t("buttons.Next") }}
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
      recoveryPhrase: null,
      password: null,
      isPasswordInvalid: false
    };
  },
  mounted() {
    this.$refs.password.focus();
  },
  methods: {
    isNextDisabled() {
      return false;
    },
    async nextStep() {
      switch (this.current) {
        case 1:
          this.validatePassword();
          break;
        case 2:
          this.$router.push({ name: "wallet" });
          break;
      }
    },
    onPasswordKeyDown() {
      this.isPasswordInvalid = false;
      if (event.keyCode !== 13) return;
      this.validatePassword();
    },
    validatePassword() {
      if (UnityBackend.UnlockWallet(this.password))
      {
        this.recoveryPhrase = UnityBackend.GetRecoveryPhrase();
        UnityBackend.LockWallet();
        this.current++;
      }
      else
      {
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

.phrase {
  padding: 20px;
  margin: 0 0 40px 0;
  font-size: 1.4em;
  font-weight: 500;
  text-align: center;
  background-color: #f5f5f5;
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
  border: 1px solid #ddd;
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
  color: var(--error-text-color, #fff);
  border-color: var(--error-color, #dd3333);
  background: var(--error-color, #dd3333);
}

</style>
