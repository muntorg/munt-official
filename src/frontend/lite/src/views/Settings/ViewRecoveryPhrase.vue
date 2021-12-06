<template>
  <div class="view-recovery-phrase-view flex-col">
    <h2>
      <span v-if="current === 1">{{ $t("common.enter_your_password") }}</span>
      <span class="important" v-else>{{ $t("common.important") }}</span>
    </h2>

    <!-- step 1: Enter password -->
    <app-form-field v-if="current === 1">
      <input
        ref="password"
        type="password"
        v-model="password"
        @keydown="getRecoveryPhraseOnEnter"
        :class="computedStatus"
      />
    </app-form-field>

    <!-- step 2: Show recovery phrase -->
    <div v-else>
      <p>{{ $t("setup.this_is_your_recovery_phrase") }}</p>
      <app-section class="phrase">
        {{ recoveryPhrase }}
      </app-section>
    </div>

    <div class="flex-1" />

    <app-button-section>
      <template v-slot:right>
        <button
          v-if="current === 1"
          @click="getRecoveryPhrase"
          :disabled="isNextDisabled"
        >
          {{ $t("buttons.next") }}
        </button>
        <button v-if="current === 2" @click="ready">
          {{ $t("buttons.ready") }}
        </button>
      </template>
    </app-button-section>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { LibraryController } from "../../unity/Controllers";

export default {
  data() {
    return {
      recoveryPhrase: null,
      password: "",
      isPasswordInvalid: false
    };
  },
  mounted() {
    if (this.walletPassword) {
      this.password = this.walletPassword;
      this.getRecoveryPhrase();
    } else {
      this.$refs.password.focus();
    }
  },
  computed: {
    ...mapState("wallet", ["walletPassword"]),
    current() {
      return this.recoveryPhrase ? 2 : 1;
    },
    computedStatus() {
      return this.isPasswordInvalid ? "error" : "";
    },
    isNextDisabled() {
      return this.password.trim().length === 0;
    }
  },
  methods: {
    getRecoveryPhraseOnEnter() {
      this.isPasswordInvalid = false;
      if (event.keyCode === 13) this.getRecoveryPhrase();
    },
    getRecoveryPhrase() {
      if (LibraryController.UnlockWallet(this.password)) {
        this.recoveryPhrase = LibraryController.GetRecoveryPhrase().phrase;
        LibraryController.LockWallet();
      } else {
        this.isPasswordInvalid = true;
      }
    },
    ready() {
      this.$router.push({ name: "settings" });
    }
  }
};
</script>

<style lang="less" scoped>
.view-recovery-phrase-view {
  height: 100%;
}

.phrase {
  padding: 15px;
  font-size: 1.05em;
  font-weight: 500;
  text-align: center;
  word-spacing: 4px;
  background-color: #f5f5f5;
  user-select: none;
}
</style>
