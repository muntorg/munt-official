import Vue from "vue";
import VueRouter from "vue-router";
import Account from "../views/Account";

import UnityBackend from "../unity/UnityBackend";

Vue.use(VueRouter);

const routes = [
  {
    path: "/account/:id?",
    name: "account",
    props: true,
    component: Account
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
  switch (to.name) {
    case "account":
      if (to.params.id !== undefined) {
        // set active account to specified id
        UnityBackend.SetActiveAccount(to.params.id);
      }
      break;
  }
  next();
});

export default router;
