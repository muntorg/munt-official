<template>
  <div class="account-view" v-if="account">
    <div class="main-section">
      <div class="header">
        <div class="info">
          <div class="label">{{ account.label }}</div>
          <div class="balance">{{ account.balance }}</div>
        </div>
        <div class="settings">
          <span>
            <fa-icon :icon="['fal', 'cog']" />
          </span>
        </div>
      </div>
      <div class="content">
        <mutation-list :mutations="mutations" />
      </div>
      <div class="footer">
        <span
          class="button"
          @click="setRightSection('Send')"
          v-if="rightSection !== 'Send'"
        >
          <fa-icon :icon="['fal', 'arrow-from-bottom']" />
          {{ $t("buttons.send") }}
        </span>
        <span
          class="button"
          @click="setRightSection('Receive')"
          v-if="rightSection !== 'Receive'"
        >
          <fa-icon :icon="['fal', 'arrow-to-bottom']" />
          {{ $t("buttons.receive") }}
        </span>
      </div>
    </div>
    <div class="right-section" v-if="rightSection">
      <div class="header">
        <div class="title">
          {{ rightSectionTitle }}
        </div>
        <div class="close" @click="rightSection = null">
          <fa-icon :icon="['fal', 'times']" />
        </div>
      </div>
      <component class="component" :is="rightSectionComponent" />
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import MutationList from "./MutationList";
import SendNovo from "./SendNovo";
import ReceiveNovo from "./ReceiveNovo";

export default {
  name: "SpendingAccount",
  props: {
    account: null
  },
  data() {
    return {
      rightSection: null,
      rightSectionComponent: null
    };
  },
  components: {
    MutationList,
    SendNovo,
    ReceiveNovo
  },
  computed: {
    ...mapState(["mutations"]),
    rightSectionTitle() {
      switch (this.rightSection) {
        case "Send":
          return this.$t("buttons.send");
        case "Receive":
          return this.$t("buttons.receive");
        default:
          return null;
      }
    }
  },
  methods: {
    setRightSection(name) {
      switch (name) {
        case "Send":
          this.rightSectionComponent = SendNovo;
          break;
        case "Receive":
          this.rightSectionComponent = ReceiveNovo;
          break;
      }
      this.rightSection = name;
    }
  }
};
</script>

<style lang="less" scoped>
.account-view {
  height: 100vh;
  display: flex;
  flex-direction: row;
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
    border-top: 1px solid var(--main-border-color);
    padding: 10px 0;
    text-align: center;

    & svg {
      font-size: 14px;
      margin-right: 5px;
    }

    & .button {
      display: inline-block;
      padding: 0 20px 0 20px;
      line-height: 32px;
      font-weight: 500;
      font-size: 1em;
      color: #009572;
      text-align: center;
      cursor: pointer;

      &:hover {
        background-color: #f5f5f5;
      }
    }
  }
}

.right-section {
  width: 50%;
  min-width: 380px;
  background-color: #f5f5f5;
  padding: 0 24px;

  & .header {
    line-height: 62px;
    font-size: 1.1em;
    font-weight: 500;

    display: flex;
    flex-direction: row;

    & .title {
      flex: 1;
    }

    & .close {
      cursor: pointer;
    }
  }

  & .component {
    height: calc(100% - 72px);
  }
}
</style>
