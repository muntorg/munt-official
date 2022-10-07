<template>
  <div class="modal-mask flex-col" v-if="visible" @click="onCancel">
    <div class="modal-container" @click.stop>
      <div class="header">
        <span>{{ $t(this.options.title) }}</span>
        <div class="close" @click="onCancel">
          <span class="icon">
            <fa-icon :icon="['fal', 'times']" />
          </span>
        </div>
      </div>
      <div class="content">
        <app-form-field title="unlock_wallet_dialog.unlock_for" v-if="options.timeout == null">
          <select-list :options="timeoutOptions" :default="timeoutOptions[0]" v-model="timeout" />
        </app-form-field>
        <content-wrapper :content="options.message">
          <app-form-field title="common.password">
            <input ref="pwd" v-model="password" type="password" @keydown="resetStatus" @keydown.enter="tryUnlock" :class="passwordStatus" />
          </app-form-field>
        </content-wrapper>
      </div>
      <app-button-section class="buttons">
        <template v-slot:right>
          <button @click="tryUnlock">{{ $t("buttons.unlock") }}</button>
        </template>
      </app-button-section>
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { LibraryController } from "../unity/Controllers";
import EventBus from "../EventBus";

export default {
  name: "UnlockWalletDialog",
  data() {
    return {
      visible: false,
      isUnlocked: null,
      timeout: 1,
      options: {},
      password: "",
      error: false
    };
  },
  mounted() {
    EventBus.$on("unlock-wallet", this.unlockWallet);
    EventBus.$on("lock-wallet", this.lockWallet);
  },
  beforeDestroy() {
    EventBus.$off("unlock-wallet", this.unlockWallet);
    EventBus.$off("lock-wallet", this.lockWallet);
  },
  computed: {
    ...mapState("wallet", ["unlocked"]),
    timeoutOptions() {
      return [1, 5, 10].map(i => {
        return { value: 60 * i, label: this.$tc("unlock_wallet_dialog.minute", i) };
      });
    },
    passwordStatus() {
      return this.error ? "error" : "";
    }
  },
  methods: {
    async lockWallet() {
      await LibraryController.LockWalletAsync();
    },
    async unlockWallet(options) {
      // note:
      // it can happen if unlocked is true now, but is false right after the callback
      // find a solution for this situation...
      // maybe LibraryController.GetWalletLockStatus can be used to determine if wallet is still unlocked for a while?
      if (this.unlocked) {
        if (typeof options.callback === "function") {
          return options.callback();
        }
      }

      this.options = {
        title: "unlock_wallet_dialog.title",
        message: null,
        timeout: options && options.callback ? 10 : null,
        ...options
      };

      this.visible = true;
      this.$nextTick(() => {
        this.$refs.pwd.focus();
      });
    },
    onCancel() {
      this.password = null;
      this.visible = false;
      this.error = false;
    },
    async resetStatus() {
      this.error = false;
    },
    async tryUnlock() {
      var result = await LibraryController.UnlockWalletAsync(this.password, this.options.timeout || this.timeout.value);
      if (result) {
        this.password = null;
        this.visible = false;

        if (this.options.callback) {
          // call and await the callback function
          await this.options.callback();

          // lock the wallet after callback finishes
          LibraryController.LockWallet();
        }
      } else {
        this.error = true;
      }
    }
  }
};
</script>

<style lang="less" scoped>
.modal-mask {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background-color: rgba(0, 0, 0, 0.5);
  margin-top: 0;
  margin-left: 0;
  align-items: center;
  justify-content: center;
  transition: opacity 0.3s ease;
  z-index: 9998;
}

.modal-container {
  max-width: 800px;
  max-height: 700px;
  width: calc(100% - 40px);
  height: auto;
  margin: 0px auto;
  background-color: #fff;
  box-shadow: 0 2px 10px rgba(0, 0, 0, 0.2);
  transition: all 0.3s ease;
}

.header {
  height: 56px;
  line-height: 56px;
  padding: 0 30px;
  border-bottom: 1px solid var(--main-border-color);
  font-weight: 600;
  font-size: 1.05rem;

  & .close {
    float: right;
    margin: 0 -10px 0 0;
  }

  & .icon {
    line-height: 42px;
    font-size: 1.2em;
    font-weight: 300;
    padding: 0 10px;
    cursor: pointer;
  }

  & .icon:hover {
    color: var(--primary-color);
    background: var(--hover-color);
  }
}

.content {
  margin: 30px 0;
  padding: 0 30px;
  overflow-y: auto;
}

.buttons {
  height: 64px;
  padding: 0 12px;
}
</style>
