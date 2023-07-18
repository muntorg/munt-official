import Vue from "vue";
import VueRouter from "vue-router";
import Account from "../views/Account";

import { AccountsController } from "../unity/Controllers";
import store from "../store";

Vue.use(VueRouter);

const routes = [
  {
    path: "/account/:id?",
    name: "account",
    props: true,
    component: Account,
    children: [
      {
        path: "send",
        name: "send",
        component: () => import(/* webpackChunkName: "account-send" */ "../views/Account/SpendingAccount/Send.vue")
      },
      {
        path: "receive",
        name: "receive",
        component: () => import(/* webpackChunkName: "account-receive" */ "../views/Account/SpendingAccount/Receive.vue")
      },
      {
        path: "link-saving-account",
        name: "link-saving-account",
        component: () => import(/* webpackChunkName: "link-saving-account" */ "../views/Account/SavingAccount/LinkSavingAccount.vue")
      },
      {
        path: "send-saving",
        name: "send-saving",
        component: () => import(/* webpackChunkName: "send-saving" */ "../views/Account/MiningAccount/Send.vue")
      },
      {
        path: "renew-account",
        name: "renew-account",
        component: () => import(/* webpackChunkName: "renew-account" */ "../views/Account/SavingAccount/RenewAccount.vue")
      },
      {
        path: "optimise-account",
        name: "optimise-account",
        component: () => import(/* webpackChunkName: "optimise-account" */ "../views/Account/SavingAccount/OptimiseAccount.vue")
      },
      {
        path: "transactions",
        name: "transactions",
        component: () => import(/* webpackChunkName: "transactions" */ "../views/Account/SpendingAccount/Transactions.vue")
      }
    ]
  },
  {
    path: "/add-saving-account",
    name: "add-saving-account",
    component: () => import(/* webpackChunkName: "add-saving-account" */ "../views/Account/SavingAccount/AddSavingAccount.vue")
  },
  {
    path: "/add-spending-account",
    name: "add-spending-account",
    component: () => import(/* webpackChunkName: "add-saving-account" */ "../views/Account/SpendingAccount/AddSpendingAccount.vue")
  },
  {
    path: "/setup",
    name: "setup",
    component: () => import(/* webpackChunkName: "setup" */ "../views/Setup.vue"),
    meta: {
      layout: "setup-layout"
    }
  },
  {
    path: "/settings",
    component: () => import(/* webpackChunkName: "settings" */ "../views/Settings"),
    children: [
      {
        path: "",
        name: "settings",
        component: () => import(/* webpackChunkName: "settings-list" */ "../views/Settings/SettingsList.vue")
      },
      {
        path: "view-recovery-phrase",
        name: "view-recovery-phrase",
        component: () => import(/* webpackChunkName: "view-recovery-phrase" */ "../views/Settings/ViewRecoveryPhrase.vue")
      },
      {
        path: "change-password",
        name: "change-password",
        component: () => import(/* webpackChunkName: "change-password" */ "../views/Settings/ChangePassword.vue")
      }
    ]
  },
  {
    path: "/debug",
    name: "debug",
    component: () => import(/* webpackChunkName: "debug-dialog" */ "../views/DebugDialog")
  },
  {
    path: "/about",
    name: "about",
    component: () => import(/* webpackChunkName: "about-dialog" */ "../views/AboutDialog")
  }
];

const router = new VueRouter({
  routes
});

router.beforeEach((to, from, next) => {
  console.log(`route to ${to.name}`);

  if (to.name === "account" && to.params.id !== undefined) {
    // If the id of the account doesn't change there is no need to call SetActiveAccount
    // This happens for example when routing from send or receive child view to the transactions view
    if (to.params.id !== from.params.id) {
      console.log(`account: ${to.params.id}`);

      // set activity indicator to true when switching accounts
      store.dispatch("app/SET_ACTIVITY_INDICATOR", true);

      // set active account to specified id
      AccountsController.SetActiveAccount(to.params.id);

      // IMPORTANT: Do not set activity indicator to false here!
      // 1. AccountsController.SetActiveAccount tells the backend to change the account (but it isn't changed immediately).
      // 2. When the account has been changed the onActiveAccountChanged handler in unity/LibUnity.js will update the store with new account data.
      // 3. After the account in the store has been changed the onAccountChanged handler in views/Account/index.vue will set the activity indicator to false.
    }
  }

  next();
});

export default router;
