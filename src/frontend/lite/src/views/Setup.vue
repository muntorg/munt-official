<template>
  <div class="setup-view flex-col">
    <div class="steps-container">
      <!-- step 1: choose setup type -->
      <content-wrapper heading="setup.setup_your_wallet" v-if="current === 1">
        <div class="settings-row" @click="setupWallet(false)">
          {{ $t("setup.create_new") }}
          <fa-icon :icon="['fal', 'chevron-right']" class="arrow" />
        </div>
        <div class="settings-row" @click="setupWallet(true)">
          {{ $t("setup.recover_existing") }}
          <fa-icon :icon="['fal', 'chevron-right']" class="arrow" />
        </div>
      </content-wrapper>

      <!-- step 2: show recovery phrase -->
      <content-wrapper heading="common.important" headingStyle="warning" content="setup.this_is_your_recovery_phrase" v-else-if="current === 2">
        <app-section class="phrase">
          {{ recoveryPhrase }}
        </app-section>
      </content-wrapper>

      <!-- step 3: enter/repeat recovery phrase -->
      <content-wrapper
        heading="setup.enter_recovery_phrase"
        :content="isRecovery ? 'setup.this_is_your_recovery_phrase' : 'setup.enter_existing_recovery_phrase'"
        v-else-if="current === 3"
      >
        <phrase-input
          ref="phraseinput"
          :validate="validate"
          :autofocus="true"
          :isPhraseInvalid="isRecoveryPhraseInvalid === true"
          :reset="reset"
          @possible-phrase="onPossiblePhrase"
          @enter="validatePhraseOnEnter"
        />
      </content-wrapper>

      <!-- step 4: enter a password -->
      <content-wrapper heading="setup.choose_password" content="setup.choose_password_information" v-else-if="current === 4">
        <app-form-field title="common.password">
          <input ref="password" type="password" v-model="password1" />
        </app-form-field>
        <app-form-field title="setup.repeat_password">
          <input type="password" v-model="password2" :class="password2Status" @keydown="validatePasswordsOnEnter" />
        </app-form-field>
      </content-wrapper>
    </div>

    <app-button-section class="steps-buttons">
      <template slot:left>
        <button v-if="showPreviousButton" @click="previousStep">
          {{ $t("buttons.previous") }}
        </button>
      </template>
      <template v-slot:right>
        <button @click="nextStep" :disabled="!isNextEnabled">
          <span v-show="showNextButton">
            {{ $t("buttons.next") }}
          </span>
          <span v-show="showFinishButton">
            {{ $t("buttons.finish") }}
          </span>
        </button>
        <button @click="nextStep" v-show="showValidateButton" :disabled="!isValidateButtonEnabled">
          {{ $t("buttons.next") }}
        </button>
      </template>
    </app-button-section>
  </div>
</template>

<script>
import { LibraryController } from "../unity/Controllers";
import PhraseInput from "../components/PhraseInput";
import EventBus from "../EventBus.js";
import AppStatus from "../AppStatus";
import ContentWrapper from "../components/layout/ContentWrapper.vue";

export default {
  data() {
    return {
      current: 1,
      isRecovery: null,
      recoveryPhrase: "",
      possiblePhrase: null,
      isRecoveryPhraseInvalid: null,
      generatedRecoveryPhrase: null,
      password1: "",
      password2: "",
      reset: false
    };
  },
  components: {
    PhraseInput,
    ContentWrapper
  },
  computed: {
    validate() {
      if (this.isRecovery) {
        return {
          length: 12,
          words: LibraryController.GetMnemonicDictionary()
        };
      } else {
        return this.recoveryPhrase;
      }
    },
    password2Status() {
      if (this.password2.length === 0) return "";
      if (this.password2.length > this.password1.length) return "error";
      return this.password1.indexOf(this.password2) === 0 ? "" : "error";
    },
    passwordsValidated() {
      if (this.password1 === null || this.password1.length < 6) return false;
      if (this.password2 === null || this.password2.length < this.password1.length) return false;

      return this.password1 === this.password2;
    },
    showPreviousButton() {
      return this.current === 2 || this.current === 3;
    },
    showNextButton() {
      return this.current === 2;
    },
    showValidateButton() {
      return this.current === 3;
    },
    showFinishButton() {
      return this.current === 4;
    },
    isNextEnabled() {
      switch (this.current) {
        case 2:
          return this.recoveryPhrase !== null;
        case 4:
          return this.passwordsValidated;
      }
      return false;
    },
    isValidateButtonEnabled() {
      return this.possiblePhrase !== null && this.isRecoveryPhraseInvalid !== true;
    },
    validateButtonClass() {
      return this.isRecoveryPhraseInvalid ? "error" : "";
    }
  },
  watch: {
    current() {
      let current = this.current;

      this.$nextTick(() => {
        switch (current) {
          case 1:
            this.isRecovery = null;
            break;
          case 4:
            this.$refs.password.focus();
            break;
        }
      });
    }
  },
  methods: {
    setupWallet(isRecovery) {
      this.isRecovery = isRecovery;
      this.nextStep();
    },
    nextStep() {
      let next = this.current + 1;

      switch (this.current) {
        case 1:
          if (this.isRecovery) {
            this.recoveryPhrase = "";
            next = 3;
          } else {
            if (this.generatedRecoveryPhrase === null) {
              var mnemonic = LibraryController.GenerateRecoveryMnemonic();
              this.generatedRecoveryPhrase = mnemonic.phrase_with_birth_number;
              this.recoveryPhrase = mnemonic.phrase;
            }
          }
          break;
        case 3:
          if (LibraryController.IsValidRecoveryPhrase(this.possiblePhrase)) {
            this.recoveryPhrase = this.possiblePhrase;
          } else {
            EventBus.$emit("show-dialog", {
              type: "error",
              title: this.$t("setup.invalid_recovery_phrase.title"),
              message: this.$t("setup.invalid_recovery_phrase.message")
            });

            this.isRecoveryPhraseInvalid = true;
            return;
          }
          break;
        case 4:
          if (this.isRecovery) {
            if (LibraryController.InitWalletFromRecoveryPhrase(this.recoveryPhrase, this.password1)) {
              this.$store.dispatch("app/SET_STATUS", AppStatus.synchronize);
            }
          } else {
            if (LibraryController.InitWalletFromRecoveryPhrase(this.generatedRecoveryPhrase, this.password1)) {
              this.$store.dispatch("app/SET_STATUS", AppStatus.synchronize);
            }
          }
          break;
      }
      this.current = next;
    },
    previousStep() {
      if (this.current === 3 && this.isRecovery) this.current = 1;
      else this.current--;
    },
    recoverFromPhrase() {
      this.isRecovery = true;
      this.nextStep();
    },
    onPossiblePhrase(phrase) {
      this.isRecoveryPhraseInvalid = null;
      this.possiblePhrase = phrase;
    },
    validatePhraseOnEnter() {
      if (this.possiblePhrase !== null) {
        this.nextStep();
      }
    },
    validatePasswordsOnEnter() {
      if (event.keyCode === 13 && this.passwordsValidated) this.nextStep();
    }
  }
};
</script>

<style lang="less" scoped>
.setup-view {
  height: 100%;
}

.steps-container {
  flex: 1;
}

.settings-row {
  margin: 0 -10px;
  padding: 10px;
  cursor: pointer;
}

.settings-row:hover {
  color: var(--primary-color);
  background-color: var(--hover-color);
}

.arrow {
  float: right;
  color: #000;
}

.settings-row:hover > .arrow {
  color: #000;
}

.phrase {
  padding: 10px;
  font-size: 1.05em;
  font-weight: 500;
  text-align: center;
  word-spacing: 4px;
  background-color: #f5f5f5;
}
</style>
