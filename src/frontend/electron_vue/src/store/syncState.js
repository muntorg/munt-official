import { ipcRenderer } from "electron";

export default store => {
  if (process.type !== "renderer") return;
  // replace the renderer state on by the main state
  ipcRenderer.on("vuex-connected", (event, state) => {
    store.dispatch({ type: "REPLACE_STATE", state });
  });
  // send vuex-connect message to receive the main state
  ipcRenderer.send("vuex-connect");
};
