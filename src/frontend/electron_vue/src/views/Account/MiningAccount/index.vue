<template>
  <div class="mining-account">
    <portal to="header-slot">
      <main-header :title="account.label" :subtitle="account.balance" />
    </portal>

    <h4>number of threads</h4>
    <vue-slider 
    v-model="miningThreadCount" 
    :min="1"
    :max="64"
    />
    <h4>memory to use</h4>
    <vue-slider 
    v-model="miningMemorySize" 
    :min="1"
    :max="12"
    />
    
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
import VueSlider from 'vue-slider-component'
import 'vue-slider-component/theme/antd.css'

export default {
  name: "MiningAccount",
   components: {
    VueSlider
  },
  props: {
    account: null
  },
  data () {
    return {
      miningMemorySize: 12,
      miningThreadCount: 2
    }
  },
  computed: {
    ...mapState(["generationActive", "generationStats"])
  },
  methods: {
    toggleGeneration() {
      if (this.generationActive) {
        UnityBackend.StopGeneration();
      } else {
        let result = UnityBackend.StartGeneration(this.miningThreadCount, this.miningMemorySize+"G");
        if (result === false) {
          // todo: starting failed, notify user
        }
      }
    }
  }
};
</script>
