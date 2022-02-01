<template>
  <div class="change-password-view">
    <h2>
      <span v-if="current === 1">{{ $t("common.enter_your_password") }}</span>
      <span v-else>{{ $t("setup.choose_password") }}</span>
    </h2>

    <!-- step 1: Enter old password -->
    <app-form-field v-if="current === 1" :title="$t('common.password')">
      <input
        ref="passwordold"
        type="password"
        v-model="passwordold"
        @keydown="validatePasswordOnEnter"
        :class="passwordOldStatus"
      />
    </app-form-field>

    <!-- step 2: enter new password -->
    <div v-else>
      <app-form-field :title="$t('common.password')">
        <input ref="password1" type="password" v-model="password1" />
      </app-form-field>
      <app-form-field :title="$t('setup.repeat_password')">
        <input
          type="password"
          v-model="password2"
          :class="password2Status"
          @keydown="validatePasswordRepeatOnEnter"
        />
      </app-form-field>
    </div>

    <div class="flex-1" />
    <portal v-if="!UIConfig.showSidebar" to="footer-slot">
      <app-button-section>
        <button
          v-if="current === 1"
          @click="nextStep"
          :disabled="isNextDisabled"
        >
          {{ $t("buttons.next") }}
        </button>
        <button
          v-if="current === 2"
          @click="nextStep"
          :disabled="isNextDisabled"
        >
          {{ $t("buttons.change_password") }}
        </button>
      </app-button-section>
    </portal>
    <app-button-section v-else>
      <template v-slot:left>
        <button v-if="current === 1" @click="routeTo('settings')">
          {{ $t("buttons.back") }}
        </button>
      </template>
      <template v-slot:right>
        <button
          v-if="current === 1"
          @click="nextStep"
          :disabled="isNextDisabled"
        >
          {{ $t("buttons.next") }}
        </button>
        <button
          v-if="current === 2"
          @click="nextStep"
          :disabled="isNextDisabled"
        >
          {{ $t("buttons.change_password") }}
        </button>
      </template>
    </app-button-section>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { LibraryController } from "../../unity/Controllers";
import UIConfig from "../../../ui-config.json";

export default {
  data() {
    return {
      current: 1,
      passwordold: "",
      password1: "",
      password2: "",
      isPasswordInvalid: false,
      UIConfig: UIConfig
    };
  },
  computed: {
    ...mapState("wallet", ["walletPassword"]),
    passwordOldStatus() {
      return this.isPasswordInvalid ? "error" : "";
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
    isNextDisabled() {
      switch (this.current) {
        case 1:
          return this.passwordold.trim().length === 0;
        case 2:
          return this.passwordsValidated === false;
      }
      return true;
    }
  },
  mounted() {
    this.$refs.passwordold.focus();
  },
  methods: {
    nextStep() {
      switch (this.current) {
        case 1:
          this.validatePassword();
          break;
        case 2:
          if (
            LibraryController.ChangePassword(this.passwordold, this.password2)
          ) {
            if (this.walletPassword) {
              this.$store.dispatch(
                "wallet/SET_WALLET_PASSWORD",
                this.password2
              );
            }
            this.$router.push({ name: "account" });
          }
          break;
      }
    },
    validatePasswordOnEnter() {
      this.isPasswordInvalid = false;
      if (event.keyCode === 13) this.validatePassword();
    },
    validatePasswordRepeatOnEnter() {
      if (event.keyCode === 13 && this.passwordsValidated) this.nextStep();
    },
    validatePassword() {
      if (LibraryController.UnlockWallet(this.passwordold)) {
        LibraryController.LockWallet();
        this.current++;
        this.$nextTick(() => {
          this.$refs.password1.focus();
        });
      } else {
        this.isPasswordInvalid = true;
      }
    },
    routeTo(route) {
      this.$router.push({ name: route });
    }
  }
};
</script>

<style lang="less" scoped>
.change-password-view {
  height: 100%;
}
</style>
