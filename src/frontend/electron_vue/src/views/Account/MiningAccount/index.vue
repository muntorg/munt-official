<template>
  <div class="mining-account">
    <portal to="header-slot">
      <account-header :account="account"></account-header>
    </portal>

    <router-view />

    <app-section v-if="isMiningView">
      <app-form-field title="mining.number_of_threads">
        <div class="flex-row">
          <vue-slider :min="1" :max="availableCores" :value="currentThreadCount" v-model="currentThreadCount" class="slider" :disabled="isActive" />
          <div class="slider-info">
            {{ currentThreadCount }}
            {{ $tc("mining.thread", currentThreadCount) }}
          </div>
        </div>
      </app-form-field>

      <app-form-field title="mining.number_of_arena_threads">
        <div class="flex-row">
          <vue-slider :min="1" :max="availableCores" :value="currentArenaThreadCount" v-model="currentArenaThreadCount" class="slider" :disabled="isActive" />
          <div class="slider-info">
            {{ currentArenaThreadCount }}
            {{ $tc("mining.thread", currentArenaThreadCount) }}
          </div>
        </div>
      </app-form-field>

      <app-form-field title="mining.memory_to_use">
        <div class="flex-row">
          <vue-slider :min="minimumMemory" :max="maximumMemory" :value="currentMemorySize" v-model="currentMemorySize" class="slider" :disabled="isActive" />
          <div class="slider-info">{{ currentMemorySize }} Gb</div>
        </div>
      </app-form-field>

      <app-form-field class="mining-statistics" title="mining.statistics" v-if="isActive">
        <div class="flex-row">
          <div>{{ $t("mining.last_reported_speed") }}</div>
          <div class="flex-1 align-right">
            {{ hashesPerSecond }}
          </div>
        </div>
        <div class="flex-row">
          <div>{{ $t("mining.moving_average") }}</div>
          <div class="flex-1 align-right">
            {{ rollingHashesPerSecond }}
          </div>
        </div>
        <div class="flex-row">
          <div>{{ $t("mining.best_reported_speed") }}</div>
          <div class="flex-1 align-right">
            {{ bestHashesPerSecond }}
          </div>
        </div>
        <div class="flex-row">
          <div>{{ $t("mining.arena_setup_time") }}</div>
          <div class="flex-1 align-right">
            {{ arenaSetupTime }}
          </div>
        </div>
      </app-form-field>
    </app-section>

    <button v-if="isMiningView" @click="toggleGeneration" :disabled="generationButtonDisabled">
      {{ isActive ? $t("buttons.stop") : $t("buttons.start") }}
    </button>

    <portal to="footer-slot">
      <footer-button title="buttons.info" :icon="['fal', 'info-circle']" routeName="account" @click="routeTo" />
      <footer-button title="buttons.send" :icon="['fal', 'arrow-from-bottom']" routeName="send-holding" @click="routeTo" />
    </portal>

    <portal to="sidebar-right">
      <component v-if="rightSidebar" :is="rightSidebar" v-bind="rightSidebarProps" />
    </portal>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { formatMoneyForDisplay } from "../../../util.js";
import { GenerationController } from "../../../unity/Controllers";

import Send from "./Send";

export default {
  name: "MiningAccount",
  props: {
    account: null
  },
  data() {
    return {
      currentMemorySize: 2,
      currentThreadCount: 4,
      currentArenaThreadCount: 4,
      availableCores: 0,
      minimumMemory: 0,
      maximumMemory: 0,
      generationButtonDisabled: false,
      rightSidebar: null
    };
  },
  created() {
    this.availableCores = GenerationController.GetAvailableCores();
    this.currentThreadCount = this.settings.threadCount || (this.availableCores < 4 ? 1 : this.availableCores - 2);

    this.currentArenaThreadCount = this.settings.arenaThreadCount || (this.availableCores < 4 ? 1 : this.availableCores / 2);

    this.minimumMemory = 1; // for now just use 1 Gb as a minimum
    this.maximumMemory = Math.floor(GenerationController.GetMaximumMemory() / 1024);
    this.currentMemorySize = this.settings.memorySize || this.maximumMemory;
  },
  computed: {
    ...mapState("mining", {
      isActive: "active",
      stats: "stats",
      settings: "settings",
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
    }),
    ...mapState("app", ["rate"]),
    isMiningView() {
      return this.$route.name === "account";
    },
    hashesPerSecond() {
      return this.stats ? `${this.stats.hashesPerSecond}/s` : null;
    },
    rollingHashesPerSecond() {
      return this.stats ? `${this.stats.rollingHashesPerSecond}/s` : null;
    },
    bestHashesPerSecond() {
      return this.stats ? `${this.stats.bestHashesPerSecond}/s` : null;
    },
    arenaSetupTime() {
      return this.stats ? `${this.stats.arenaSetupTime}s` : null;
    },
    rightSidebarProps() {
      return null;
    }
  },
  watch: {
    isActive() {
      this.generationButtonDisabled = false;
    },
    currentMemorySize() {
      this.$store.dispatch("mining/SET_MEMORY_SIZE", this.currentMemorySize);
    },
    currentThreadCount() {
      this.$store.dispatch("mining/SET_THREAD_COUNT", this.currentThreadCount);
    },
    currentArenaThreadCount() {
      this.$store.dispatch("mining/SET_ARENA_THREAD_COUNT", this.currentArenaThreadCount);
    }
  },
  methods: {
    toggleGeneration() {
      this.generationButtonDisabled = true;
      if (this.isActive) {
        GenerationController.StopGeneration();
      } else {
        let result = GenerationController.StartGeneration(this.currentThreadCount, this.currentArenaThreadCount, this.currentMemorySize + "G");
        if (result === false) {
          // todo: starting failed, notify user
        }
      }
    },
    routeTo(route) {
      if (this.$route.name === route) return;
      this.$router.push({ name: route, params: { id: this.account.UUID } });
    },
    closeRightSidebar() {
      this.rightSidebar = null;
      this.txHash = null;
    },
    emptyAccount() {
      this.rightSidebar = Send;
    }
  }
};
</script>

<style lang="less" scoped>
.mining-account {
  display: flex;
  flex-direction: column;
  justify-content: space-between;
}

.slider {
  width: calc(100% - 100px) !important;
  display: inline-block;
}

.slider-info {
  text-align: right;
  line-height: 18px;
  flex: 1;
}

.mining-statistics .flex-row {
  line-height: 20px;
}
</style>
