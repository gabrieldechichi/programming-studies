async function main() {
  await window.odin.runWasm("/resources/game.wasm");
}

main()
  .then(() => console.log("done"))
  .catch((error) => console.error(error));
