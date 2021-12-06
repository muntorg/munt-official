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
  box-shadow: 0 2px 10px rgba(0, 0, 0, 0.2);
  transition: all 0.3s ease;
}

.header {
  height: 56px;
  line-height: 56px;
  padding: 0 30px;
  border-bottom: 1px solid var(--main-border-color);
  font-weight: 600;
  font-size: 1.05em;

  & .close {
    float: right;
    margin: 0 -10px 0 0;
  }

  & .icon {
    line-height: 42px;
    font-size: 1.2em;
    font-weight: 300;
    padding: 0 10px;
    cursor: pointer;
  }

  & .icon:hover {
    color: var(--primary-color);
    background: var(--hover-color);
  }
}

.content {
  margin: 30px 0;
  padding: 0 30px;
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
