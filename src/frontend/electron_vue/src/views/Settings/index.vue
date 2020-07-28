<template>
  <div class="settings-view flex-col">
    <div class="header flex-row">
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
.header {
  height: var(--header-height);
  border-bottom: 1px solid var(--main-border-color);
  padding: 15px 30px 0 30px;

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
