<template>
  <div class="view-recovery-phrase-view">
    <div>
      <!-- step 1: Enter password -->
      <content-wrapper v-if="current === 1" heading="common.enter_your_password">
        <app-form-field>
          <input ref="password" type="password" v-model="password" @keydown="getRecoveryPhraseOnEnter" :class="computedStatus" />
        </app-form-field>
      </content-wrapper>

      <!-- step 2: Show recovery phrase -->
      <content-wrapper v-else heading="common.important" heading-style="warning" content="setup.this_is_your_recovery_phrase">
        <app-section class="phrase">
          {{ recoveryPhrase }}
        </app-section>
      </content-wrapper>
    </div>

    <div v-if="UIConfig.showSidebar">
      <div class="flex-1" />
      <app-button-section>
        <template v-slot:left>
          <button v-if="current === 1" @click="routeTo('settings')">
            {{ $t("buttons.back") }}
          </button>
        </template>
        <template v-slot:right>
          <button v-if="current === 1" @click="getRecoveryPhrase" :disabled="isNextDisabled">
            {{ $t("buttons.next") }}
          </button>
          <button v-if="current === 2" @click="ready">
            {{ $t("buttons.ready") }}
          </button>
        </template>
      </app-button-section>
    </div>
    <div v-else>
      <portal to="footer-slot">
        <app-button-section>
          <button v-if="current === 1" @click="getRecoveryPhrase" :disabled="isNextDisabled">
            {{ $t("buttons.next") }}
          </button>
          <button v-if="current === 2" @click="ready">
            {{ $t("buttons.ready") }}
          </button>
        </app-button-section>
      </portal>
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { LibraryController } from "../../unity/Controllers";
import UIConfig from "../../../ui-config.json";

export default {
  data() {
    return {
      recoveryPhrase: null,
      password: "",
      isPasswordInvalid: false,
      UIConfig: UIConfig
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
    },
    routeTo(route) {
      this.$router.push({ name: route });
    }
  }
};
</script>

<style lang="less" scoped>
.view-recovery-phrase-view {
  display: flex;
  height: 100%;
  flex-direction: column;
  flex-wrap: nowrap;
  justify-content: space-between;
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
