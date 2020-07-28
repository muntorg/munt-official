<template>
  <novo-page-layout class="account-view">
    <template v-slot:header>
      <section class="header flex-row">
        <div class="info">
          <div class="label ellipsis">{{ account.label }}</div>
          <div class="balance ellipsis">{{ account.balance }}</div>
        </div>
        <div class="settings flex-col" v-if="false">
          <span>
            <fa-icon :icon="['fal', 'cog']" />
          </span>
        </div>
      </section>
    </template>

    <h4>active: {{ generationActive }}</h4>
    <h4>stats:</h4>
    <pre>{{ generationStats }}</pre>

    <template v-slot:footer>
      <section class="footer">
        <button @click="toggleGeneration">
          <span v-if="generationActive">{{ $t("buttons.stop") }}</span>
          <span v-else>{{ $t("buttons.start") }}</span>
        </button>
      </section>
    </template>
  </novo-page-layout>
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
::v-deep {
  & .header {
    & > .info {
      width: calc(100% - 48px - 26px);
      padding-right: 10px;

      & > .label {
        font-size: 1.1em;
        font-weight: 500;
        line-height: 20px;
      }

      & > .balance {
        line-height: 20px;
      }
    }

    & > .settings {
      font-size: 16px;

      & span {
        padding: 10px;
        cursor: pointer;

        &:hover {
          background: #f5f5f5;
        }
      }
    }
  }

  & .footer {
    text-align: right;
    padding-right: 5px;
  }
}
</style>
