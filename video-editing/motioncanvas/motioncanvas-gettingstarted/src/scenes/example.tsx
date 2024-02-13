import { Circle, makeScene2D, View2D } from "@motion-canvas/2d";
import {
    all,
    createRef,
    makeRef,
    ThreadGenerator,
    waitFor,
} from "@motion-canvas/core";

export default makeScene2D(function* (view) {
    // Create your animations here
    const myCircle = createRef<Circle>();

    view.add(
        <Circle
            ref={myCircle}
            x={-300}
            width={140}
            height={140}
            fill="#e13238"
        />
    );
    yield* flicker(myCircle());
});

function* sideToSide(myCircle: Circle): ThreadGenerator {
    yield* all(
        myCircle.position.x(300, 1).to(-300, 1),
        myCircle.fill("#e6a700", 1).to("#e13238", 1)
    );
}

function* flicker(circle: Circle): ThreadGenerator {
    yield* circle.fill("red", 1);
    yield* circle.fill("blue", 1);
    yield* circle.fill("red", 1);
}
