import { makeProject } from "@motion-canvas/core";
import tween from "./scenes/examples/tween?scene";
import layout1 from "./scenes/examples/layout1?scene";
import positioning from "./scenes/examples/positioning?scene";
import codeblock from "./scenes/examples/codeblock?scene";

export default makeProject({
    scenes: [tween],
});
