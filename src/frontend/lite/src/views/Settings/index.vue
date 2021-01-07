<template>
  <div>
    <portal to="header-slot">
      <main-header :title="$t('settings.header')" />
    </portal>
    <router-view />
  </div>
</template>
