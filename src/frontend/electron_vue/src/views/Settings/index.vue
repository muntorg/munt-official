<template>
  <div class="settings-view">
    <div class="header">
      <div class="back">
        <span>
          <fa-icon :icon="['fal', 'arrow-left']" />
          {{ $t("buttons.back") }}
        </span>
      </div>
      <div class="title">
        {{ $t("settings.header") }}
      </div>
    </div>
    <router-view class="content" />
  </div>
</template>

<style scoped lang="less">
.settings-view {
  display: flex;
  flex-direction: column;
}

.header {
  height: var(--header-height);
  border-bottom: 1px solid var(--main-border-color);
  padding: 15px 30px 0 30px;

  display: flex;
  flex-direction: row;

  & .back {
    font-weight: 300;
  }

  & .back span {
    padding: 10px;
    line-height: 32px;
    cursor: pointer;

    &:hover {
      background-color: #f5f5f5;
    }
  }

  & .title {
    flex: 1;
    font-size: 1.1em;
    font-weight: 500;
    line-height: 32px;
    text-align: center;
  }
}

.content {
  flex: 1;
  padding: 40px;
}
</style>
