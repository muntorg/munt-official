<template>
  <div class="information-page">
    <app-section>
      <h4>General</h4>

      <div class="flex-row">
        <div>Client version</div>
        <div>{{ clientInfo.client_version }}</div>
      </div>
      <div class="flex-row">
        <div>Useragent</div>
        <div>{{ clientInfo.user_agent }}</div>
      </div>
      <div class="flex-row">
        <div>Data dir</div>
        <div class="path-row">
          <div @click="openFile(clientInfo.datadir_path)" class="list-item-icon">
            <fa-icon :icon="['fal', 'file-search']" />
          </div>
          <div class="selectable">{{ clientInfo.datadir_path }}</div>
        </div>
      </div>
      <div class="flex-row">
        <div>Log file</div>
        <div class="path-row">
          <div @click="openFile(clientInfo.logfile_path)" class="list-item-icon">
            <fa-icon :icon="['fal', 'file-search']" />
          </div>
          <div class="selectable">
            {{ clientInfo.logfile_path }}
          </div>
        </div>
      </div>
      <div class="flex-row">
        <div>Startup time</div>
        <div>{{ startupTime }}</div>
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
const { shell } = require("electron"); // deconstructing assignment

let timeout;

export default {
  data() {
    return {
      clientInfo: null,
      isDestroyed: false
    };
  },
  name: "InformationPage",
  computed: {
    startupTime() {
      return this.formatDate(this.clientInfo.startup_timestamp);
    },
    lastBlockTime() {
      return this.formatDate(this.clientInfo.chain_tip_time);
    },
    numberOfConnections() {
      let connections_in = parseInt(this.clientInfo.num_connections_in);
      let connections_out = parseInt(this.clientInfo.num_connections_out);

      return `${connections_in + connections_out} (In: ${connections_in} / Out: ${connections_out})`;
    }
  },
  created() {
    this.updateClientInfo();
  },
  beforeDestroy() {
    this.isDestroyed = true;
    clearTimeout(timeout);
  },
  methods: {
    updateClientInfo() {
      clearTimeout(timeout);
      this.clientInfo = LibraryController.GetClientInfo();
      if (this.isDestroyed) return;
      timeout = setTimeout(this.updateClientInfo, 10 * 1000);
    },
    formatDate(timestamp) {
      const date = new Date(timestamp * 1000);
      return date.toLocaleString(this.$i18n.locale, {
        weekday: "short",
        year: "numeric",
        month: "long",
        day: "numeric",
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit"
      });
    },
    openFile(path) {
      shell.openPath(path);
    }
  }
};
</script>

<style lang="less" scoped>
.information-page {
  width: 100%;
  height: 100%;

  & h4 {
    margin-bottom: 5px;
  }
  & h4:not(:first) {
    margin-top: 5px;
  }

  & .flex-row > div {
    font-size: 0.85rem;
    line-height: 20px;
    margin-bottom: 5px;
  }
  & .flex-row :first-child {
    flex: 0 0 180px;
  }
  & .flex-row :last-child {
    flex: 1;
    overflow-wrap: break-word;
    word-break: break-word;
  }
}

.list-item-icon {
  max-width: 30px;
  cursor: pointer;
}

.path-row {
  display: flex;
  flex-direction: row;
}

.selectable {
  user-select: all;
}
</style>
