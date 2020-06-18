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
          <span v-else>{{ $t("setup.step3.header") }}</span>
        </h2>

        <!-- step 1: Enter old password -->
        <div v-if="current === 1" class="password">
          <div class="password-row">
            <h4>{{ $t("setup.step3.password") }}:</h4>
            <input ref="password" type="password" v-model="passwordold" @keyup="onPasswordKeyUp" :class="{ error: isPasswordInvalid }" />
          </div>
        </div>
        <!-- step 2: enter new password -->
        <div v-else class="password">
          <div class="password-row">
            <h4>{{ $t("setup.step3.password") }}:</h4>
            <input type="password" v-model="password1" />
          </div>
          <div class="password-row">
            <h4>{{ $t("setup.step3.repeat_password") }}:</h4>
            <input type="password" v-model="password2" />
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
        <span v-if="current < 2">
          {{ $t("buttons.Next") }}
        </span>
        <span v-else>
          Change password
        </span>
      </button>
    </div>
  </div>
</template>


<script>
import NovoBackend from "../../libnovo/NovoBackend";

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
    computePassword1Help() {
      return this.password1 !== null &&
        this.password1.length > 0 &&
        this.password1.length < 6
        ? "setup.step3.password1_placeholder"
        : "";
    },
    computePassword1Status() {
      return this.password1 !== null && this.password1.length >= 6
        ? "success"
        : "";
    },
    computePassword2Help() {
      return this.calculatePassword2Status() === "error"
        ? "setup.step3.passwords_dont_match_error"
        : "";
    },
    computePassword2Status() {
      return this.calculatePassword2Status();
    }
  },
  mounted() {
    this.$refs.password.focus();
  },
  methods: {
    isNextDisabled() {
      switch (this.current) {
        case 1:
          return false;
        case 2:
          return this.calculatePassword2Status() !== "success";          
      }
      return true;
    },
    async nextStep() {
      switch (this.current) {
        case 1:
          this.validatePassword();
          break;
        case 2:
          if (NovoBackend.ChangePassword(this.passwordold, this.password2))
          {
            this.$router.push({ name: "wallet" });
          }
          break;
      }
    },
    onPasswordKeyUp() {
      this.isPasswordInvalid = false;
      if (event.keyCode !== 13) return;
      this.validatePassword();
    },
    validatePassword() {
      if (NovoBackend.UnlockWallet(this.passwordold))
      {
        NovoBackend.LockWallet();
        this.current++;
      }
      else
      {
        this.isPasswordInvalid = true;
      }
    },
    calculatePassword2Status() {
      //if (this.password1 === null || this.password1.length < 6) return "";
      if (
        this.password2 === null ||
        this.password2.length < this.password1.length
      )
        return "";

      return this.password1 === this.password2 ? "success" : "error";
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
