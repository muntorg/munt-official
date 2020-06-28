<template>
  <div class="view-recovery-phrase-view">
    <div class="section">
      <div class="back">
        <router-link :to="{ name: current === 1 ? 'settings' : 'wallet' }">
          <fa-icon :icon="['fal', 'long-arrow-left']" />
          <span> {{ $t("buttons.back") }}</span>
        </router-link>
      </div>
      <h2>
        <span v-if="current === 1">{{ $t("setup.enter_your_password") }}</span>
        <span class="important" v-else>{{ $t("common.important") }}</span>
      </h2>

      <!-- step 1: Enter password -->
      <div class="password" v-if="current === 1">
        <div class="password-row">
          <h4>{{ $t("common.password") }}:</h4>
          <input
            ref="password"
            type="password"
            v-model="password"
            @keydown="validatePasswordOnEnter"
            :class="computedStatus"
          />
        </div>
      </div>

      <!-- step 2: Show recovery phrase -->
      <div v-else>
        <p>{{ $t("setup.this_is_your_recovery_phrase") }}</p>
        <div class="phrase">
          {{ recoveryPhrase }}
        </div>
      </div>

      <div class="button-wrapper">
        <button
          class="btn"
          v-if="current === 1"
          @click="validatePassword"
          :disabled="isNextDisabled"
        >
          {{ $t("buttons.next") }}
        </button>
      </div>
    </div>
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
.back a {
  padding: 4px 8px;
  margin: 0 0 0 -8px;
}
.back {
  margin-bottom: 20px;
}

.back a:hover {
  background-color: #f5f5f5;
}

.button-wrapper {
  margin: 10px 0 0 0;
  float: right;
}

.important {
  color: #dd3333;
}

.phrase {
  padding: 10px;
  font-size: 1.05em;
  font-weight: 500;
  text-align: center;
  word-spacing: 4px;
  background-color: #f5f5f5;
}

.password {
  margin: 0 0 20px 0;
}

.password-row {
  margin: 0 0 20px 0;
}
</style>
