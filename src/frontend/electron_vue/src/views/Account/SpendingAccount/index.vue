<template>
  <account-page-layout
    class="spending-account"
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

    <mutation-list :mutations="mutations" />

    <template v-slot:footer>
      <section class="footer">
        <span
          class="button"
          @click="setRightSection('Send')"
          v-if="rightSection !== 'Send'"
        >
          <fa-icon :icon="['fal', 'arrow-from-bottom']" />
          {{ $t("buttons.send") }}
        </span>
        <span
          class="button"
          @click="setRightSection('Receive')"
          v-if="rightSection !== 'Receive'"
        >
          <fa-icon :icon="['fal', 'arrow-to-bottom']" />
          {{ $t("buttons.receive") }}
        </span>
      </section>
    </template>
  </account-page-layout>
</template>

<script>
import { mapState } from "vuex";
import AccountPageLayout from "../AccountPageLayout";

import MutationList from "./MutationList";
import SendNovo from "./SendNovo";
import ReceiveNovo from "./ReceiveNovo";

export default {
  name: "SpendingAccount",
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
    AccountPageLayout,
    MutationList
  },
  computed: {
    ...mapState(["mutations"])
  },
  methods: {
    closeRightSection() {
      this.rightSection = null;
      this.rightSectionComponent = null;
    },
    setRightSection(name) {
      this.rightSection = name;
      switch (name) {
        case "Send":
          this.rightSectionComponent = {
            title: this.$t("buttons.send"),
            component: SendNovo
          };
          break;
        case "Receive":
          this.rightSectionComponent = {
            title: this.$t("buttons.receive"),
            component: ReceiveNovo
          };
          break;
      }
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

    & svg {
      font-size: 14px;
      margin-right: 5px;
    }

    & .button {
      display: inline-block;
      padding: 0 20px 0 20px;
      line-height: 32px;
      font-weight: 500;
      font-size: 1em;
      color: var(--primary-color);
      text-align: center;
      cursor: pointer;

      &:hover {
        background-color: #f5f5f5;
      }
    }
  }
}
</style>
