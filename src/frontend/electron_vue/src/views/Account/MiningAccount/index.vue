<template>
  <div class="mining-account">
    <portal to="header-slot">
      <account-header :account="account"></account-header>
    </portal>

    <router-view />

    <app-section v-if="isMiningView">
      <div v-if="!isActive">
        <div>
          <app-form-field title="mining.address">
            <div class="address-container">
              <div @click="editAddress" v-if="!editMiningAddress" class="address-row">
                <p class="address-value">{{ miningAddress }}</p>
              </div>
              <div v-else class="flex flex-row" style="width: 100%">
                <input :class="computedStatus" ref="miningAddressInput" type="text" v-model="newMiningAddress" @keydown="onKeydown" />
              </div>
              <div class="action-row" v-if="!editMiningAddress">
                <div @click="copyToClipboard" class="action-icon">
                  <fa-icon :icon="['fal', 'copy']" />
                </div>
                <div @click="resetOverrideAddress" class="action-icon" v-if="usingOverride">
                  <fa-icon :icon="['fal', 'undo']" />
                </div>
                <div @click="editAddress" class="action-icon">
                  <fa-icon :icon="['fal', 'pen']" />
                </div>
              </div>
            </div>
            <div style="height: 10px">
              <span class="copy-confirmation" v-if="confirmCopy"> {{ $t(confirmation) }} </span>
            </div>
          </app-form-field>
        </div>

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
      </div>
      <div v-else>
        <app-form-field class="mining-statistics" title="mining.settings">
          <div class="flex-row">
            <div>{{ $t("mining.number_of_threads") }}</div>
            <div class="flex-1 align-right">
              {{ $t("mining.current_of_max", { current: currentThreadCount, max: availableCores }) }}
            </div>
          </div>
          <div class="flex-row">
            <div>{{ $t("mining.number_of_arena_threads") }}</div>
            <div class="flex-1 align-right">
              {{ $t("mining.current_of_max", { current: currentArenaThreadCount, max: availableCores }) }}
            </div>
          </div>
          <div class="flex-row">
            <div>
              {{ $t("mining.memory_to_use") }}
              <fa-icon
                v-if="currentMemorySize < maximumMemory"
                class="warning"
                :title="$t('mining.warning_performance')"
                :icon="['fal', 'fa-exclamation-triangle']"
              ></fa-icon>
            </div>
            <div class="flex-1 align-right">
              <span> {{ currentMemorySize }} Gb</span>
            </div>
          </div>
        </app-form-field>

        <app-form-field class="mining-statistics" title="mining.statistics">
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
      </div>
    </app-section>

    <button v-if="isMiningView" @click="toggleGeneration" :disabled="generationButtonDisabled">
      {{ isActive ? $t("buttons.stop") : $t("buttons.start") }}
    </button>

    <portal to="footer-slot">
      <div style="display: flex">
        <footer-button title="buttons.info" :icon="['fal', 'info-circle']" routeName="account" @click="routeTo" />
        <footer-button title="buttons.transactions" :icon="['far', 'list-ul']" routeName="transactions" @click="routeTo" />
        <footer-button title="buttons.send" :icon="['fal', 'arrow-from-bottom']" routeName="send-saving" @click="routeTo" />
      </div>
    </portal>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { formatMoneyForDisplay } from "../../../util.js";
import { GenerationController, LibraryController } from "../../../unity/Controllers";
import { clipboard } from "electron";

export default {
  name: "MiningAccount",
  props: {
    account: null,
    confirmation: {
      type: String,
      default: "clipboard_field.copied_to_clipboard"
    }
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
      miningAddress: null,
      editMiningAddress: false,
      newMiningAddress: null,
      addressInvalid: false,
      usingOverride: false,
      confirmCopy: false
    };
  },
  created() {
    this.availableCores = GenerationController.GetAvailableCores();
    this.currentThreadCount = this.settings.threadCount || (this.availableCores < 4 ? 1 : this.availableCores - 2);

    this.currentArenaThreadCount = this.settings.arenaThreadCount || (this.availableCores < 4 ? 1 : this.availableCores / 2);

    this.minimumMemory = 1; // for now just use 1 Gb as a minimum
    this.maximumMemory = Math.floor(GenerationController.GetMaximumMemory() / 1024);
    this.currentMemorySize = this.settings.memorySize || this.maximumMemory;
    const overrideAddress = GenerationController.GetGenerationOverrideAddress();
    if (overrideAddress) {
      this.miningAddress = overrideAddress;
      this.usingOverride = true;
    } else {
      this.miningAddress = GenerationController.GetGenerationAddress();
      this.usingOverride = false;
    }
  },
  computed: {
    ...mapState("mining", {
      isActive: "active",
      stats: "stats",
      settings: "settings",
      totalBalanceFiat() {
        if (!this.rate) return "";
        return `${this.currency.symbol || ""} ${formatMoneyForDisplay(this.account.balance * this.rate, true)}`;
      },
      balanceForDisplay() {
        if (this.account.balance == null) return "";
        return formatMoneyForDisplay(this.account.balance);
      }
    }),
    ...mapState("app", ["rate", "currency"]),
    isMiningView() {
      return this.$route.name === "account";
    },
    hashesPerSecond() {
      return this.formatStats(this.stats, "hashesPerSecond");
    },
    rollingHashesPerSecond() {
      return this.formatStats(this.stats, "rollingHashesPerSecond");
    },
    bestHashesPerSecond() {
      return this.formatStats(this.stats, "bestHashesPerSecond");
    },
    arenaSetupTime() {
      return this.formatStats(this.stats, "arenaSetupTime", " s");
    },
    computedStatus() {
      return this.addressInvalid ? "error" : "";
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
    formatStats(stats, which, postfix = "/s") {
      if (!stats) return null;
      const current = stats[which];
      const result = /[a-z]/i.exec(current);
      return result === null ? `${current}${postfix}` : `${current.substr(0, result.index)} ${current.substr(result.index)}${postfix}`;
    },
    editAddress() {
      this.newMiningAddress = this.miningAddress;
      this.editMiningAddress = true;
      this.$nextTick(() => {
        this.$refs["miningAddressInput"].focus();
      });
    },
    onKeydown(e) {
      switch (e.keyCode) {
        case 13:
          this.changeAccountAddress();
          break;
        case 27:
          this.editMiningAddress = false;
          break;
      }
    },
    changeAccountAddress() {
      if (this.newMiningAddress === "") {
        this.resetOverrideAddress();
        return;
      }
      if (this.newMiningAddress === this.miningAddress) {
        return;
      }

      this.addressInvalid = !LibraryController.IsValidNativeAddress(this.newMiningAddress);

      if (this.addressInvalid) {
        return;
      }

      GenerationController.SetGenerationOverrideAddress(this.newMiningAddress);
      this.miningAddress = this.newMiningAddress;
      this.usingOverride = true;
      this.editMiningAddress = false;
    },
    resetOverrideAddress() {
      GenerationController.SetGenerationOverrideAddress("");
      this.miningAddress = GenerationController.GetGenerationAddress();
      this.usingOverride = false;
      this.editMiningAddress = false;
    },
    copyToClipboard() {
      clipboard.writeText(this.miningAddress);
      if (this.confirmation) {
        this.confirmCopy = true;
        setTimeout(() => {
          this.confirmCopy = false;
        }, 1500);
      }
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

.address-container {
  display: flex;
  flex-direction: row;
  width: 100%;
  height: 40px;
}

.address-row {
  height: 40px;
  margin: 0 -10px;
  padding: 10px;
  flex: 1;
}

.address-value {
  overflow: hidden;
  text-overflow: ellipsis;
  cursor: pointer;
}
.action-row {
  width: 100px;
  height: 40px;
  display: flex;
  align-items: center;
  justify-content: center;
}

.action-icon {
  width: 30px;
  height: 30px;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 3px;
}

.action-icon:hover {
  color: var(--primary-color);
  background: var(--hover-color);
}

.copy-confirmation {
  width: calc(100%) !important;
  display: flex;
  justify-content: flex-end;
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

.warning {
  color: var(--error-color);
}
</style>
