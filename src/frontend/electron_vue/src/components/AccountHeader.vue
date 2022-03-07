<template>
  <div class="account-header">
    <div v-if="!editMode" class="display-mode" @click="editName">
      <div class="flex flex-row">
        <div class="name ellipsis flex-1">
          {{ name }}
        </div>
        <div class="icon-bar">
          <fa-icon :icon="['fal', 'fa-pen']" />
        </div>
      </div>
      <div class="balance ellipsis">
        {{ balance }}
      </div>
    </div>
    <div v-else class="flex flex-row">
      <input class="flex-1" ref="accountNameInput" type="text" v-model="newAccountName" @keydown="onKeydown" />
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { AccountsController } from "../unity/Controllers";
import { formatMoneyForDisplay } from "../util.js";

export default {
  name: "AccountHeader",
  data() {
    return {
      editMode: false,
      newAccountName: null
    };
  },
  props: {
    account: {
      type: Object,
      default: () => {}
    }
  },
  computed: {
    ...mapState("app", ["rate", "activityIndicator"]),
    name() {
      return this.account.label;
    },
    balance() {
      return `${this.balanceForDisplay} ${this.totalBalanceFiat}`;
    },
    totalBalanceFiat() {
      if (!this.rate) return "";
      return `€ ${formatMoneyForDisplay(this.account.balance * this.rate)}`;
    },
    balanceForDisplay() {
      if (this.account.balance == null) return "";
      return formatMoneyForDisplay(this.account.balance);
    }
  },
  watch: {
    name: {
      immediate: true,
      handler() {
        this.editMode = false;
      }
    }
  },
  methods: {
    editName() {
      this.newAccountName = this.name;
      this.editMode = true;
      this.$nextTick(() => {
        this.$refs["accountNameInput"].focus();
      });
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
    }
  }
};
</script>

<style scoped>
.account-header {
  width: 100%;
  height: var(--header-height);
  line-height: 20px;
  padding: calc((var(--header-height) - 40px) / 2) 0;
}

.display-mode {
  cursor: pointer;
}

.name {
  font-size: 1.1em;
  font-weight: 500;
  line-height: 20px;
}

.account-header:hover .icon-bar {
  display: block;
}

.icon-bar {
  display: none;
  cursor: pointer;
}
</style>
