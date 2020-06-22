<template>
  <div id="setup-container">
    <div class="steps-container section">
      <!-- step 1: show recovery phrase -->
      <div v-if="current === 1">
        <h2 class="important">{{ $t("common.important") }}</h2>
        <p>{{ $t("setup.this_is_your_recovery_phrase") }}</p>
        <div class="phrase">
          {{ recoveryPhrase }}
        </div>
      </div>

      <!-- step 2: repeat recovery phrase -->
      <div v-else-if="current === 2">
        <h2>{{ $t("setup.enter_recovery_phrase") }}</h2>
        <p>{{ $t("setup.repeat_your_recovery_phrase") }}</p>
        <novo-phrase-validator
          :phrase="recoveryPhrase"
          :autofocus="true"
          @validated="onPhraseValidated"
        />
      </div>

      <!-- step 3: enter a password -->
      <div v-else-if="current === 3">
        <h2>{{ $t("setup.choose_password") }}</h2>
        <p>{{ $t("setup.choose_password_information") }}</p>
        <div class="password">
          <div class="password-row">
            <h4>{{ $t("setup.password") }}:</h4>
            <novo-input ref="password" type="password" v-model="password1" />
          </div>
          <div class="password-row">
            <h4>{{ $t("setup.repeat_password") }}:</h4>
            <novo-input
              type="password"
              v-model="password2"
              :status="password2Status"
              @keyup="onPassword2Keyup"
            />
          </div>
        </div>
      </div>

      <div class="button-wrapper">
        <novo-button
          v-if="current === 1"
          @click="nextStep"
          :disabled="isButtonDisabled()"
        >
          {{ $t("buttons.next") }}
        </novo-button>
        <novo-button v-else-if="current === 2" @click="previousStep">
          {{ $t("buttons.back") }}
        </novo-button>
        <novo-button
          v-else-if="current === 3"
          @click="nextStep"
          :disabled="isButtonDisabled()"
        >
          {{ $t("buttons.finish") }}
        </novo-button>
      </div>
    </div>
  </div>
</template>

<script>
import UnityBackend from "../unity/UnityBackend";

export default {
  data() {
    return {
      current: 1,
      recoveryPhrase: "",
      password1: "",
      password2: "",
      isRecoveryPhraseCorrect: false
    };
  },
  async mounted() {
    this.recoveryPhrase = await UnityBackend.GenerateRecoveryMnemonicAsync();
  },
  computed: {
    recoveryPhraseWords() {
      return this.recoveryPhrase.split(" ");
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
    }
  },
  watch: {
    isRecoveryPhraseCorrect() {
      if (this.isRecoveryPhraseCorrect) {
        this.nextStep();
      }
    }
  },
  methods: {
    isButtonDisabled() {
      switch (this.current) {
        case 1:
          return this.recoveryPhrase === null;
        case 2:
          return this.isRecoveryPhraseCorrect === false;
        case 3:
          return this.passwordsValidated === false;
      }
      return true;
    },
    nextStep() {
      switch (this.current) {
        case 2:
          this.$nextTick(() => {
            this.$refs.password.$el.focus();
          });
          break;
        case 3:
          if (
            UnityBackend.InitWalletFromRecoveryPhrase(
              this.recoveryPhrase,
              this.password1
            )
          ) {
            this.$router.push({ name: "wallet" });
          }
          break;
      }
      this.current++;
    },
    previousStep() {
      if (this.current === 2) {
        this.current--;
      }
    },
    onPassword2Keyup() {
      if (event.keyCode === 13 && this.passwordsValidated) this.nextStep();
    },
    onPhraseValidated() {
      setTimeout(() => {
        this.isRecoveryPhraseCorrect = true;
      }, 1000);
    }
  }
};
</script>

<style lang="less" scoped>
.button-wrapper {
  margin: 10px 0 0 0;
  float: right;
}

.phrase {
  padding: 10px;
  font-size: 1.05em;
  font-weight: 500;
  text-align: center;
  word-spacing: 4px;
  background-color: #f5f5f5;
}

.phrase,
.password,
.password-row {
  margin: 0 0 20px 0;
}

.important {
  color: #dd3333;
}
</style>
