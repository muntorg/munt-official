<template>
  <div class="mining-account">
    <portal to="header-slot">
      <main-header
        :title="account.label"
        :subtitle="account.balance.toFixed(2)"
      />
    </portal>

    <novo-form-field :title="$t('mining.number_of_threads')">
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
    </novo-form-field>

    <novo-form-field :title="$t('mining.memory_to_use')">
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
    </novo-form-field>

    <novo-form-field
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
    </novo-form-field>

    <portal to="footer-slot">
      <novo-button-section>
        <button @click="toggleGeneration" :disabled="buttonDisabled">
          <span v-if="isActive">{{ $t("buttons.stop") }}</span>
          <span v-else>{{ $t("buttons.start") }}</span>
        </button>
      </novo-button-section>
    </portal>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { GenerationController } from "../../../unity/Controllers";

export default {
  name: "MiningAccount",
  props: {
    account: null
  },
  data() {
    return {
      currentMemorySize: 2,
      currentThreadCount: 4,
      availableCores: 0,
      minimumMemory: 0,
      maximumMemory: 0,
      buttonDisabled: false
    };
  },
  created() {
    this.availableCores = GenerationController.GetAvailableCores();
    this.currentThreadCount =
      this.settings.threadCount ||
      (this.availableCores < 4 ? 1 : this.availableCores - 2);
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
    }
  },
  watch: {
    isActive() {
      this.buttonDisabled = false;
    },
    currentMemorySize() {
      this.$store.dispatch("mining/SET_MEMORY_SIZE", this.currentMemorySize);
    },
    currentThreadCount() {
      this.$store.dispatch("mining/SET_THREAD_COUNT", this.currentThreadCount);
    }
  },
  methods: {
    toggleGeneration() {
      this.buttonDisabled = true;
      if (this.isActive) {
        GenerationController.StopGeneration();
      } else {
        let result = GenerationController.StartGeneration(
          this.currentThreadCount,
          this.currentMemorySize + "G"
        );
        if (result === false) {
          // todo: starting failed, notify user
        }
      }
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
</style>
