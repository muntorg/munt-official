import { ipcRenderer } from "electron";
import cloneDeep from "lodash.clonedeep";

export default store => {
  if (process.type !== "renderer") return;
  // replace the renderer state by the main state
  ipcRenderer.on("vuex-connected", (event, state) => {
    console.log("vuex-connected");
    store.replaceState(cloneDeep(state));
  });
  // send vuex-connect message to receive the main state
  ipcRenderer.send("vuex-connect");
};
