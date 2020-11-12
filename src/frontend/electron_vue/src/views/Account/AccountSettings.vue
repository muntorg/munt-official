<template>
  <div class="account-settings flex-col">
    <portal to="sidebar-right-title">
      {{ $t("account_settings.title") }}
    </portal>

    <div class="main">
      <h5>{{ $t("account_settings.name") }}</h5>
      <input type="text" v-model="accountName">
    </div>
  </div>
</template>

<script>
import {
  AccountsController
} from "../../unity/Controllers";

export default {
  name: "AccountSettings",
  props: {
    account: null,
    accountName: null
  },
  created() {
    this.accountName = this.account.label
  },
  watch: {
    account() {
      this.accountName = this.account.label
    },
    accountName() {
       if (this.accountName && this.accountName != this.account.label )
       {
           AccountsController.RenameAccount(this.account.UUID, this.accountName)
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

  .tx-address {
    margin: 0 0 20px 0;
    padding: 10px;
    font-size: 0.9em;
    line-height: 1.2em;
    user-select: text;
    word-wrap: break-word;
    background-color: #fff;
  }

  .tx-hash {
    padding: 10px;
    font-size: 0.9em;
    line-height: 1.2em;
    user-select: text;
    word-wrap: break-word;
    background-color: #fff;
  }
}
</style>
