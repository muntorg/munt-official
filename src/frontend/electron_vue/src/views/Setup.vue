<template>
  <div class="setup-view">
    <div class="steps-container">
      <!-- step 1: choose setup type -->
      <novo-section v-if="current === 1">
        <h2>{{ $t("setup.setup_novo_wallet") }}</h2>
        <novo-section>
          <div class="settings-row" @click="isRecovery = false">
            {{ $t("setup.create_new") }}
            <fa-icon :icon="['fal', 'long-arrow-right']" class="arrow" />
          </div>
          <div class="settings-row" @click="isRecovery = true">
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
          @phrase-complete="validatePhrase"
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

      <novo-button-section>
        <template v-slot:left>
          <button v-if="current === 2 || current === 3" @click="previousStep">
            {{ $t("buttons.previous") }}
          </button>
          <button
            v-show="current === 3 && isRecoveryPhraseInvalid === true"
            class="error"
            @click="clearPhraseInput"
          >
            {{ $t("buttons.invalid_recovery_phrase") }}
          </button>
        </template>
        <template v-slot:right>
          <button @click="nextStep" :disabled="!isNextEnabled">
            <span v-show="current === 2">
              {{ $t("buttons.next") }}
            </span>
            <span v-show="current === 4">
              {{ $t("buttons.finish") }}
            </span>
          </button>
        </template>
      </novo-button-section>
    </div>
  </div>
</template>

<script>
import UnityBackend from "../unity/UnityBackend";
import PhraseInput from "../components/PhraseInput";

export default {
  data() {
    return {
      current: 1,
      isRecovery: null,
      recoveryPhrase: "",
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
    isNextEnabled() {
      switch (this.current) {
        case 2:
          return this.recoveryPhrase !== null;
        case 4:
          return this.passwordsValidated;
      }
      return false;
    }
  },
  watch: {
    isRecoveryPhraseCorrect() {
      if (this.isRecoveryPhraseInvalid === false) {
        this.nextStep();
      }
    },
    isRecovery() {
      if (this.isRecovery === null) return;
      this.nextStep();
    }
  },
  methods: {
    clearPhraseInput() {
      this.isRecoveryPhraseInvalid = null;
      this.$refs.phraseinput.clear();
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
          this.$nextTick(() => {
            this.$refs.password.focus();
          });
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
      let previous = this.current - 1;
      switch (this.current) {
        case 2:
        case 3:
          this.isRecovery = null;
          previous = 1;
          break;
      }
      this.current = previous;
    },
    recoverFromPhrase() {
      this.isRecovery = true;
      this.current++;
    },
    validatePhrase(phrase) {
      this.isRecoveryPhraseInvalid = !UnityBackend.IsValidRecoveryPhrase(
        phrase
      );
      if (!this.isRecoveryPhraseInvalid) {
        this.recoveryPhrase = phrase;
      }
    },
    validatePasswordsOnEnter() {
      if (event.keyCode === 13 && this.passwordsValidated) this.nextStep();
    }
  }
};
</script>

<style lang="less" scoped>
.settings-row {
  padding: 4px 0;
  border-bottom: 1px solid #ccc;
  cursor: pointer;
}

.arrow {
  float: right;
  margin: 6px 0 0 0;
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
