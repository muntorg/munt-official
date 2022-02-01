<template>
  <div class="information-page">
    <app-section>
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
    </app-section>

    <app-section>
      <h4>Network</h4>
      <div class="flex-row">
        <div>Status</div>
        <div>{{ clientInfo.network_status }}</div>
      </div>
      <div class="flex-row">
        <div>Connection count</div>
        <div>{{ numberOfConnections }}</div>
      </div>
    </app-section>

    <app-section>
      <h4>Block chain</h4>
      <div class="flex-row">
        <div>Number of blocks</div>
        <div>{{ clientInfo.chain_tip_height }}</div>
      </div>
      <div class="flex-row">
        <div>Last block time</div>
        <div>{{ lastBlockTime }}</div>
      </div>
      <div class="flex-row">
        <div>Last block hash</div>
        <div>{{ clientInfo.chain_tip_hash }}</div>
      </div>
    </app-section>

    <app-section>
      <h4>Memory pool</h4>
      <div class="flex-row">
        <div>Number of transactions</div>
        <div>{{ clientInfo.mempool_transaction_count }}</div>
      </div>
      <div class="flex-row">
        <div>Memory usage</div>
        <div>{{ clientInfo.mempool_memory_size }}</div>
      </div>
    </app-section>
  </div>
</template>

<script>
import { LibraryController } from "../../unity/Controllers";
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
      return new Date(this.clientInfo.startup_timestamp * 1000);
    },
    lastBlockTime() {
      return new Date(this.clientInfo.chain_tip_time * 1000);
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
      console.log("call update GetClientInfo");
      this.clientInfo = LibraryController.GetClientInfo();
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
  & h4 {
    margin: 0 0 10px 0;
  }
  & .flex-row > div {
    font-size: 0.95em;
    line-height: 20px;
  }
  & .flex-row :first-child {
    min-width: 180px;
  }
}
</style>
