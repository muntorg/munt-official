<template>
  <div class="account-view">
    <div class="main-section">
      <div class="header">
        <div class="info">
          <div class="label">{{ account.label }}</div>
          <div class="balance">{{ account.balance }}</div>
        </div>
        <div class="settings" v-if="false">
          <span>
            <fa-icon :icon="['fal', 'cog']" />
          </span>
        </div>
      </div>
      <div class="content">
        <h4>active: {{ generationActive }}</h4>
        <h4>stats:</h4>
        <pre>{{ generationStats }}</pre>
      </div>
      <div class="footer">
        <button @click="toggleGeneration">
          <span v-if="generationActive">{{ $t("buttons.stop") }}</span>
          <span v-else>{{ $t("buttons.start") }}</span>
        </button>
      </div>
    </div>
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
  computed: {
    ...mapState(["generationActive", "generationStats"])
  },
  methods: {
    toggleGeneration() {
      if (this.generationActive) {
        UnityBackend.StopGeneration();
      } else {
        let result = UnityBackend.StartGeneration(1, "4096M"); // use hardcoded settings for now
        if (result === false) {
          // todo: starting failed, notify user
        }
      }
    }
  }
};
</script>

<style lang="less" scoped>
.account-view {
  height: 100vh;
}

.main-section {
  height: 100%;
  flex: 1;

  display: flex;
  flex-direction: column;

  & .header {
    height: var(--header-height);
    border-bottom: 1px solid var(--main-border-color);
    padding: 11px 24px 0 24px;
    white-space: nowrap;
    overflow: hidden;

    display: flex;
    flex-direction: row;
    overflow: hidden;

    & .info {
      & .label {
        font-size: 1.1em;
        font-weight: 500;
        line-height: 20px;
      }
      & .balance {
        line-height: 20px;
      }

      flex: 1;
    }

    & .settings {
      margin: 2px -10px 0 0;
      font-size: 16px;
      display: flex;
      flex-direction: column;

      & span {
        padding: 10px;
        cursor: pointer;

        &:hover {
          background: #f5f5f5;
        }
      }
    }
  }

  & .content {
    flex: 1;
    padding: 24px;
    overflow-y: hidden;

    --scrollbarBG: #fff;
    --thumbBG: #ddd;

    &:hover {
      overflow-y: overlay;
    }
    &::-webkit-scrollbar {
      width: 14px;
    }
    &::-webkit-scrollbar-track {
      background: var(--scrollbarBG);
    }
    &::-webkit-scrollbar-thumb {
      border: 3px solid var(--scrollbarBG);
      background-color: var(--thumbBG);
      border-radius: 14px;
    }
  }

  & .footer {
    height: var(--footer-height);
    line-height: var(--footer-height);
    border-top: 1px solid var(--main-border-color);
    text-align: right;
    padding-right: 5px;
  }
}
</style>
