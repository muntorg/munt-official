<template>
  <div class="mining-account">
    <portal to="header-slot">
      <main-header
        :title="account.label"
        :subtitle="account.balance.toFixed(2)"
      />
    </portal>

    <app-form-field :title="$t('mining.number_of_threads')">
      <div class="flex-row">
        <vue-slider
          :min="1"
          :max="availableCores"
          :value="currentThreadCount"
          v-model="currentThreadCount"
          class="slider"
          :disabled="isActive"
        />
        <div class="slider-info">
          {{ currentThreadCount }}
          {{ $tc("mining.thread", currentThreadCount) }}
        </div>
      </div>
    </app-form-field>

    <app-form-field :title="$t('mining.number_of_arena_threads')">
      <div class="flex-row">
        <vue-slider
          :min="1"
          :max="availableCores"
          :value="currentArenaThreadCount"
          v-model="currentArenaThreadCount"
          class="slider"
          :disabled="isActive"
        />
        <div class="slider-info">
          {{ currentArenaThreadCount }}
          {{ $tc("mining.thread", currentArenaThreadCount) }}
        </div>
      </div>
    </app-form-field>

    <app-form-field :title="$t('mining.memory_to_use')">
      <div class="flex-row">
        <vue-slider
          :min="minimumMemory"
          :max="maximumMemory"
          :value="currentMemorySize"
          v-model="currentMemorySize"
          class="slider"
          :disabled="isActive"
        />
        <div class="slider-info">{{ currentMemorySize }} Gb</div>
      </div>
    </app-form-field>

    <app-form-field>
      <div class="flex-row">
        <div class="flex-1 align-right">
          <button
            @click="toggleGeneration"
            :disabled="generationButtonDisabled"
          >
            {{ isActive ? $t("buttons.stop") : $t("buttons.start") }}
          </button>
        </div>
      </div>
    </app-form-field>

    <app-form-field
      class="mining-statistics"
      :title="$t('mining.statistics')"
      v-if="isActive"
    >
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

    <portal to="footer-slot">
      <section class="footer">
        <span class="button" @click="emptyAccount" v-if="sendButtonVisible">
          <fa-icon :icon="['fal', 'arrow-from-bottom']" />
          {{ $t("buttons.send") }}
        </span>
      </section>
    </portal>

    <portal to="sidebar-right">
      <component
        v-if="rightSidebar"
        :is="rightSidebar"
        v-bind="rightSidebarProps"
      />
    </portal>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { GenerationController } from "../../../unity/Controllers";
import EventBus from "../../../EventBus";

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
  mounted() {
    EventBus.$on("close-right-sidebar", this.closeRightSidebar);
  },
  beforeDestroy() {
    EventBus.$off("close-right-sidebar", this.closeRightSidebar);
  },
  created() {
    this.availableCores = GenerationController.GetAvailableCores();
    this.currentThreadCount =
      this.settings.threadCount ||
      (this.availableCores < 4 ? 1 : this.availableCores - 2);

    this.currentArenaThreadCount =
      this.settings.arenaThreadCount ||
      (this.availableCores < 4 ? 1 : this.availableCores / 2);

    this.minimumMemory = 1; // for now just use 1 Gb as a minimum
    this.maximumMemory = Math.floor(
      GenerationController.GetMaximumMemory() / 1024
    );
    this.currentMemorySize = this.settings.memorySize || this.maximumMemory;
  },
  computed: {
    ...mapState("mining", {
      isActive: "active",
      stats: "stats",
      settings: "settings"
    }),
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
    },
    sendButtonDisabled() {
      return this.account.spendable > 0;
    },
    sendButtonVisible() {
      return this.sendButtonDisabled && this.rightSidebar === null;
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
      this.$store.dispatch(
        "mining/SET_ARENA_THREAD_COUNT",
        this.currentArenaThreadCount
      );
    },
    sendButtonDisabled() {
      if (this.rightSidebar !== null && this.sendButtonDisabled === false)
        this.closeRightSidebar();
    }
  },
  methods: {
    toggleGeneration() {
      this.generationButtonDisabled = true;
      if (this.isActive) {
        GenerationController.StopGeneration();
      } else {
        let result = GenerationController.StartGeneration(
          this.currentThreadCount,
          this.currentArenaThreadCount,
          this.currentMemorySize + "G"
        );
        if (result === false) {
          // todo: starting failed, notify user
        }
      }
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
  width: 100%;
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

// todo: .footer styles below are copy/pasted from SpendingAccount/index.vue, maybe move to parent
.footer {
  text-align: center;
  line-height: calc(var(--footer-height) - 1px);

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
</style>
