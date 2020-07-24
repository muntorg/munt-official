<template>
  <component :is="accountType" :account="account" />
</template>

<script>
import { mapGetters } from "vuex";

import SpendingAccount from "./SpendingAccount";
import HoldingAccount from "./HoldingAccount";

export default {
  name: "Account",
  data() {
    return {
      accountType: "div"
    };
  },
  props: {
    id: null
  },
  components: {
    SpendingAccount,
    HoldingAccount
  },
  computed: {
    ...mapGetters(["account"])
  },
  watch: {
    account() {
      this.onAccountChanged();
    }
  },
  mounted() {
    this.onAccountChanged();
  },
  methods: {
    onAccountChanged() {
      if (this.account) {
        switch (this.account.type) {
          case "Desktop":
            this.accountType = SpendingAccount;
            break;
          case "Witness":
            this.accountType = HoldingAccount;
            break;
          case "Mining":
            this.accountType = SpendingAccount;
            break;
          default:
            this.accountType = "div";
            break;
        }
      }
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
    padding: 11px 30px 0 30px;

    display: flex;
    flex-direction: row;

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
  }

  & .content {
    flex: 1;
    padding: 30px;
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
  width: 400px;
  background-color: #f5f5f5;
  padding: 0 30px;

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
