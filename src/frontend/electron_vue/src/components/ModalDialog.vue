<template>
  <div class="modal-mask flex-col" @click="closeModal" v-if="showModal">
    <div class="modal-container" @click.stop>
      <div class="header">
        <span :class="type">{{ title }}</span>
        <div class="close" @click="closeModal">
          <span class="icon">
            <fa-icon :icon="['fal', 'times']" />
          </span>
        </div>
      </div>
      <div class="content">
        {{ message }}
        <component
          v-if="component"
          :is="component"
          v-bind="componentProps"
        ></component>
      </div>
      <app-button-section class="buttons" v-if="showButtons">
        <template v-slot:right>
          <button @click="closeModal">{{ $t("buttons.ok") }}</button>
        </template>
      </app-button-section>
    </div>
  </div>
</template>

<script>
import EventBus from "../EventBus";

export default {
  name: "ModalDialog",
  props: {
    value: {
      type: Object,
      default: null
    }
  },
  computed: {
    showModal() {
      return this.value !== null;
    },
    title() {
      return this.value.title || "title";
    },
    showButtons() {
      return typeof this.value.showButtons === "boolean"
        ? this.value.showButtons
        : true;
    },
    type() {
      return this.value.type;
    },
    message() {
      return this.value.message;
    },
    component() {
      return this.value.component;
    },
    componentProps() {
      return this.value.componentProps;
    }
  },
  methods: {
    closeModal() {
      EventBus.$emit("close-dialog");
    }
  }
};
</script>

<style lang="less" scoped>
.modal-mask {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background-color: rgba(0, 0, 0, 0.5);
  margin-top: 0;
  margin-left: 0;
  align-items: center;
  justify-content: center;
  transition: opacity 0.3s ease;
  z-index: 9998;
}

.modal-container {
  max-width: 800px;
  max-height: 700px;
  width: calc(100% - 40px);
  height: auto;
  margin: 0px auto;
  background-color: #fff;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.33);
  transition: all 0.3s ease;
}

.header {
  height: 64px;
  line-height: 64px;
  padding: 0 24px;
  border-bottom: 1px solid var(--main-border-color);
  font-size: 1.2rem;

  & .close {
    float: right;
  }

  & .icon {
    color: #606060;
    height: 15px;
    padding: 6px 12px;
  }

  & .icon:hover {
    color: var(--primary-color);
    background: var(--hover-color);
    cursor: pointer;
  }
}

.content {
  margin: 24px 0;
  padding: 0 24px;
  overflow-y: auto;
}

.buttons {
  height: 64px;
  padding: 0 12px;
}

.error {
  color: var(--error-color, #dd3333);
}
</style>
