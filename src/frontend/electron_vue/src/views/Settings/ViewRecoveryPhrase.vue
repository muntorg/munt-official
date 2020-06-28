<template>
  <div class="view-recovery-phrase-view">
    <h2>
      <span v-if="current === 1">{{ $t("setup.enter_your_password") }}</span>
      <span class="important" v-else>{{ $t("common.important") }}</span>
    </h2>

    <!-- step 1: Enter password -->
    <novo-form-field :title="$t('common.password')" v-if="current === 1">
      <input
        ref="password"
        type="password"
        v-model="password"
        @keydown="validatePasswordOnEnter"
        :class="computedStatus"
      />
    </novo-form-field>

    <!-- step 2: Show recovery phrase -->
    <div v-else>
      <p>{{ $t("setup.this_is_your_recovery_phrase") }}</p>
      <novo-section class="phrase">
        {{ recoveryPhrase }}
      </novo-section>
    </div>

    <novo-button-section>
      <button
        v-if="current === 1"
        @click="validatePassword"
        :disabled="isNextDisabled"
      >
        {{ $t("buttons.next") }}
      </button>
    </novo-button-section>
  </div>
</template>

<script>
import UnityBackend from "../../unity/UnityBackend";

export default {
  data() {
    return {
      current: 1,
      recoveryPhrase: null,
      password: "",
      isPasswordInvalid: false
    };
  },
  mounted() {
    this.$refs.password.focus();
  },
  computed: {
    computedStatus() {
      return this.isPasswordInvalid ? "error" : "";
    },
    isNextDisabled() {
      return this.password.trim().length === 0;
    }
  },
  methods: {
    validatePasswordOnEnter() {
      this.isPasswordInvalid = false;
      if (event.keyCode === 13) this.validatePassword();
    },
    validatePassword() {
      if (UnityBackend.UnlockWallet(this.password)) {
        this.recoveryPhrase = UnityBackend.GetRecoveryPhrase();
        UnityBackend.LockWallet();
        this.current++;
      } else {
        this.isPasswordInvalid = true;
      }
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
