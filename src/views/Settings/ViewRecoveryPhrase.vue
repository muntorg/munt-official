<template>
  <div id="setup-container">
    <div class="steps-container section">
      <!-- step 1: Enter password -->
      <div v-if="current === 1">
        <h2>{{ "Enter your password" }}</h2>
        <div class="password">
          <div class="password-row">
            <h4>{{ $t("setup.step3.password") }}:</h4>
            <input type="password" v-model="password" />
          </div>
        </div>
      </div>

      <!-- step 2: Show recovery phrase -->
      <div v-else-if="current === 2">
        <h2>{{ $t("setup.step1.header") }}</h2>
        <div class="phrase">
          {{ recoveryPhrase }}
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
          {{ $t("buttons.Finish") }}
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
      recoveryPhrase: null,
      password: null
    };
  },
  methods: {
    isNextDisabled() {
      return false;
    },
    async nextStep() {
      switch (this.current) {
        case 1:
          if (NovoBackend.UnlockWallet(this.password))
          {
              this.recoveryPhrase = NovoBackend.GetRecoveryPhrase();
              NovoBackend.LockWallet();
              this.current++;
          } 
          else
          {
            alert("Invalid password");
          }
          break;
        case 2:
          this.$router.push({ name: "wallet" });
          break;
      }
    }
  }
};
</script>

<style lang="less" scoped>
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
</style>
