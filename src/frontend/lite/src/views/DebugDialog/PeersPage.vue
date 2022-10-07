<template>
  <div class="layout">
    <div>
      <div class="header">
        <h4>{{ $t("peers.node_id") }}</h4>
        <h4>{{ $t("peers.node_service") }}</h4>
        <h4>{{ $t("peers.user_agent") }}</h4>
      </div>
      <div class="list scrollable">
        <div v-for="peer in peerInfo.peers" :key="peer.id" @click="showPeerDetails(peer)" @contextmenu.prevent="onRightClick">
          <div>{{ peer.id }}</div>
          <div>{{ peer.ip }}</div>
          <div>{{ peer.userAgent }}</div>
        </div>
      </div>
    </div>
    <div v-if="hasBannedPeers">
      <div class="header">
        <h4>{{ $t("peers.banned_peers") }}</h4>
        <h4>{{ $t("peers.reason") }}</h4>
      </div>
      <div class="list scrollable">
        <div v-for="peer in peerInfo.banned" :key="peer.id" @click="showPeerDetails(peer)" @contextmenu.prevent="onRightClick">
          <div>{{ peer.address }}</div>
          <div>{{ peer.reason }}</div>
        </div>
      </div>
    </div>
    <div class="buttons" v-if="hasBannedPeers">
      <button @click="clearBannedPeers" v-if="peerInfo.banned.length > 0" class="clear-banned" outlined small>
        {{ $t("peers.buttons.clear_banned") }}
      </button>
    </div>
  </div>
</template>

<script>
import { P2pNetworkController } from "@/unity/Controllers";
import PeerDetailsDialog from "./PeerDetailsDialog";
import EventBus from "@/EventBus";

let timeout;

export default {
  data() {
    return {
      peerInfo: {
        peers: [],
        banned: []
      },
      isDestroyed: false
    };
  },
  name: "PeersPage2",
  computed: {
    hasBannedPeers() {
      return this.peerInfo.banned.length;
    }
  },
  methods: {
    onRightClick() {
      // TODO: Right Click
      // Add Context menu on Right Click
    },
    getPeers() {
      clearTimeout(timeout);

      this.peerInfo = {
        peers: P2pNetworkController.GetPeerInfo(),
        banned: P2pNetworkController.ListBannedPeers()
      };

      if (this.isDestroyed) return;
      timeout = setTimeout(this.getPeers, 10 * 1000);
    },
    showPeerDetails(peer) {
      EventBus.$emit("show-dialog", {
        title: this.$t("peers.peer_details"),
        component: PeerDetailsDialog,
        componentProps: {
          peer: peer
        },
        noMargin: true,
        showButtons: false
      });
    },
    async clearBannedPeers() {
      await P2pNetworkController.ClearBannedAsync();
      this.getPeers();
    }
  },
  created() {
    this.getPeers();
  },
  mounted() {
    EventBus.$on("close-dialog", this.getPeers);
  },
  beforeDestroy() {
    this.isDestroyed = true;
    clearTimeout(timeout);
    EventBus.$off("close-dialog", this.getPeers);
  }
};
</script>

<style lang="less" scoped>
.layout {
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;

  & > div:not(.buttons) {
    flex: 1 0 0;
    margin-bottom: 10px;

    display: flex;
    flex-direction: column;
  }

  & > div.buttons {
    text-align: right;
  }
}

.header,
.list > div {
  display: flex;

  & :first-child {
    flex: 1 0 0;
  }

  & :not(:first-child) {
    flex: 2 0 0;
  }
}

.list {
  border: solid 0.5px #888888;
  padding: 0 5px;
  flex: 1 0 0;

  & > div {
    cursor: pointer;
    padding: 5px 0;
  }

  & > div:hover {
    color: var(--primary-color);
    background: var(--hover-color);
  }
}

h4 {
  margin-bottom: 5px;
}
</style>
