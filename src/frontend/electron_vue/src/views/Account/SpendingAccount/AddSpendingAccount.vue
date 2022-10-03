<template>
  <div class="add-spending-account">
    <portal to="header-slot">
      <main-header title="add_spending_account.title"></main-header>
    </portal>

    <section class="content">
      <section>
        <app-form-field title="common.account_name">
          <input type="text" v-model="accountName" maxlength="30" ref="accountName" />
        </app-form-field>
      </section>
    </section>
    <div class="flex-1"></div>
    <app-button-section>
      <button @click="tryAddSpendingAccount" :disabled="accountName.length < 3">
        {{ $t("buttons.done") }}
      </button>
    </app-button-section>
  </div>
</template>

<script>
import { mapGetters } from "vuex";
import { AccountsController } from "../../../unity/Controllers";
import EventBus from "../../../EventBus";

export default {
  name: "AddSpendingAccount",
  data() {
    return {
      accountName: ""
    };
  },
  computed: {
    ...mapGetters("wallet", ["accounts"]),
    disableLockButton() {
      if (this.accountName.trim().length === 0) return true;
      return false;
    }
  },
  methods: {
    tryAddSpendingAccount() {
      EventBus.$emit("unlock-wallet", {
        callback: async () => {
          let accountId = AccountsController.CreateAccount(this.accountName, "HD");

          if (accountId) {
            this.$router.push({ name: "account", params: { id: accountId } });
          } else {
            console.log("Error");
            AccountsController.DeleteAccount(accountId); // something went wrong, so delete the account
          }
        }
      });
    }
  }
};
</script>

<style scoped>
.add-spending-account {
  display: flex;
  flex-direction: column;
}
</style>
