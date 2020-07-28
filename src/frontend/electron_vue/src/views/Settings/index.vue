<template>
  <novo-page-layout>
    <template v-slot:header>
      <section class="header flex-row">
        <div class="title">
          {{ $t("settings.header") }}
        </div>
      </section>
    </template>

    <router-view class="content" />

    <template v-slot:footer>
      <section class="footer">
        <portal-target ref="footerSlot" name="footer-slot" slim></portal-target>
      </section>
    </template>
  </novo-page-layout>
</template>

<style scoped lang="less">
::v-deep {
  & .header {
    & .title {
      flex: 1;
      font-size: 1.1em;
      font-weight: 500;
      line-height: 32px;
      text-align: center;
    }
  }

  & .footer {
    width: 100%;
  }
}

/*
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
*/
</style>
