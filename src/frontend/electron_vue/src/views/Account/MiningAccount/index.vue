<template>
  <div class="mining-account">
    <portal to="header-slot">
      <main-header :title="account.label" :subtitle="account.balance" />
    </portal>

    <novo-form-field :title="$t('mining.number_of_threads')">
      <div class="flex-row">
        <vue-slider
          :min="1"
          :max="availableCores"
          :value="miningThreadCount"
          v-model="miningThreadCount"
          class="slider"
          :disabled="generationActive"
        />
        <div class="slider-info">
          {{ miningThreadCount }} {{ $tc("mining.thread", miningThreadCount) }}
        </div>
      </div>
    </novo-form-field>

    <novo-form-field :title="$t('mining.memory_to_use')">
      <div class="flex-row">
        <vue-slider
          :min="minimumMemory"
          :max="maximumMemory"
          :value="miningMemorySize"
          v-model="miningMemorySize"
          class="slider"
          :disabled="generationActive"
        />
        <div class="slider-info">{{ miningMemorySize }} Gb</div>
      </div>
    </novo-form-field>

    <novo-form-field
      class="mining-statistics"
      :title="$t('mining.statistics')"
      v-if="generationActive"
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
          <span v-if="generationActive">{{ $t("buttons.stop") }}</span>
          <span v-else>{{ $t("buttons.start") }}</span>
        </button>
      </novo-button-section>
    </portal>
  </div>
</template>

<script>
import { mapState } from "vuex";
import UnityBackend from "../../../unity/UnityBackend";

export default {
  name: "MiningAccount",
  props: {
    account: null
  },
  data() {
    return {
      miningMemorySize: 2,
      miningThreadCount: 4,
      availableCores: 0,
      minimumMemory: 0,
      maximumMemory: 0,
      buttonDisabled: false
    };
  },
  created() {
    this.availableCores = UnityBackend.GetAvailableCores();
    this.miningThreadCount = this.availableCores < 4 ? 1 : this.availableCores - 2;
    this.minimumMemory = 1; // for now just use 1 Gb as a minimum
    this.maximumMemory = Math.floor(UnityBackend.GetMaximumMemory() / 1024);
    this.miningMemorySize = this.maximumMemory;
  },
  computed: {
    ...mapState(["generationActive", "generationStats"]),
    hashesPerSecond() {
      return this.generationStats
        ? `${this.generationStats.hashesPerSecond}/s`
        : null;
    },
    rollingHashesPerSecond() {
      return this.generationStats
        ? `${this.generationStats.rollingHashesPerSecond}/s`
        : null;
    },
    bestHashesPerSecond() {
      return this.generationStats
        ? `${this.generationStats.bestHashesPerSecond}/s`
        : null;
    },
    arenaSetupTime() {
      return this.generationStats
        ? `${this.generationStats.arenaSetupTime}s`
        : null;
    }
  },
  watch: {
    generationActive() {
      this.buttonDisabled = false;
    }
  },
  methods: {
    toggleGeneration() {
      this.buttonDisabled = true;
      if (this.generationActive) {
        UnityBackend.StopGeneration();
      } else {
        let result = UnityBackend.StartGeneration(
          this.miningThreadCount,
          this.miningMemorySize + "G"
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
