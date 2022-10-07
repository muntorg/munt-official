<template>
  <div class="change-password-view">
    <content-wrapper heading="common.enter_your_password">
      <app-form-field>
        <input ref="currentPassword" type="password" v-model="currentPassword" @keydown="resetStatus" :class="currentPasswordStatus" />
      </app-form-field>
    </content-wrapper>

    <content-wrapper>
      <app-form-field title="setup.choose_password">
        <input ref="password1" type="password" v-model="password1" :class="password2Status" />
      </app-form-field>
      <app-form-field title="setup.repeat_password">
        <input type="password" v-model="password2" :class="password2Status" @keydown="validatePasswordRepeatOnEnter" />
      </app-form-field>
    </content-wrapper>

    <div class="flex-1" />
    <portal v-if="!isSingleAccount" to="footer-slot">
      <app-button-section>
        <button @click="tryChangePassword" :disabled="isButtonDisabled">
          {{ $t("buttons.change_password") }}
        </button>
      </app-button-section>
    </portal>
    <app-button-section v-else>
      <template v-slot:left>
        <button @click="routeTo('settings')">
          {{ $t("buttons.back") }}
        </button>
      </template>
      <template v-slot:right>
        <button @click="tryChangePassword" :disabled="isButtonDisabled">
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
      currentPassword: "",
      password1: "",
      password2: "",
      isCurrentPasswordInvalid: false,
      isSingleAccount: UIConfig.isSingleAccount
    };
  },
  computed: {
    ...mapState("wallet", ["walletPassword"]),
    currentPasswordStatus() {
      return this.isCurrentPasswordInvalid ? "error" : "";
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
    isButtonDisabled() {
      return this.currentPassword.trim().length === 0 || this.passwordsValidated === false;
    }
  },
  mounted() {
    this.$refs.currentPassword.focus();
  },
  methods: {
    tryChangePassword() {
      if (LibraryController.UnlockWallet(this.currentPassword, 10)) {
        if (LibraryController.ChangePassword(this.currentPassword, this.password2)) {
          LibraryController.LockWallet();
          this.routeTo("account");
        }
      } else {
        this.isCurrentPasswordInvalid = true;
      }
    },
    async resetStatus() {
      this.isCurrentPasswordInvalid = false;
    },
    validatePasswordRepeatOnEnter(e) {
      if (e.keyCode === 13 && this.passwordsValidated) this.tryChangePassword();
    },
    routeTo(route) {
      this.$router.push({ name: route });
    }
  }
};
</script>

<style lang="less" scoped>
.change-password-view {
  display: flex;
  height: 100%;
  flex-direction: column;
  flex-wrap: nowrap;
  justify-content: space-between;
}
</style>
