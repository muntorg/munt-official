<template>
  <div class="modal-mask" @click="closeModal" v-if="showModal">
    <div class="modal-container" @click.stop>
      <div class="header">
        <span>{{ title }}</span>
        <div class="close" @click="closeModal">
          <span class="icon">
            <fa-icon :icon="['fal', 'times']" />
          </span>
        </div>
      </div>
      <div class="content">
        <component :is="component"></component>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: "ModalDialog",
  props: {
    title: {
      type: String,
      default: ""
    },
    component: {
      type: String,
      default: "div"
    },
    showModal: {
      type: Boolean,
      default: false
    }
  },
  methods: {
    closeModal() {
      this.$emit("close-modal");
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
  display: flex;
  align-items: center;
  flex-direction: column;
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
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.33);
  transition: all 0.3s ease;
}

.header {
  height: 64px;
  line-height: 64px;
  padding: 0 24px;
  border-bottom: 1px solid #e1e1e1;
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
    color: #1d1c1d;
    background-color: #f6f6f6;
    border-radius: 4px;
    cursor: pointer;
  }
}

.content {
  margin: 24px 0;
  padding: 0 24px;
  height: calc(100% - 64px - 48px);
  overflow-y: auto;
}
</style>
