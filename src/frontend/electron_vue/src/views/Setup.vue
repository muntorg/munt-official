<template>
  <div class="setup-view">
    <div class="steps-container">
      <!-- step 1: choose setup type -->
      <novo-section v-if="current === 1">
        <h2>{{ $t("setup.setup_novo_wallet") }}</h2>
        <novo-section>
          <div class="settings-row" @click="setupWallet(false)">
            {{ $t("setup.create_new") }}
            <fa-icon :icon="['fal', 'long-arrow-right']" class="arrow" />
          </div>
          <div class="settings-row" @click="setupWallet(true)">
            {{ $t("setup.recover_existing") }}
            <fa-icon :icon="['fal', 'long-arrow-right']" class="arrow" />
          </div>
        </novo-section>
      </novo-section>

      <!-- step 2: show recovery phrase -->
      <novo-section v-else-if="current === 2">
        <h2 class="important">{{ $t("common.important") }}</h2>
        <p>{{ $t("setup.this_is_your_recovery_phrase") }}</p>
        <novo-section class="phrase">
          {{ recoveryPhrase }}
        </novo-section>
      </novo-section>

      <!-- step 3: enter/repeat recovery phrase -->
      <novo-section v-else-if="current === 3">
        <h2>{{ $t("setup.enter_recovery_phrase") }}</h2>
        <p v-if="!isRecovery">{{ $t("setup.repeat_your_recovery_phrase") }}</p>
        <p v-else>{{ $t("setup.enter_existing_recovery_phrase") }}</p>
        <phrase-input
          ref="phraseinput"
          :validate="validate"
          :autofocus="true"
          :isPhraseInvalid="isRecoveryPhraseInvalid === true"
          :reset="reset"
          @possible-phrase="onPossiblePhrase"
          @enter="validatePhraseOnEnter"
        />
      </novo-section>

      <!-- step 4: enter a password -->
      <div v-else-if="current === 4">
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
            @keydown="validatePasswordsOnEnter"
          />
        </novo-form-field>
      </div>
    </div>
    <novo-button-section class="steps-buttons">
      <template v-slot:left>
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
        <button
          @click="nextStep"
          v-show="showValidateButton"
          :disabled="!isValidateButtonEnabled"
        >
          {{ $t("buttons.next") }}
        </button>
      </template>
    </novo-button-section>
  </div>
</template>

<script>
import UnityBackend from "../unity/UnityBackend";
import PhraseInput from "../components/PhraseInput";
import EventBus from "../EventBus.js";

export default {
  data() {
    return {
      current: 1,
      isRecovery: null,
      recoveryPhrase: "",
      possiblePhrase: null,
      isRecoveryPhraseInvalid: null,
      password1: "",
      password2: "",
      reset: false
    };
  },
  components: {
    PhraseInput
  },
  computed: {
    validate() {
      if (this.isRecovery) {
        return {
          length: 12,
          words: UnityBackend.GetMnemonicDictionary()
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
      if (
        this.password2 === null ||
        this.password2.length < this.password1.length
      )
        return false;

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
      return (
        this.possiblePhrase !== null && this.isRecoveryPhraseInvalid !== true
      );
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
            this.recoveryPhrase = UnityBackend.GenerateRecoveryMnemonic();
          }
          break;
        case 3:
          if (UnityBackend.IsValidRecoveryPhrase(this.possiblePhrase)) {
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
  display: flex;
  flex-direction: column;
}

.steps-container {
  flex: 1;
}

.settings-row {
  padding: 4px 0;
  border-bottom: 1px solid #ccc;
  cursor: pointer;
}

.arrow {
  float: right;
  color: #fff;
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
