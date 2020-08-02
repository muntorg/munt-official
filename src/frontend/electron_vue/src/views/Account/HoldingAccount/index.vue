<template>
  <div class="holding-account">
    <portal to="header-slot">
      <section class="header flex-row">
        <main-header
          class="info"
          :title="account.label"
          :subtitle="account.balance"
        />
        <div class="settings flex-col" v-if="false /* not implemented yet */">
          <span>
            <fa-icon :icon="['fal', 'cog']" />
          </span>
        </div>
      </section>
    </portal>

    <pre>{{ statistics }}</pre>

    <portal to="footer-slot">
      <div />
    </portal>
  </div>
</template>

<script>
import UnityBackend from "../../../unity/UnityBackend";
export default {
  name: "HoldingAccount",
  props: {
    account: null
  },
  data() {
    return {
      rightSection: null,
      rightSectionComponent: null,
      statistics: null
    };
  },
  created() {
    this.statistics = UnityBackend.GetAccountWitnessStatistics(
      this.account.UUID
    );
  },
  methods: {
    closeRightSection() {
      this.rightSection = null;
      this.rightSectionComponent = null;
    }
  }
};
</script>

<style lang="less" scoped>
.header {
  & > .info {
    width: calc(100% - 26px);
    padding-right: 10px;
  }

  & > .settings {
    font-size: 16px;
    padding: calc((var(--header-height) - 40px) / 2) 0;

    & span {
      padding: 10px;
      cursor: pointer;

      &:hover {
        background: #f5f5f5;
      }
    }
  }
}
</style>
