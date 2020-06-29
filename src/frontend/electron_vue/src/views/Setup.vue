<template>
  <div class="setup-view">
    <div class="steps-container">
      <!-- step 1: show recovery phrase -->
      <novo-section v-if="current === 1">
        <h2 class="important">{{ $t("common.important") }}</h2>
        <p>{{ $t("setup.this_is_your_recovery_phrase") }}</p>
        <novo-section class="phrase">
          {{ recoveryPhrase }}
        </novo-section>
      </novo-section>

      <!-- step 2: repeat recovery phrase -->
      <novo-section v-else-if="current === 2">
        <h2>{{ $t("setup.enter_recovery_phrase") }}</h2>
        <p>{{ $t("setup.repeat_your_recovery_phrase") }}</p>
        <phrase-validator
          :phrase="recoveryPhrase"
          :isRecovery="isRecovery"
          :autofocus="true"
          @validated="onPhraseValidated"
        />
      </novo-section>

      <!-- step 3: enter a password -->
      <div v-else-if="current === 3">
        <h2>{{ $t("setup.choose_password") }}</h2>
        <p>{{ $t("setup.choose_password_information") }}</p>
        <novo-form-field :title="$t('common.password')">
          <input ref="password" type="password" v-model="password1" />
        </novo-form-field>
        <novo-form-field :title="$t('setup.repeat_password')">
          <input
            type="password"
            v-model="password2"
            :class="password2Status"
            @keyup="onPassword2Keyup"
          />
        </novo-form-field>
      </div>

      <novo-button-section>
        <template v-slot:left>
          <button
            v-if="current === 2"
            @click="previousStep"
            :disabled="isButtonDisabled()"
          >
            {{ $t("buttons.back") }}
          </button>
          <button v-if="current === 1" @click="recoverFromPhrase">
            {{ "Recover from phrase" }}
          </button>
        </template>
        <template v-slot:right>
          <button
            v-if="current === 1"
            @click="nextStep"
            :disabled="isButtonDisabled()"
          >
            {{ $t("buttons.next") }}
          </button>

          <button
            v-else-if="current === 3"
            @click="nextStep"
            :disabled="isButtonDisabled()"
          >
            {{ $t("buttons.finish") }}
          </button>
        </template>
      </novo-button-section>
    </div>
  </div>
</template>

<script>
import UnityBackend from "../unity/UnityBackend";
import PhraseValidator from "../components/PhraseValidator";

export default {
  data() {
    return {
      current: 1,
      password1: "",
      password2: "",
      recoveryPhrase: "",
      isRecoveryPhraseCorrect: false,
      isBackDisabled: false,
      isRecovery: false
    };
  },
  components: {
    PhraseValidator
  },
  created() {
    this.$nextTick(() => {
      this.recoveryPhrase = UnityBackend.GenerateRecoveryMnemonic();
    });
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
          return this.isBackDisabled;
        case 3:
          return this.passwordsValidated === false;
      }
      return true;
    },
    nextStep() {
      switch (this.current) {
        case 2:
          this.$nextTick(() => {
            this.$refs.password.focus();
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
    recoverFromPhrase() {
      this.isRecovery = true;
      this.current++;
    },
    onPassword2Keyup() {
      if (event.keyCode === 13 && this.passwordsValidated) this.nextStep();
    },
    onPhraseValidated(validatedPhrase) {
      console.log("<<<INITWITHPHRASE:"+validatedPhrase);
      if (this.isRecovery) this.recoveryPhrase = validatedPhrase;
      this.isBackDisabled = true;
      this.$nextTick(() => {
        setTimeout(() => {
          this.isRecoveryPhraseCorrect = true;
        }, 1000);
      });
    }
  }
};
</script>

<style lang="less" scoped>
.phrase {
  padding: 10px;
  font-size: 1.05em;
  font-weight: 500;
  text-align: center;
  word-spacing: 4px;
  background-color: #f5f5f5;
}
</style>
