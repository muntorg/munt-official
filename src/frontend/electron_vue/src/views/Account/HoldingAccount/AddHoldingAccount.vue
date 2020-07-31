<template>
  <div class="add-holding-account">
    <portal to="header-slot">
      <main-header :title="$t('add_holding_account.title')"></main-header>
    </portal>

    <section class="content">
      <novo-form-field title="account name">
        <input type="text" v-model="accountName" />
      </novo-form-field>
      <novo-form-field title="funding account">
        <select>
          <option v-for="account in fundingAccounts" :key="account.UUID">{{
            account.label
          }}</option>
        </select>
      </novo-form-field>
      <novo-form-field title="amount">
        <input type="number" min="50" v-model="amount" />
      </novo-form-field>
      <novo-form-field title="lock time">
        <input type="range" v-model="lockTime" />
      </novo-form-field>
    </section>

    <portal to="footer-slot">
      <novo-button-section>
        <button>{{ $t("buttons.fund") }}</button>
      </novo-button-section>
    </portal>
  </div>
</template>

<script>
import { mapGetters } from "vuex";
export default {
  name: "AddHoldingAccount",
  data() {
    return {
      accountName: "",
      amount: 50,
      lockTime: 1,
      password: ""
    };
  },
  computed: {
    ...mapGetters(["accounts"]),
    fundingAccounts() {
      return this.accounts.filter(
        x => x.state === "Normal" && ["Desktop"].indexOf(x.type) !== -1
      );
    }
  }
};
</script>
