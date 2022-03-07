<template>
  <div class="holding-account">
    <portal to="header-slot">
      <account-header :account="account"></account-header>
    </portal>

    <app-section v-if="isAccountView && accountIsFunded" class="align-right">
      {{ $t("holding_account.compound_earnings") }}
      <toggle-button
        :value="isCompounding"
        :color="{ checked: '#c0aa70', unchecked: '#ddd' }"
        :labels="{
          checked: $t('common.on'),
          unchecked: $t('common.off')
        }"
        :sync="true"
        :speed="0"
        :height="16"
        :width="44"
        @change="toggleCompounding"
      />
    </app-section>

    <app-section v-if="isAccountView && accountIsFunded" class="holding-information">
      <h5>{{ $t("common.information") }}</h5>
      <div class="flex-row">
        <div>{{ $t("holding_account.status") }}</div>
        <div>{{ accountStatus }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("holding_account.parts") }}</div>
        <div>{{ accountParts }}</div>
      </div>

      <div class="flex-row">
        <div>{{ $t("holding_account.coins_locked") }}</div>
        <div>{{ accountAmountLockedAtCreation }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("holding_account.coins_earned") }}</div>
        <div>{{ accountAmountEarned }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("holding_account.locked_from_block") }}</div>
        <div>{{ lockedFrom }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("holding_account.locked_until_block") }}</div>
        <div>{{ lockedUntil }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("holding_account.lock_duration") }}</div>
        <div>{{ lockDuration }} {{ $t("common.days") }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("holding_account.remaining_lock_period") }}</div>
        <div>{{ remainingLockPeriod }} {{ $t("common.days") }}</div>
      </div>

      <div class="flex-row">
        <div>{{ $t("holding_account.required_earnings_frequency") }}</div>
        <div>{{ requiredEarningsFrequency }} {{ $t("common.days") }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("holding_account.expected_earnings_frequency") }}</div>
        <div>{{ expectedEarningsFrequency }} {{ $t("common.days") }}</div>
      </div>

      <div class="flex-row">
        <div>{{ $t("holding_account.account_weight") }}</div>
        <div>{{ accountWeight }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("holding_account.network_weight") }}</div>
        <div>{{ networkWeight }}</div>
      </div>
    </app-section>

    <app-section v-show="isAccountView && !accountIsFunded" class="holding-empty">
      {{ $t("holding_account.empty") }}
    </app-section>

    <router-view />

    <portal to="footer-slot">
      <footer-button :icon="['fal', 'info-circle']" routeName="account" @click="routeTo">
        {{ $t("buttons.info") }}
      </footer-button>
      <footer-button :icon="['fal', 'key']" routeName="link-holding-account" @click="routeTo">
        {{ $t("buttons.holding_key") }}
      </footer-button>
      <footer-button v-if="renewButtonVisible" :icon="['fal', 'redo-alt']" routeName="renew-account" @click="routeTo">
        {{ $t("buttons.renew") }}
      </footer-button>
      <footer-button :icon="['fal', 'arrow-from-bottom']" routeName="send-holding" @click="routeTo">
        {{ $t("buttons.send") }}
      </footer-button>
    </portal>

    <portal to="sidebar-right">
      <component v-if="rightSidebar" :is="rightSidebar" v-bind="rightSidebarProps" />
    </portal>
  </div>
</template>

<script>
import { WitnessController } from "../../../unity/Controllers";
import { formatMoneyForDisplay } from "../../../util.js";
import EventBus from "../../../EventBus";
import AccountSettings from "../AccountSettings";
import { mapState } from "vuex";

let timeout;

export default {
  name: "HoldingAccount",
  props: {
    account: null
  },
  data() {
    return {
      rightSection: null,
      rightSectionComponent: null,
      statistics: null,
      isCompounding: false,
      rightSidebar: null
    };
  },
  computed: {
    ...mapState("app", ["rate", "activityIndicator"]),
    isAccountView() {
      return this.$route.name === "account";
    },
    accountStatus() {
      return this.getStatistics("account_status");
    },
    accountAmountLockedAtCreation() {
      return formatMoneyForDisplay(this.getStatistics("account_amount_locked_at_creation"));
    },
    accountAmountEarned() {
      let earnings = formatMoneyForDisplay(this.account.balance - this.getStatistics("account_amount_locked_at_creation"));
      if (earnings < 0) return formatMoneyForDisplay(0);
      return earnings;
    },
    accountIsFunded() {
      if (this.getStatistics("account_status") == "empty") return false;
      if (this.getStatistics("account_status") == "empty_with_remainder") return false;
      return true;
    },
    lockedFrom() {
      return this.getStatistics("account_initial_lock_creation_block_height");
    },
    lockedUntil() {
      return this.getStatistics("account_initial_lock_creation_block_height") + this.getStatistics("account_initial_lock_period_in_blocks");
    },
    lockDuration() {
      return (this.getStatistics("account_initial_lock_period_in_blocks") / 288).toFixed(2);
    },
    remainingLockPeriod() {
      return (this.getStatistics("account_remaining_lock_period_in_blocks") / 288).toFixed(2);
    },
    requiredEarningsFrequency() {
      return (this.getStatistics("account_expected_witness_period_in_blocks") / 288).toFixed(2);
    },
    expectedEarningsFrequency() {
      return (this.getStatistics("account_estimated_witness_period_in_blocks") / 288).toFixed(2);
    },
    accountWeight() {
      return this.getStatistics("account_weight");
    },
    networkWeight() {
      return this.getStatistics("network_tip_total_weight");
    },
    accountParts() {
      return this.getStatistics("account_parts");
    },
    rightSidebarProps() {
      if (this.rightSidebar === AccountSettings) {
        return { account: this.account };
      }
      return null;
    },
    renewButtonVisible() {
      return this.accountStatus === "expired";
    },
    totalBalanceFiat() {
      if (!this.rate) return "";
      return `€ ${formatMoneyForDisplay(
        this.account.balance * this.rate,
        true
      )}`;
    },
    balanceForDisplay() {
      if (this.account.balance == null) return "";
      return formatMoneyForDisplay(this.account.balance);
    }
  },
  mounted() {
    EventBus.$on("close-right-sidebar", this.closeRightSidebar);
  },
  beforeDestroy() {
    clearTimeout(timeout);
    EventBus.$off("close-right-sidebar", this.closeRightSidebar);
  },
  created() {
    this.initialize();
  },
  watch: {
    account() {
      this.initialize();
    }
  },
  methods: {
    initialize() {
      this.updateStatistics();

      this.isCompounding = WitnessController.IsAccountCompounding(this.account.UUID);
    },
    getStatistics(which) {
      return this.statistics[which] || null;
    },
    updateStatistics() {
      return new Promise(resolve => {
        clearTimeout(timeout);
        this.statistics = WitnessController.GetAccountWitnessStatistics(this.account.UUID);
        timeout = setTimeout(this.updateStatistics, 2 * 60 * 1000); // update statistics every two minutes
        if (this.statistics) {
          resolve();
        }
      });
    },
    toggleCompounding() {
      WitnessController.SetAccountCompounding(this.account.UUID, !this.isCompounding);
      this.isCompounding = !this.isCompounding;
    },
    setRightSidebar(name) {
      switch (name) {
        case "Settings":
          this.rightSidebar = AccountSettings;
          break;
      }
    },
    getButtonClassNames(route) {
      let classNames = ["button"];
      if (route === this.$route.name) {
        classNames.push("active");
      }
      return classNames;
    },
    closeRightSidebar() {
      this.rightSidebar = null;
    },
    routeTo(route) {
      if (this.$route.name === route) return;
      this.$router.push({ name: route, params: { id: this.account.UUID } });
    }
  }
};
</script>

<style lang="less" scoped>
.holding-information {
  & .flex-row > div {
    line-height: 18px;
  }
  & .flex-row :first-child {
    min-width: 220px;
  }
}
</style>
