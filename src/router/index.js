import Vue from "vue";
import VueRouter from "vue-router";
import Startup from "../views/Startup.vue";

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
    component: () =>
      import(/* webpackChunkName: "wallet" */ "../views/Wallet.vue")
  },
  {
    path: "/phrase",
    name: "phrase",
    component: () =>
      import(/* webpackChunkName: "phrase" */ "../views/Phrase.vue")
  },
  {
    path: "/changepassword",
    name: "changepassword",
    component: () =>
      import(/* webpackChunkName: "changepassword" */ "../views/ChangePassword.vue")
  }
];

const router = new VueRouter({
  routes
});

export default router;
