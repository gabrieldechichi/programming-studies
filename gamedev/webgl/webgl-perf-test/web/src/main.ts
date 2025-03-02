async function main() {
  await window.odin.runWasm("game.wasm");
}

main()
  .then(() => console.log("done"))
  .catch((error) => console.error(error));
