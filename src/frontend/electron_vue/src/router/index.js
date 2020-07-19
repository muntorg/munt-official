import Vue from "vue";
import VueRouter from "vue-router";
import Wallet from "../views/Wallet";

Vue.use(VueRouter);

const routes = [
  {
    path: "/",
    name: "wallet",
    component: Wallet,
    meta: {
      layout: "wallet-layout"
    }
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
    path: "/send",
    name: "send",
    component: () => import(/* webpackChunkName: "send" */ "../views/Send.vue"),
    meta: {
      layout: "wallet-layout"
    }
  },
  {
    path: "/history",
    name: "history",
    component: () =>
      import(/* webpackChunkName: "send" */ "../views/History.vue"),
    meta: {
      layout: "wallet-layout"
    }
  },
  {
    path: "/settings",
    component: () =>
      import(/* webpackChunkName: "settings" */ "../views/Settings.vue"),
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
    ],
    meta: {
      layout: "wallet-layout"
    }
  },
  {
    path: "/debug",
    name: "debug",
    component: () =>
      import(/* webpackChunkName: "debug" */ "../views/Debug.vue"),
    meta: {
      isDialog: true
    }
  }
];

const router = new VueRouter({
  routes
});

export default router;
