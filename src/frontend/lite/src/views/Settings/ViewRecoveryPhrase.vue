<template>
  <div class="view-recovery-phrase-view">
    <div>
      <content-wrapper heading="common.important" heading-style="warning" content="setup.this_is_your_recovery_phrase">
        <app-section class="phrase">
          {{ computedPhrase }}
        </app-section>
      </content-wrapper>
    </div>

    <div class="flex-1"></div>

    <app-button-section>
      <button @click="viewRecoveryPhrase" v-if="!recoveryPhrase">
        {{ $t("buttons.unlock") }}
      </button>
      <button @click="back">
        {{ $t("buttons.back") }}
      </button>
    </app-button-section>
  </div>
</template>

<script>
import { mapState } from "vuex";
import EventBus from "../../EventBus";
import { LibraryController } from "../../unity/Controllers";

export default {
  data() {
    return {
      recoveryPhrase: null
    };
  },
  computed: {
    ...mapState("wallet", ["unlocked"]),
    computedPhrase() {
      if (this.recoveryPhrase !== null) return this.recoveryPhrase;
      else return "In order to see your recovery phrase you need to unlock your wallet";
    }
  },
  watch: {
    unlocked: {
      immediate: true,
      handler() {
        if (this.unlocked) {
          this.viewRecoveryPhrase();
        }
      }
    }
  },
  methods: {
    viewRecoveryPhrase() {
      if (this.recoveryPhrase) return;
      EventBus.$emit("unlock-wallet", {
        callback: async () => {
          this.recoveryPhrase = LibraryController.GetRecoveryPhrase().phrase;
        }
      });
    },
    back() {
      this.$router.push({ name: "settings" });
    },
    routeTo(route) {
      this.$router.push({ name: route });
    }
  }
};
</script>

<style lang="less" scoped>
.view-recovery-phrase-view {
  display: flex;
  height: 100%;
  flex-direction: column;
  flex-wrap: nowrap;
  justify-content: space-between;
}

.phrase {
  padding: 15px;
  font-size: 1.05em;
  font-weight: 500;
  text-align: center;
  word-spacing: 4px;
  background-color: #f5f5f5;
  user-select: none;
}
</style>
