<template>
  <div>
    <confirm-dialog v-model="modal" />
    <div class="account-header">
      <div v-if="!editMode" class="flex-row">
        <div v-if="isSingleAccount" class="flex-row flex-1">
          <div class="logo" />
          <div class="balance-row flex-1">
            <span>{{ balanceForDisplay }}</span>
            <span>{{ totalBalanceFiat }}</span>
          </div>
        </div>
        <div v-else class="left-colum">
          <div>
            <account-tooltip type="Account" :account="account">
              <div style="display: flex; flex-direction: row">
                <div style="width: calc(100% - 45px)" @click="editName" class="flex-row flex-1">
                  <div class="accountname ellipsis">{{ name }}</div>
                  <fa-icon class="pen" :icon="['fal', 'fa-pen']" />
                </div>
                <div style="width: 40px; text-align: center" @click="deleteAccount" class="trash flex-row">
                  <fa-icon :icon="['fal', 'fa-trash']" />
                </div>
              </div>
              <div class="balance-row">
                <span>{{ balanceForDisplay }}</span>
                <span>{{ totalBalanceFiat }}</span>
              </div>
            </account-tooltip>
          </div>
        </div>
        <div v-if="showBuySellButtons">
          <button outlined class="small" @click="buyCoins" :disabled="buyDisabled">{{ $t("buttons.buy") }}</button>
          <button outlined class="small" @click="sellCoins" :disabled="sellDisabled">{{ $t("buttons.sell") }}</button>
        </div>
        <div v-if="!showBuySellButtons && showHoldinButtons">
          <div v-if="!isLinkedToHoldin">
            <button outlined class="small" @click="linkToHoldin('add')" :disabled="sellDisabled">
              {{ $t("saving_account.add_to_holdin") }}
            </button>
          </div>
          <div v-else>
            <button outlined class="small" @click="linkToHoldin('remove')" :disabled="sellDisabled">
              {{ $t("saving_account.remove_from_holdin") }}
            </button>
          </div>
        </div>
        <div v-if="isSingleAccount" class="flex-row icon-buttons">
          <div class="icon-button">
            <fa-icon :icon="['fal', 'cog']" @click="showSettings" />
          </div>
          <div class="icon-button">
            <fa-icon :icon="['fal', lockIcon]" @click="changeLockSettings" />
          </div>
        </div>
      </div>
      <input v-else ref="accountNameInput" type="text" v-model="newAccountName" @keydown="onKeydown" @blur="cancelEdit" />
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { AccountsController, BackendUtilities, WitnessController, GenerationController } from "../unity/Controllers";
import { formatMoneyForDisplay } from "../util.js";
import AccountTooltip from "./AccountTooltip.vue";
import EventBus from "../EventBus";
import { apiKey } from "../../holdinAPI";
import ConfirmDialog from "./ConfirmDialog.vue";

export default {
  components: { AccountTooltip, ConfirmDialog },
  name: "AccountHeader",
  data() {
    return {
      editMode: false,
      newAccountName: null,
      buyDisabled: false,
      sellDisabled: false,
      requestLinkToHoldin: false,
      isLinkedToHoldin: false,
      keyHash: "",
      modal: null
    };
  },
  props: {
    account: {
      type: Object,
      default: () => {}
    },
    isSingleAccount: {
      type: Boolean,
      default: false
    }
  },
  computed: {
    ...mapState("app", ["rate", "currency"]),
    ...mapState("wallet", ["walletPassword", "unlocked"]),
    name() {
      return this.account ? this.account.label : null;
    },
    totalBalanceFiat() {
      if (!this.rate) return "";
      return `${this.currency.symbol || ""} ${formatMoneyForDisplay(this.account.balance * this.rate, true)}`;
    },
    balanceForDisplay() {
      if (!this.account || this.account.balance === undefined) return "";
      return formatMoneyForDisplay(this.account.balance);
    },
    showBuySellButtons() {
      return !this.account || (this.account.type === "Desktop" && !this.editMode);
    },
    showHoldinButtons() {
      return apiKey;
    },
    lockIcon() {
      return this.unlocked ? "unlock" : "lock";
    }
  },
  watch: {
    name: {
      immediate: true,
      handler() {
        this.editMode = false;
      }
    },
    unlocked: {
      immediate: true,
      handler() {
        if (this.unlocked && this.requestLinkToHoldin) {
          // Check if add or remove.
          if (this.isLinkedToHoldin) {
            this.holdinAPI("remove");
          } else {
            this.holdinAPI("add");
          }
        }
      }
    },
    account: {
      immediate: true,
      handler() {
        if (!this.showBuySellButtons) {
          this.checkForHoldinLink();
        }
      }
    }
  },
  mounted() {
    if (!this.showBuySellButtons) {
      this.checkForHoldinLink();
    }
  },
  methods: {
    checkForHoldinLink() {
      AccountsController.ListAccountLinksAsync(this.account.UUID).then(result => {
        const findHoldin = result.find(element => element.serviceName == "holdin");
        this.isLinkedToHoldin = findHoldin && findHoldin.serviceName === "holdin";
        if (this.isLinkedToHoldin) {
          this.keyHash = findHoldin.serviceData;
        }
      });
    },
    editName() {
      this.newAccountName = this.name;
      this.editMode = true;
      this.$nextTick(() => {
        this.$refs["accountNameInput"].focus();
      });
    },
    deleteAccount() {
      this.showConfirmModal();
    },
    onKeydown(e) {
      switch (e.keyCode) {
        case 13:
          this.changeAccountName();
          break;
        case 27:
          this.editMode = false;
          break;
      }
    },
    changeAccountName() {
      if (this.newAccountName !== this.account.label) {
        AccountsController.RenameAccount(this.account.UUID, this.newAccountName);
      }
      this.editMode = false;
    },
    cancelEdit() {
      this.editMode = false;
    },
    dismissIndicator() {
      setTimeout(() => {
        this.$store.dispatch("app/SET_ACTIVITY_INDICATOR", false);
      }, 1000);
    },
    async sellCoins() {
      try {
        this.sellDisabled = true;
        const url = await BackendUtilities.GetSellSessionUrl();
        window.open(url, "sell-coins");
      } finally {
        this.sellDisabled = false;
      }
    },
    async buyCoins() {
      try {
        this.buyDisabled = true;
        const url = await BackendUtilities.GetBuySessionUrl();
        window.open(url, "buy-coins");
      } finally {
        this.buyDisabled = false;
      }
    },
    showSettings() {
      if (this.$route.path === "/settings/") return;
      this.$router.push({ name: "settings" });
    },
    changeLockSettings() {
      EventBus.$emit(this.unlocked ? "lock-wallet" : "unlock-wallet");
    },
    linkToHoldin(action) {
      this.requestLinkToHoldin = true;

      if (!this.unlocked) {
        EventBus.$emit("unlock-wallet");
      } else {
        this.holdinAPI(action);
      }
    },
    async holdinAPI(action) {
      this.$store.dispatch("app/SET_ACTIVITY_INDICATOR", true);
      AccountsController.GetWitnessKeyURIAsync(this.account.UUID).then(async key => {
        let result = null;

        if (action === "add") {
          let infoResult = await BackendUtilities.holdinAPIActions(key, "getinfo");
          const keyHashLocal = infoResult.data.keyhash;

          if (infoResult.data.available === 1 && infoResult.data.active === "1") {
            // Account was linked elsewhere. Note that on Gulden and update the compound value.
            GenerationController.GetGenerationAddressAsync()
              .then(async payoutAddress => {
                // Add payout address to Holdin..
                await BackendUtilities.holdinAPIActions(key, "payoutaddress", payoutAddress);
                this.addAccountLink(this.account.UUID, keyHashLocal, infoResult.data.compound);
              })
              .catch(err => {
                alert(err.message);
                this.dismissIndicator();
              });
          } else if (infoResult.data.available === 1 && infoResult.data.active === "0") {
            // Account was linked and then removed. Reactivate.
            GenerationController.GetGenerationAddressAsync()
              .then(async payoutAddress => {
                result = await BackendUtilities.holdinAPIActions(key, "activate");
                if (result.status_code === 200) {
                  await BackendUtilities.holdinAPIActions(key, "payoutaddress", payoutAddress);
                  this.addAccountLink(this.account.UUID, keyHashLocal);
                } else {
                  alert(`Holdin: ${result.status_message}`);
                }
              })
              .catch(err => {
                alert(err.message);
                this.dismissIndicator();
              });
          } else if (infoResult.data.available === 0) {
            // Add account for the first time.
            result = await BackendUtilities.holdinAPIActions(key, "add");
            if (result.status_code === 200) {
              this.addAccountLink(this.account.UUID, keyHashLocal);
            } else {
              this.dismissIndicator();
              alert(`Holdin: ${result.status_message}`);
            }
          } else {
            this.dismissIndicator();
            alert("Holdin: API Error");
          }
        } else {
          result = await BackendUtilities.holdinAPIActions(key, "remove");
          if (result.status_code === 200) {
            AccountsController.RemoveAccountLinkAsync(this.account.UUID, "holdin")
              .then(() => {
                this.isLinkedToHoldin = false;
                this.requestLinkToHoldin = false;
                this.dismissIndicator();
              })
              .catch(err => {
                alert(err.message);
                this.dismissIndicator();
              });
          } else {
            alert(`Holdin: ${result.status_message}`);
          }
        }
      });
    },
    addAccountLink(accountUID, keyHash, compound) {
      AccountsController.AddAccountLinkAsync(accountUID, "holdin", keyHash)
        .then(() => {
          if (compound) {
            WitnessController.SetAccountCompoundingAsync(accountUID, compound).then(() => {
              this.dismissIndicator();
              this.requestLinkToHoldin = false;
              this.isLinkedToHoldin = true;
            });
          } else {
            this.dismissIndicator();
            this.requestLinkToHoldin = false;
            this.isLinkedToHoldin = true;
          }
        })
        .catch(err => {
          alert(err.message);
        });
    },
    showConfirmModal() {
      if (
        this.account.allBalances.availableIncludingLocked === 0 &&
        this.account.allBalances.unconfirmedIncludingLocked === 0 &&
        this.account.allBalances.immatureIncludingLocked === 0
      ) {
        this.modal = { title: "Confirm Delete", message: "Are you sure you want to delete the account?", closeModal: this.closeModal, confirm: this.confirm };
      } else {
        this.modal = { title: "Error", message: "Your account needs to be empty before you can delete it", showButtons: false, closeModal: this.closeModal };
      }
    },
    closeModal() {
      this.modal = null;
    },
    confirm() {
      this.$store.dispatch("app/SET_ACTIVITY_INDICATOR", true);
      this.modal = null;
      setTimeout(() => {
        AccountsController.DeleteAccountAsync(this.account.UUID)
          .then(() => {
            this.$store.dispatch("app/SET_ACTIVITY_INDICATOR", false);
          })
          .catch(err => {
            this.$store.dispatch("app/SET_ACTIVITY_INDICATOR", false);
            alert(err.message);
          });
      }, 1000);
    }
  }
};
</script>

<style lang="less" scoped>
.account-header {
  width: 100%;
  height: var(--header-height);
  line-height: var(--header-height);

  & > div {
    align-items: center;
    justify-content: center;
    height: var(--header-height);
  }
}

.left-colum {
  display: flex;
  flex-direction: column;
  flex: 1;
  white-space: nowrap;
  overflow: hidden;
  height: 40px;
  cursor: pointer;
  position: relative;
}

.balance-row {
  line-height: 20px;
  font-size: 0.9em;

  & :first-child {
    margin-right: 10px;
  }
}

button.small {
  height: 20px !important;
  line-height: 20px !important;
  font-size: 10px !important;
  padding: 0 10px !important;
  margin-left: 5px;
}

.accountname {
  flex: 1;
  font-size: 1em;
  font-weight: 500;
  line-height: 20px;
  margin-right: 30px;
}

.pen {
  display: none;
  position: absolute;
  right: 45px;
  line-height: 20px;
}

.trash {
  display: none;
  position: absolute;
  right: 5px;
  line-height: 18px;
}

.left-colum:hover .trash {
  display: block;
  color: #ff0000;
}

.left-colum:hover .pen {
  display: block;
}

.logo {
  width: 22px;
  min-width: 22px;
  height: 22px;
  min-height: 22px;
  background: url("../img/logo-black.svg"), linear-gradient(transparent, transparent);
  background-size: cover;
  margin-right: 10px;
}

.icon-buttons {
  margin-left: 10px;
}

.icon-button {
  display: inline-block;
  padding: 0 13px;
  line-height: 40px;
  height: 40px;
  font-weight: 500;
  font-size: 1em;
  color: var(--primary-color);
  text-align: center;
  cursor: pointer;

  &:hover {
    background-color: #f5f5f5;
  }
}
</style>
