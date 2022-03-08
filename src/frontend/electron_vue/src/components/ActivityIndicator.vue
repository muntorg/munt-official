<template>
  <div class="modal-mask flex-col">
    <div class="spinner"></div>
  </div>
</template>

<script>
export default {
  name: "ActivityIndicator"
};
</script>

<style>
.modal-mask {
  position: fixed;
  top: 0;
  left: var(--sidebar-left-width);
  width: calc(100% - var(--sidebar-left-width));
  height: 100%;
  background-color: rgba(255, 255, 255, 0.8);
  margin-top: 0;
  align-items: center;
  justify-content: center;
  z-index: 9998;
}

.spinner {
  position: relative;
  display: inline-block;
  width: 60px;
  height: 60px;
  border-style: solid;
  border-radius: 100%;
  border-width: 6px;
  border-left-color: transparent;
  color: var(--primary-color);
  opacity: 0;

  animation-name: rotate, fadeIn;
  animation-duration: 800ms, 600ms;
  animation-timing-function: linear, ease;
  animation-iteration-count: infinite, 1;
  animation-delay: 400ms;
  animation-fill-mode: forwards;
}

@keyframes rotate {
  0% {
    transform: rotate(0deg);
  }
  100% {
    transform: rotate(360deg);
  }
}

@keyframes fadeIn {
  0% {
    opacity: 0;
  }
  100% {
    opacity: 1;
  }
}

@media (max-width: 960px) {
  .modal-mask {
    left: var(--sidebar-left-width-small);
    width: calc(100% - var(--sidebar-left-width-small));
  }
}
</style>
