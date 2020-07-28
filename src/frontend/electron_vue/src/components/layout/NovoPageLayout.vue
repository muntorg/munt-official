<template>
  <section class="novo-page-layout flex-row" :class="novoPageLayoutClass">
    <section class="main flex-col">
      <section class="header" v-if="hasHeaderSlot">
        <div class="vertical-center">
          <slot name="header"></slot>
        </div>
      </section>
      <section class="content scrollable" :class="contentClass">
        <slot></slot>
      </section>
      <section class="footer" v-if="hasFooterSlot">
        <div class="vertical-center">
          <slot name="footer"></slot>
        </div>
      </section>
    </section>
    <section class="right" v-if="rightSection">
      <header class="flex-row">
        <div class="title">{{ rightSection.title }}</div>
        <div class="close" @click="closeRightSection">
          <fa-icon :icon="['fal', 'times']" />
        </div>
      </header>
      <component class="component" :is="rightSection.component" />
    </section>
  </section>
</template>

<script>
export default {
  name: "NovoPageLayout",
  props: {
    rightSection: null
  },
  computed: {
    novoPageLayoutClass() {
      return this.rightSection ? "right-section-opened" : "";
    },
    contentClass() {
      let classes = [];
      if (!this.hasHeaderSlot) classes.push("no-header");
      if (!this.hasFooterSlot) classes.push("no-footer");
      return classes;
    },
    hasHeaderSlot() {
      return !!this.$slots.header;
    },
    hasFooterSlot() {
      return !!this.$slots.footer;
    }
  },
  methods: {
    closeRightSection() {
      this.$emit("close-right-section");
    }
  }
};
</script>

<style lang="less" scoped>
.novo-page-layout {
  height: 100vh;

  & > .main {
    flex: 0 0 100%;
  }

  & > .right {
    flex: 0 0 0;
    background-color: #f5f5f5;
  }

  &.right-section-opened {
    & > .main {
      flex: 0 0 calc(100% - var(--right-section-width));

      & > .header ::v-deep section.header {
        width: calc(100vw - var(--sidebar-width) - var(--right-section-width));
      }
      & > .footer ::v-deep section.footer {
        width: calc(100vw - var(--sidebar-width) - var(--right-section-width));
      }
    }

    & > .right {
      flex: 0 0 var(--right-section-width);
    }
  }
}

.main {
  min-width: 0; // make sure main section width can be decreased to 0

  & > .header {
    flex: 0 0 var(--header-height);
    border-bottom: 1px solid var(--main-border-color);
    position: relative;
    padding: 0 24px;

    & ::v-deep .header {
      width: calc(100vw - var(--sidebar-width));
    }
  }

  & > .content {
    flex: 0 0 calc(100% - var(--header-height) - var(--footer-height));
    padding: 24px;

    &.no-header {
      flex: 0 0 calc(100% - var(--footer-height));
    }

    &.no-footer {
      flex: 0 0 calc(100% - var(--header-height));
    }

    &.no-header.no-footer {
      flex: 0 0 100%;
    }
  }

  & > .footer {
    flex: 0 0 var(--footer-height);
    border-top: 1px solid var(--main-border-color);
    position: relative;

    & ::v-deep .footer {
      width: calc(100vw - var(--sidebar-width));
    }
  }
}

.right {
  background-color: #f5f5f5;
  padding: 0 24px;

  & > header {
    line-height: 62px;
    font-size: 1.1em;
    font-weight: 500;

    & .title {
      flex: 1;
    }

    & .close {
      cursor: pointer;
    }
  }

  & .component {
    height: calc(100% - 72px);
  }
}
</style>
