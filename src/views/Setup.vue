<template>
  <div id="setup-container">
    <div class="steps-container section">
      <!-- step 1: show recovery phrase -->
      <div v-if="current === 1">
        <h2>{{ $t("setup.step1.header") }}</h2>
        <div class="phrase">
          {{ recoveryPhrase }}
        </div>
      </div>

      <!-- step 2: repeat recovery phrase -->
      <div v-else-if="current === 2">
        <h2>{{ $t("setup.step2.header") }}</h2>
        <div class="phrase-repeat">
          <phrase-repeat-input ref="firstWord" :word="recoveryPhraseWords[0]" placeholder="word 1" @match-changed="onMatchChanged" />
          <phrase-repeat-input :word="recoveryPhraseWords[1]" placeholder="word 2" @match-changed="onMatchChanged" />
          <phrase-repeat-input :word="recoveryPhraseWords[2]" placeholder="word 3" @match-changed="onMatchChanged" />
          <phrase-repeat-input :word="recoveryPhraseWords[3]" placeholder="word 4" @match-changed="onMatchChanged" />
          <phrase-repeat-input :word="recoveryPhraseWords[4]" placeholder="word 5" @match-changed="onMatchChanged" />
          <phrase-repeat-input :word="recoveryPhraseWords[5]" placeholder="word 6" @match-changed="onMatchChanged" />
          <phrase-repeat-input :word="recoveryPhraseWords[6]" placeholder="word 7" @match-changed="onMatchChanged" />
          <phrase-repeat-input :word="recoveryPhraseWords[7]" placeholder="word 8" @match-changed="onMatchChanged" />
          <phrase-repeat-input :word="recoveryPhraseWords[8]" placeholder="word 9" @match-changed="onMatchChanged" />
          <phrase-repeat-input :word="recoveryPhraseWords[9]" placeholder="word 10" @match-changed="onMatchChanged" />
          <phrase-repeat-input :word="recoveryPhraseWords[10]" placeholder="word 11" @match-changed="onMatchChanged" />
          <phrase-repeat-input :word="recoveryPhraseWords[11]" placeholder="word 12" @match-changed="onMatchChanged" />
        </div>
      </div>

      <!-- step 3: enter a password -->
      <div v-else-if="current === 3">
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
        v-if="current <= 3"
        @click="nextStep"
        :disabled="isNextDisabled()"
      >
        <span v-if="current < 3">
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
      initialized: false
    };
  },
  async mounted() {
    this.recoveryPhrase = await NovoBackend.GenerateRecoveryMnemonicAsync();
  },
  computed: {
    recoveryPhraseWords() {
      return this.recoveryPhrase === null ? [] : this.recoveryPhrase.split(" ");
    },
    isRecoveryPhraseCorrect() {
      return (
        this.matchingWords > 0 &&
        this.matchingWords === this.recoveryPhraseWords.length
      );
    }
  },
  methods: {
    isNextDisabled() {
      switch (this.current) {
        case 1:
          return this.recoveryPhrase === null;
        case 2:
          return this.isRecoveryPhraseCorrect === false;
        case 3:
          return this.calculatePassword2Status() !== "success";
      }
      return true;
    },
    nextStep() {
      switch (this.current) {
        case 1:
          setTimeout(() => {
            this.$refs.firstWord.$el.focus();
          }, 100);
          break;
        case 3:
          if (this.initialized) return;
          this.initialized = true;
          if (
            NovoBackend.InitWalletFromRecoveryPhrase(
              this.recoveryPhrase,
              this.password1
            )
          ) {
            this.$router.push({ name: "wallet" }); // maybe also route from backend
          }
          break;
      }
      this.current++;
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
