<template>
  <div class="peer-details-dialog">
    <activity-indicator v-if="loading" :paddingHidden="true" />
    <div class="peer-info scrollable">
      <div v-for="(value, propertyName) in peerInfo" :key="propertyName" class="row">
        <h4>{{ $t(`peers.${propertyName}`) }}</h4>
        <div>{{ value }}</div>
      </div>
    </div>
    <div class="buttons">
      <button @click="unBanPeer(peer)" outlined small v-if="isBanned">
        {{ $t("peers.buttons.un_ban") }}
      </button>
      <button @click="disconnectPeer(peer)" outlined small v-if="!isBanned">
        {{ $t("peers.buttons.disconnect") }}
      </button>

      <div style="display: flex" v-if="!isBanned">
        <div>{{ $t("peers.ban") }}</div>
        <button @click="banPeer(3600)" outlined small>
          {{ $t("peers.buttons.ban_hour") }}
        </button>
        <button @click="banPeer(86400)" outlined small>
          {{ $t("peers.buttons.ban_day") }}
        </button>
        <button @click="banPeer(604800)" outlined small>
          {{ $t("peers.buttons.ban_week") }}
        </button>
        <button @click="banPeer(31449600)" outlined small>
          {{ $t("peers.buttons.ban_year") }}
        </button>
      </div>
    </div>
  </div>
</template>

<script>
import EventBus from "@/EventBus";
import { P2pNetworkController } from "@/unity/Controllers";
import ActivityIndicator from "@/components/ActivityIndicator.vue";

export default {
  data() {
    return {
      loading: false
    };
  },
  name: "PeerDetailsDialog",
  props: {
    peer: {
      type: Object,
      default: null
    }
  },
  components: {
    ActivityIndicator
  },
  computed: {
    isBanned() {
      return this.peer.banned_from !== undefined;
    },
    peerInfo() {
      const peer = this.peer;

      return this.isBanned
        ? {
            address: peer.address,
            banned_from: this.formatDate(peer.banned_from),
            banned_until: this.formatDate(peer.banned_until),
            reason: peer.reason
          }
        : {
            node_id: peer.id,
            node_service: peer.ip,
            whitelisted: peer.whitelisted ? "Yes" : "No",
            direction: peer.inbound ? "Inbound" : "Outbound",
            user_agent: peer.userAgent,
            services: peer.services,
            starting_block: peer.start_height,
            synced_headers: peer.synced_height,
            synced_blocks: peer.common_height,
            ban_score: peer.banscore,
            connection_time: this.formatDateFrom(peer.time_connected),
            last_send: this.formatDateFrom(peer.last_send),
            last_receive: this.formatDateFrom(peer.last_receive),
            sent: peer.send_bytes,
            received: peer.receive_bytes,
            ping_time: peer.latency,
            time_offset: peer.time_offset
          };
    }
  },
  methods: {
    formatDateFrom(date) {
      const now = Date.now() / 1000;
      return parseInt(now - date);
    },
    formatDate(date) {
      const dateTimeString = new Date(date * 1000).toLocaleString();
      return dateTimeString;
    },
    async disconnectPeer() {
      this.loading = true;
      const result = await P2pNetworkController.DisconnectPeerAsync(this.peer.id);
      this.closeDialog(result);
    },
    async banPeer(interval) {
      this.loading = true;
      const result = await P2pNetworkController.BanPeerAsync(this.peer.addrBind, interval);
      this.closeDialog(result);
    },
    async unBanPeer() {
      this.loading = true;
      const result = await P2pNetworkController.UnbanPeerAsync(this.peer.address);
      this.closeDialog(result);
    },
    closeDialog(result) {
      console.log(result); // todo: do something usefull with result

      setTimeout(() => {
        this.loading = false;
        EventBus.$emit("close-dialog");
      }, 1000);
    }
  }
};
</script>

<style lang="less" scoped>
.peer-details-dialog {
  overflow: hidden;
  display: flex;
  flex-direction: column;
  max-height: calc(100vh - 100px);
  margin-top: 10px;

  & > div {
    padding: 0 30px;
  }
}

.peer-info {
  flex: 1;
}

.row {
  display: flex;

  & > h4 {
    flex: 1;
    margin-bottom: 10px;
  }

  & > div {
    flex: 1;
  }
}

.buttons {
  line-height: 50px;
  height: 50px;
  display: flex;
  align-items: center;
  justify-content: flex-end;

  & > div {
    flex: 1 0 0;
    display: flex;
    align-items: center;
    justify-content: flex-end;

    & > button {
      margin-left: 10px;
      min-width: 60px;
    }
  }
}
</style>
