<template>
  <div class="mining-account">
    <portal to="header-slot">
      <main-header :title="account.label" :subtitle="account.balance" />
    </portal>

    <h4>active: {{ generationActive }}</h4>
    <h4>stats:</h4>
    <pre>{{ generationStats }}</pre>

    <portal to="footer-slot">
      <novo-button-section>
        <button @click="toggleGeneration">
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
