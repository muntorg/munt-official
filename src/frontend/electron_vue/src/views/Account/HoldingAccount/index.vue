<template>
  <account-page-layout
    class="holding-account"
    :right-section="rightSectionComponent"
    @close-right-section="closeRightSection"
  >
    <template v-slot:header>
      <section class="header flex-row">
        <div class="info">
          <div class="label ellipsis">{{ account.label }}</div>
          <div class="balance ellipsis">{{ account.balance }}</div>
        </div>
        <div class="settings flex-col" v-if="false">
          <span>
            <fa-icon :icon="['fal', 'cog']" />
          </span>
        </div>
      </section>
    </template>
  </account-page-layout>
</template>

<script>
import AccountPageLayout from "../AccountPageLayout";

export default {
  name: "HoldingAccount",
  props: {
    account: null
  },
  data() {
    return {
      rightSection: null,
      rightSectionComponent: null
    };
  },
  components: {
    AccountPageLayout
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
::v-deep {
  & .header {
    & > .info {
      width: calc(100% - 48px - 26px);
      padding-right: 10px;

      & > .label {
        font-size: 1.1em;
        font-weight: 500;
        line-height: 20px;
      }

      & > .balance {
        line-height: 20px;
      }
    }

    & > .settings {
      font-size: 16px;

      & span {
        padding: 10px;
        cursor: pointer;

        &:hover {
          background: #f5f5f5;
        }
      }
    }
  }

  & .footer {
    text-align: center;
  }
}
</style>
