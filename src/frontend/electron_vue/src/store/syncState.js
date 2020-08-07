import { ipcRenderer } from "electron";

export default store => {
  if (process.type !== "renderer") return;
  // replace the renderer state by the main state
  ipcRenderer.on("vuex-connected", (event, state) => {
    console.log("vuex-connected");
    store.dispatch({ type: "REPLACE_STATE", state });
  });
  // send vuex-connect message to receive the main state
  ipcRenderer.send("vuex-connect");
};
