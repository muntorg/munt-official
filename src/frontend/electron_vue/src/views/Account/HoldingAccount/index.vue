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

    <novo-section class="holding-information">
      <h4>information</h4>

      <div class="flex-row">
        <div>Status</div>
        <div>{{ accountStatus }}</div>
      </div>
      <div class="flex-row">
        <div>Locked from block</div>
        <div>{{ lockedFrom }}</div>
      </div>
      <div class="flex-row">
        <div>Locked until block</div>
        <div>
          {{ lockedUntil }}
        </div>
      </div>
      <div class="flex-row">
        <div>Lock duration</div>
        <div>{{ lockDuration }} blocks</div>
      </div>
      <div class="flex-row">
        <div>Remaining lock period</div>
        <div>{{ remainingLockPeriod }} blocks</div>
      </div>

      <div class="flex-row">
        <div>Required earnings frequency</div>
        <div>{{ requiredEarningsFrequency }} blocks</div>
      </div>
      <div class="flex-row">
        <div>Expected earnings frequency</div>
        <div>{{ expectedEarningsFrequency }} blocks</div>
      </div>

      <div class="flex-row">
        <div>Account weight</div>
        <div>{{ accountWeight }}</div>
      </div>
      <div class="flex-row">
        <div>Network weight</div>
        <div>{{ networkWeight }}</div>
      </div>
    </novo-section>

    <portal to="footer-slot">
      <div />
    </portal>
  </div>
</template>

<script>
import UnityBackend from "../../../unity/UnityBackend";

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
      statistics: null
    };
  },
  computed: {
    accountStatus() {
      return this.statistics.account_status || null;
    },
    lockedFrom() {
      return this.statistics.account_initial_lock_creation_block_height || null;
    },
    lockedUntil() {
      return (
        this.statistics.account_initial_lock_creation_block_height +
          this.statistics.account_initial_lock_period_in_blocks || null
      );
    },
    lockDuration() {
      return this.statistics.account_initial_lock_period_in_blocks || null;
    },
    remainingLockPeriod() {
      return this.statistics.account_remaining_lock_period_in_blocks || null;
    },
    requiredEarningsFrequency() {
      return this.statistics.account_expected_witness_period_in_blocks || null;
    },
    expectedEarningsFrequency() {
      return this.statistics.account_estimated_witness_period_in_blocks || null;
    },
    accountWeight() {
      return this.statistics.account_weight || null;
    },
    networkWeight() {
      return this.statistics.network_tip_total_weight || null;
    }
  },
  created() {
    this.updateStatistics();
  },
  beforeDestroy() {
    clearTimeout(timeout);
  },
  watch: {
    account() {
      this.updateStatistics();
    }
  },
  methods: {
    closeRightSection() {
      this.rightSection = null;
      this.rightSectionComponent = null;
    },
    updateStatistics() {
      clearTimeout(timeout);
      this.statistics = UnityBackend.GetAccountWitnessStatistics(
        this.account.UUID
      );
      timeout = setTimeout(this.updateStatistics, 5000);
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

.flex-row > div {
  line-height: 18px;
}
.flex-row :first-child {
  min-width: 220px;
}
</style>
