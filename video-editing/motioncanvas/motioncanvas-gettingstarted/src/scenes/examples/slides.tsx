import { makeScene2D, Txt } from "@motion-canvas/2d";
import { beginSlide, createRef, waitFor } from "@motion-canvas/core";

export default makeScene2D(function* (view) {
    const title = createRef<Txt>();
    view.add(<Txt ref={title} />);

    title().text("FIRST SLIDE");
    yield* beginSlide("first slide");
    yield* waitFor(1); // try doing some actual animations here

    title().text("SECOND SLIDE");
    yield* beginSlide("second slide");
    yield* waitFor(1);

    title().text("LAST SLIDE");
    yield* beginSlide("last slide");
    yield* waitFor(1);
});
