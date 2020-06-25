import Vue from "vue";
import VueRouter from "vue-router";
import Startup from "../views/Startup.vue";
import Wallet from "../views/Wallet.vue";

Vue.use(VueRouter);

const routes = [
  {
    path: "/",
    name: "startup",
    component: Startup
  },
  {
    path: "/setup",
    name: "setup",
    component: () =>
      import(/* webpackChunkName: "setup" */ "../views/Setup.vue")
  },
  {
    path: "/wallet",
    name: "wallet",
    component: Wallet
  },
  {
    path: "/send",
    name: "send",
    component: () => import(/* webpackChunkName: "send" */ "../views/Send.vue")
  },
  {
    path: "/settings",
    name: "settings",
    component: () =>
      import(/* webpackChunkName: "settings" */ "../views/Settings.vue")
  },
  {
    path: "/settings/view-recovery-phrase",
    name: "view-recovery-phrase",
    component: () =>
      import(
        /* webpackChunkName: "view-recovery-phrase" */ "./../views/Settings/ViewRecoveryPhrase.vue"
      )
  },
  {
    path: "/settings/change-password",
    name: "change-password",
    component: () =>
      import(
        /* webpackChunkName: "change-password" */ "./../views/Settings/ChangePassword.vue"
      )
  }
];

const router = new VueRouter({
  routes
});

export default router;
