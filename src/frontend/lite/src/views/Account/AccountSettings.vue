<template>
  <div class="account-settings flex-col">
    <portal to="sidebar-right-title">
      {{ $t("account_settings.title") }}
    </portal>

    <div class="main">
      <h5>{{ $t("account_settings.name") }}</h5>
      <input type="text" v-model="newAccountName" />
    </div>
  </div>
</template>

<script>
import { AccountsController } from "../../unity/Controllers";

export default {
  name: "AccountSettings",
  props: {
    account: null
  },
  data() {
    return {
      newAccountName: null
    };
  },
  created() {
    this.newAccountName = this.account.label;
  },
  watch: {
    newAccountName() {
      if (this.newAccountName && this.newAccountName !== this.account.label) {
        AccountsController.RenameAccount(
          this.account.UUID,
          this.newAccountName
        );
      }
    }
  }
};
</script>

<style lang="less" scoped>
.account-settings {
  height: 100%;

  .main {
    flex: 1;
  }
}
</style>
