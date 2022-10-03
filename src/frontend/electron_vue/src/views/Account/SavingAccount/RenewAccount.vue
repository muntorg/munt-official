<template>
  <div class="send-coins flex-col">
    <div class="main">
      <app-form-field title="renew_saving_account.funding_account">
        <select-list :options="fundingAccounts" :default="fundingAccount" v-model="fundingAccount" />
      </app-form-field>
    </div>
    <button @click="tryRenew" :disabled="disableSendButton">
      {{ $t("buttons.send") }}
    </button>
  </div>
</template>

<script>
import { mapGetters } from "vuex";
import { AccountsController, WitnessController } from "../../../unity/Controllers";
import EventBus from "../../../EventBus";

export default {
  name: "RenewAccount",
  data() {
    return {
      fundingAccount: null
    };
  },
  computed: {
    ...mapGetters("wallet", ["accounts", "account"]),
    fundingAccounts() {
      return this.accounts.filter(x => x.state === "Normal" && ["Desktop"].indexOf(x.type) !== -1);
    },
    disableSendButton() {
      return this.fundingAccounts.length === 0;
    }
  },
  mounted() {
    if (this.fundingAccounts.length) {
      this.fundingAccount = this.fundingAccounts[0];
    }
  },
  methods: {
    async tryRenew() {
      EventBus.$emit("unlock-wallet", {
        callback: async () => {
          let accountUUID = AccountsController.GetActiveAccount();
          let result = WitnessController.RenewWitnessAccount(this.fundingAccount.UUID, accountUUID);

          if (result.status === "success") {
            this.fundingAccount = null;
          } else {
            // todo: handle error
            console.log(result);
          }
        }
      });
    }
  }
};
</script>

<style lang="less" scoped>
.send-coins {
  height: 100%;

  .main {
    flex: 1;
  }
}

button {
  width: 100%;
}
</style>
