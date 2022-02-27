<template>
  <div class="modal">
  <div class="modal-mask-margin"/>
  <div class="modal-mask flex-col">
    <div class="loader-ring">
      <div></div>
      <div></div>
      <div></div>
      <div></div>
    </div>
  </div>
  </div>
</template>

<script>
export default {
  name: "ActivityIndicator"
};
</script>

<style>
.modal
{
  display: flex;
  position: fixed;
  top: 0;
  left: 0;
  margin-top: 0;
  width: 100%;
  height: 100%;
}
.modal-mask-margin {
  flex: 0 0 var(--sidebar-left-width);
  height: 100%;
}
.modal-mask {
  float: right;
  flex: 1;
  height: 100%;
  align-items: center;
  justify-content: center;
  z-index: 9998;
}
.loader-ring {
  display: inline-block;
  left: 50%;
  width: 80px;
  height: 80px;
}
.loader-ring div {
  box-sizing: border-box;
  display: block;
  position: absolute;
  width: 64px;
  height: 64px;
  margin: 8px;
  border: 8px solid #000;
  border-radius: 50%;
  animation: loader-ring 1.2s cubic-bezier(0.5, 0, 0.5, 1) infinite;
  border-color: #000 transparent transparent transparent;
}
.loader-ring div:nth-child(1) {
  animation-delay: -0.45s;
}
.loader-ring div:nth-child(2) {
  animation-delay: -0.3s;
}
.loader-ring div:nth-child(3) {
  animation-delay: -0.15s;
}
@keyframes loader-ring {
  0% {
    transform: rotate(0deg);
  }
  100% {
    transform: rotate(360deg);
  }
}
</style>
