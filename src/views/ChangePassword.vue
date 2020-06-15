<template>
  <div id="setup-container">
    <div class="steps-container section">
      <!-- step 1: Enter old password -->
      <div v-if="current === 1">
        <h2>{{ "Enter your password" }}</h2>
        <div class="password">
          <div class="password-row">
            <h4>{{ $t("setup.step3.password") }}:</h4>
            <input type="password" v-model="passwordold" />
          </div>
        </div>
      </div>

      <!-- step 2: enter new password -->
      <div v-else-if="current === 2">
        <h2>{{ $t("setup.step3.header") }}</h2>
        <div class="password">
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
import NovoBackend from "../libnovo/NovoBackend";

export default {
  data() {
    return {
      current: 1,
      recoveryPhrase: null,
      matchingWords: 0,
      password1: null,
      password2: null,
      passwordold: null,
      initialized: false
    };
  },
  async mounted() {
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
          if (NovoBackend.UnlockWallet(this.passwordold))
          {
            this.current++;
          }
          else
          {
            alert("Invalid password");
          }
          break;
        case 2:
          if (this.initialized) return;
          this.initialized = true;
          if (NovoBackend.ChangePassword(this.passwordold, this.password2))
          {
            this.$router.push({ name: "wallet" }); // maybe also route from backend  
          }
          else
          {
            alert("Invalid password");
          }
          break;
      }
    },
    onAcceptChange() {
      this.accepted = !this.accepted;
    },
    onMatchChanged(match) {
      this.matchingWords += match ? 1 : -1;
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

.phrase-repeat {
  width: calc(100% + 10px);
  margin: -5px -5px 35px -5px;
}

.phrase-repeat input {
  width: calc(25% - 10px);
  margin: 5px;
  height: 40px;
  font-style: normal;
  font-weight: 400;
  line-height: 22px;
  font-size: 16px;
  padding: 10px;
  border-radius: 0px;
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
