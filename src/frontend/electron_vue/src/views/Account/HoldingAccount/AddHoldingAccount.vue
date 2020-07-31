<template>
  <div class="add-holding-account">
    <portal to="header-slot">
      <main-header :title="$t('add_holding_account.title')"></main-header>
    </portal>

    <section class="content">
      <novo-form-field title="funding account">
        <select>
          <option v-for="account in fundingAccounts" :key="account.UUID">{{
            account.label
          }}</option>
        </select>
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
