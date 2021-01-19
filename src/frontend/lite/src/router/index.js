import Vue from "vue";
import VueRouter from "vue-router";
import Account from "../views/Account";

import { AccountsController } from "../unity/Controllers";
import EventBus from "../EventBus";

Vue.use(VueRouter);

const routes = [
  {
    path: "/account/:id?",
    name: "account",
    props: true,
    component: Account,
    children: [
      {
        path: "",
        name: "transactions",
        component: () =>
          import(
            /* webpackChunkName: "account-transactions" */ "../views/Account/SpendingAccount/Transactions.vue"
          )
      },
      {
        path: "send",
        name: "send",
        component: () =>
          import(
            /* webpackChunkName: "account-send" */ "../views/Account/SpendingAccount/Send.vue"
          )
      },
      {
        path: "receive",
        name: "receive",
        component: () =>
          import(
            /* webpackChunkName: "account-receive" */ "../views/Account/SpendingAccount/Receive.vue"
          )
      }
    ]
  },
  {
    path: "/setup",
    name: "setup",
    component: () =>
      import(/* webpackChunkName: "setup" */ "../views/Setup.vue"),
    meta: {
      layout: "setup-layout"
    }
  },
  {
    path: "/settings",
    component: () =>
      import(/* webpackChunkName: "settings" */ "../views/Settings"),
    children: [
      {
        path: "",
        name: "settings",
        component: () =>
          import(
            /* webpackChunkName: "settings-list" */ "../views/Settings/SettingsList.vue"
          )
      },
      {
        path: "view-recovery-phrase",
        name: "view-recovery-phrase",
        component: () =>
          import(
            /* webpackChunkName: "view-recovery-phrase" */ "../views/Settings/ViewRecoveryPhrase.vue"
          )
      },
      {
        path: "change-password",
        name: "change-password",
        component: () =>
          import(
            /* webpackChunkName: "change-password" */ "../views/Settings/ChangePassword.vue"
          )
      }
    ]
  },
  {
    path: "/debug",
    name: "debug",
    component: () =>
      import(/* webpackChunkName: "debug-dialog" */ "../views/DebugDialog")
  }
];

const router = new VueRouter({
  routes
});

router.beforeEach((to, from, next) => {
  console.log(`route to ${to.name}`);
  switch (to.name) {
    case "account":
      if (to.params.id !== undefined) {
        console.log(`account: ${to.params.id}`);
        // set active account to specified id
        AccountsController.SetActiveAccount(to.params.id);

        // close the right sidebar when switching accounts
        EventBus.$emit("close-right-sidebar");
      } else {
        console.log("account: undefined");
      }
      break;
  }
  next();
});

export default router;
