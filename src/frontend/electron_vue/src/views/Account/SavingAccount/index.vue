<template>
  <div class="saving-account">
    <portal to="header-slot">
      <account-header :account="account"></account-header>
    </portal>

    <div v-if="isAccountView && accountIsFunded">
      <app-form-field title="saving_account.compound_earnings">
        <div class="flex-row">
          <vue-slider :min="0" :max="100" :value="compoundingPercent" v-model="compoundingPercent" class="slider" />
          <div class="slider-info">
            {{ compoundingPercent }}
            {{ $tc("saving_account.percent") }}
          </div>
        </div>
      </app-form-field>
    </div>

    <app-section v-if="isAccountView && accountIsFunded" class="saving-information">
      <h2>{{ $t("common.information") }}</h2>
      <div class="flex-row">
        <div>{{ $t("saving_account.status") }}</div>
        <div>{{ accountStatus }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("saving_account.parts") }}</div>
        <div>{{ accountParts }}</div>
      </div>

      <div class="flex-row">
        <div>{{ $t("saving_account.coins_locked") }}</div>
        <div>{{ accountAmountLockedAtCreation }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("saving_account.coins_earned") }}</div>
        <div>{{ accountAmountEarned }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("saving_account.locked_from_block") }}</div>
        <div>{{ lockedFrom }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("saving_account.locked_until_block") }}</div>
        <div>{{ lockedUntil }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("saving_account.lock_duration") }}</div>
        <div>{{ lockDuration }} {{ $t("common.days") }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("saving_account.remaining_lock_period") }}</div>
        <div>{{ remainingLockPeriod }} {{ $t("common.days") }}</div>
      </div>

      <div class="flex-row">
        <div>{{ $t("saving_account.required_earnings_frequency") }}</div>
        <div>{{ requiredEarningsFrequency }} {{ $t("common.days") }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("saving_account.expected_earnings_frequency") }}</div>
        <div>{{ expectedEarningsFrequency }} {{ $t("common.days") }}</div>
      </div>

      <div class="flex-row">
        <div>{{ $t("saving_account.account_weight") }}</div>
        <div>{{ accountWeight }}</div>
      </div>
      <div class="flex-row">
        <div>{{ $t("saving_account.network_weight") }}</div>
        <div>{{ networkWeight }}</div>
      </div>
    </app-section>

    <app-section v-show="isAccountView && !accountIsFunded" class="saving-empty">
      {{ $t("saving_account.empty") }}
    </app-section>

    <router-view />
    <div>
      <portal to="footer-slot">
        <div style="display: flex">
          <div
            id="footer-layout"
            v-on:scroll.passive="handleScroll"
            class="footer-layout"
            :style="{ justifyContent: !showOverFlowArrowRight && !showOverFlowArrowLeft ? 'center' : null }"
          >
            <div @click="scrollToStart" class="scroll-arrow-left" v-if="showOverFlowArrowLeft">
              <fa-icon class="pen" :icon="['fal', 'fa-long-arrow-left']" />
            </div>
            <div @click="scrollToEnd" class="scroll-arrow-right" v-if="showOverFlowArrowRight">
              <fa-icon class="pen" :icon="['fal', 'fa-long-arrow-right']" />
            </div>
            <footer-button title="buttons.info" :icon="['fal', 'info-circle']" routeName="account" @click="routeTo" />
            <footer-button title="buttons.saving_key" :icon="['fal', 'key']" routeName="link-saving-account" @click="routeTo" />
            <footer-button title="buttons.transactions" :icon="['far', 'list-ul']" routeName="transactions" @click="routeTo" />
            <footer-button title="buttons.send" :icon="['fal', 'arrow-from-bottom']" routeName="send-saving" @click="routeTo" />
            <footer-button :class="optimiseButtonClass" title="buttons.optimise" :icon="['fal', 'redo-alt']" routeName="optimise-account" @click="routeTo" />
            <footer-button v-if="renewButtonVisible" title="buttons.renew" :icon="['fal', 'redo-alt']" routeName="renew-account" @click="routeTo" />
          </div>
        </div>
        xx
      </portal>
    </div>
  </div>
</template>

<script>
import { WitnessController, AccountsController, BackendUtilities } from "../../../unity/Controllers";
import { formatMoneyForDisplay } from "../../../util.js";
import { mapState } from "vuex";

let timeout;

export default {
  name: "SavingAccount",
  props: {
    account: null
  },
  data() {
    return {
      rightSection: null,
      rightSectionComponent: null,
      statistics: null,
      compoundingPercent: 0,
      keyHash: null,
      showOverFlowArrowRight: false,
      showOverFlowArrowLeft: false
    };
  },
  mounted() {
    this.isOverflown();
  },
  computed: {
    ...mapState("app", ["rate", "activityIndicator", "currency"]),
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
    renewButtonVisible() {
      return this.accountStatus === "expired";
    },
    optimiseButtonVisible() {
      return this.getStatistics("is_optimal") === false;
    },
    optimiseButtonClass() {
      if (this.getStatistics("is_optimal") === true && this.getStatistics("blocks_since_last_activity") < 100) {
        return "optimise-button-inactive";
      } else if (this.getStatistics("is_optimal") === false) {
        return "optimise-button-hidden";
      }
      return "";
    },
    totalBalanceFiat() {
      if (!this.rate) return "";
      return `${this.currency.symbol || ""} ${formatMoneyForDisplay(this.account.balance * this.rate, true)}`;
    },
    balanceForDisplay() {
      if (this.account.balance == null) return "";
      return formatMoneyForDisplay(this.account.balance);
    }
  },
  beforeDestroy() {
    clearTimeout(timeout);
  },
  created() {
    this.initialize();
  },
  destroyed() {
    window.removeEventListener("resize", this.isOverflown);
  },
  watch: {
    account() {
      this.initialize();
    },
    compoundingPercent(newVal) {
      // Prevent calling this on initialization.
      if (this.compoundingPercent === 0) {
        return;
      } else {
        const timeoutHandler = setTimeout(() => {
          if (newVal == this.compoundingPercent) {
            if (this.keyHash) {
              BackendUtilities.holdinAPIActions(this.keyHash, "distribution", this.compoundingPercent);
              WitnessController.SetAccountCompounding(this.account.UUID, this.compoundingPercent);
            } else {
              WitnessController.SetAccountCompounding(this.account.UUID, this.compoundingPercent);
            }
          }
        }, 1000);

        return timeoutHandler;
      }
    }
  },
  methods: {
    initialize() {
      this.updateStatistics();

      this.compoundingPercent = parseInt(WitnessController.GetAccountWitnessStatistics(this.account.UUID).compounding_percent) || 0;

      AccountsController.ListAccountLinksAsync(this.account.UUID).then(async result => {
        const findHoldin = result.find(element => element.serviceName == "holdin");

        if (findHoldin) {
          // Use the Holdin %
          this.keyHash = findHoldin.serviceData;
          let infoResult = await BackendUtilities.holdinAPIActions(this.keyHash, "getinfo");

          if (infoResult.data.compound) {
            this.compoundingPercent = parseInt(infoResult.data.compound);
          } else {
            this.compoundingPercent = parseInt(WitnessController.GetAccountWitnessStatistics(this.account.UUID).compounding_percent) || 0;
          }
        } else {
          // Use the Gulden compounding Percent
          this.compoundingPercent = parseInt(WitnessController.GetAccountWitnessStatistics(this.account.UUID).compounding_percent) || 0;
        }
      });

      window.addEventListener("resize", this.isOverflown);
    },
    getStatistics(which) {
      return this.statistics[which];
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
    getButtonClassNames(route) {
      let classNames = ["button"];
      if (route === this.$route.name) {
        classNames.push("active");
      }
      return classNames;
    },
    routeTo(route) {
      if (route === "optimise-account" && this.optimiseButtonClass === "optimise-button-inactive") {
        alert("Cannot perform this operation while account is in cooldown, please wait and try again later.");
      } else {
        if (this.$route.name === route) return;
        this.$router.push({ name: route, params: { id: this.account.UUID } });
      }
    },
    isOverflown(e) {
      // Determine whether to show the overflow arrow
      if (e) {
        const width = e.currentTarget.innerWidth;
        if (width && width <= 1000) {
          if (this.renewButtonVisible || this.optimiseButtonVisible) {
            this.showOverFlowArrowRight = true;
          } else {
            this.showOverFlowArrowRight = false;
          }
        } else {
          this.showOverFlowArrowRight = false;
        }
      } else {
        // Triggered on page load.
        if (window.innerWidth <= 1000) {
          if (this.renewButtonVisible || this.optimiseButtonVisible) {
            this.showOverFlowArrowRight = true;
          } else {
            this.showOverFlowArrowRight = false;
          }
        } else {
          this.showOverFlowArrowRight = false;
        }
      }
    },
    handleScroll(e) {
      // Determine when user it at the end of the horizontal scroll bar.
      if (e.target.scrollWidth - e.target.scrollLeft === e.target.clientWidth) {
        this.showOverFlowArrowRight = false;
        this.showOverFlowArrowLeft = true;
      } else {
        this.showOverFlowArrowRight = true;
        this.showOverFlowArrowLeft = false;
      }
    },
    scrollToEnd() {
      var container = document.querySelector("#footer-layout");
      container.scrollLeft = container.scrollWidth;
    },
    scrollToStart() {
      var container = document.querySelector("#footer-layout");
      container.scrollLeft = 0;
    }
  }
};
</script>

<style lang="less" scoped>
.saving-information {
  & .flex-row > div {
    line-height: 18px;
  }
  & .flex-row :first-child {
    min-width: 220px;
  }
}
.slider {
  width: calc(100% - 60px) !important;
  display: inline-block;
}
.slider-info {
  text-align: right;
  line-height: 18px;
  flex: 1;
}
.footer-layout {
  display: flex;
  flex-direction: row;
  align-items: center;
  overflow-x: scroll;
  width: 100%;
}
.footer-layout::-webkit-scrollbar {
  display: none;
}
.scroll-arrow-right {
  cursor: pointer;
  background-image: linear-gradient(90deg, rgba(255, 255, 255, 0.9), #ffffff);
  height: 55px;
  width: 55px;
  position: absolute;
  display: flex;
  flex-direction: row;
  justify-content: center;
  align-items: center;
  right: 0;
  bottom: 0;
}
.scroll-arrow-left {
  cursor: pointer;
  background-image: linear-gradient(270deg, rgba(255, 255, 255, 0.9), #ffffff);
  height: 55px;
  width: 55px;
  position: absolute;
  display: flex;
  flex-direction: row;
  justify-content: center;
  left: -30;
  align-items: center;
  bottom: 0;
}
.optimise-button-inactive {
  color: #a8a8a8;
}
.optimise-button-hidden {
  display: none !important;
}
</style>
