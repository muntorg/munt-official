<template>
  <div class="information-page">
    <novo-section>
      <h4>General</h4>

      <div class="flex-row">
        <div>Client version</div>
        <div class="ellipsis">{{ clientInfo.client_version }}</div>
      </div>
      <div class="flex-row">
        <div>Useragent</div>
        <div class="ellipsis">{{ clientInfo.user_agent }}</div>
      </div>
      <div class="flex-row">
        <div>Data dir</div>
        <div class="ellipsis">{{ clientInfo.datadir_path }}</div>
      </div>
      <div class="flex-row">
        <div>Log file</div>
        <div class="ellipsis">{{ clientInfo.logfile_path }}</div>
      </div>
      <div class="flex-row">
        <div>Startup time</div>
        <div class="ellipsis">{{ startupTime }}</div>
      </div>
    </novo-section>

    <novo-section>
      <h4>Network</h4>
      <div class="flex-row">
        <div>Status</div>
        <div>{{ clientInfo.network_status }}</div>
      </div>
      <div class="flex-row">
        <div>Connection count</div>
        <div>{{ numberOfConnections }}</div>
      </div>
    </novo-section>

    <novo-section>
      <h4>Block chain</h4>
      <div class="flex-row">
        <div>Number of blocks</div>
        <div>{{ clientInfo.chain_tip_height }}</div>
      </div>
      <div class="flex-row">
        <div>Last block time</div>
        <div>{{ clientInfo.chain_tip_time }}</div>
      </div>
      <div class="flex-row">
        <div>Last block hash</div>
        <div>{{ clientInfo.chain_tip_hash }}</div>
      </div>
    </novo-section>

    <novo-section>
      <h4>Memory pool</h4>
      <div class="flex-row">
        <div>Number of transactions</div>
        <div>{{ clientInfo.mempool_transaction_count }}</div>
      </div>
      <div class="flex-row">
        <div>Memory usage</div>
        <div>{{ clientInfo.mempool_memory_size }}</div>
      </div>
    </novo-section>
  </div>
</template>

<script>
import UnityBackend from "../../unity/UnityBackend";
let timeout;

export default {
  data() {
    return {
      clientInfo: null,
      enableTimeout: true
    };
  },
  name: "InformationPage",
  computed: {
    startupTime() {
      let date = new Date(this.clientInfo.startup_timestamp * 1000);
      return date;
    },
    numberOfConnections() {
      let connections_in = parseInt(this.clientInfo.num_connections_in);
      let connections_out = parseInt(this.clientInfo.num_connections_out);

      return `${connections_in +
        connections_out} (In: ${connections_in} / Out: ${connections_out})`;
    }
  },
  created() {
    document.addEventListener("webkitvisibilitychange", this.visibilityChange);
    this.updateClientInfo();
  },
  beforeDestroy() {
    this.enableTimeout = false;
    clearTimeout(timeout);
    document.removeEventListener(
      "webkitvisibilitychange",
      this.visibilityChange
    );
  },
  methods: {
    updateClientInfo() {
      clearTimeout(timeout);
      if (this.enableTimeout === false) return;
      this.clientInfo = UnityBackend.GetClientInfo();
      timeout = setTimeout(this.updateClientInfo, 5000);
    },
    visibilityChange() {
      if (document["webkitHidden"]) {
        this.enableTimeout = false;
      } else {
        this.enableTimeout = true;
        this.updateClientInfo();
      }
    }
  }
};
</script>

<style lang="less" scoped>
.information-page {
  width: 100%;
  height: 100%;

  & .flex-row > div {
    line-height: 18px;
  }
  & .flex-row :first-child {
    min-width: 180px;
  }
}
</style>
